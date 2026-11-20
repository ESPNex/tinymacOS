#include "../kernel/k.h"
#include "sha256.h"
#include "disklayout.h"
static int iszero(const void *p, u32 n)
{
    const u64 *q = p;
    for (u32 i = 0; i < n / 8; i++)
        if (q[i])
            return 0;
    return 1;
}
static u8 io[CHUNK * 512] ALIGNED(4096), snapshot[NDATA * 512] ALIGNED(512), mbr[512];
static Part parts[128];
static Gpt primary, backup;
static int mounted = -1;
static u32 offset;
static int bread(u32 l, void *p)
{
    return l < NPART ? sata_io(mounted, offset + l, 1, p, 0) : -1;
}
static int bwrite(u32 l, const void *p)
{
    return l < NPART && !sata_io(mounted, offset + l, 1, (void *)p, 1) ? sata_flush(mounted) : -1;
}
int volume_mount(Block *b)
{
    for (int d = 0; d < sata_count(); d++) {
        u32 z = sata_size(d);
        if (z < NPART || sata_io(d, 0, 1, io, 0))
            continue;
#ifdef TMC_DEBUG
        logstr("[vmdbg] ilen=");
        lognum(boot.install_len);
        logstr(" io=");
        for (int i = 0; i < 8; i++) {
            u8 c = io[i];
            char t[3] = {(char)("0123456789abcdef"[c >> 4]), (char)("0123456789abcdef"[c & 15]), 0};
            logstr(t);
        }
        logstr("\n");
#endif
        if (boot.install_len && !memcmp(io, "TINYFS1\0", 8))offset = 0;
        else {
            static Layout map;
            Medium m = {sata_io, sata_flush, d, z};
            if (!layout_read(&m, &map) || (!boot.install_len && memcmp(map.parts[0].id, boot.esp_id, 16)))continue;
            offset = LDATA;
        }
        mounted = d;
        *b = (Block) {
            bread, bwrite, NPART
        };
        return 1;
    }
    return 0;
}
static InstallInfo st;
static u32 pos;
static int isbusy(void)
{
    return st.state >= INSTALL_SCAN && st.state <= INSTALL_VERIFY;
}
static void note(int s, const char *t)
{
    st.state = s;
    strset(st.message, t, sizeof st.message);
    logstr("[install] ");
    logstr(t);
    logstr("\n");
}
static int err(const char *t)
{
    note(INSTALL_ERROR, t);
    return -1;
}
static void refresh(void)
{
    st.count = sata_count();
    st.live = !!boot.install_len;
    for (int i = 0; i < st.count; i++) {
        InstallDisk *q = st.disk + i;
        q->sectors = sata_size(i);
        strset(q->model, sata_model(i), 41);
        q->usable = st.live && i != mounted && q->sectors >= MINSECT && q->sectors <= 268435456u &&
                    !sata_io(i, 0, 2, io, 0)
                    && iszero(io, 1024) && !sata_io(i, q->sectors - 1, 1, io, 0) && iszero(io, 512);
    }
}
void install_status_i(InstallInfo *q)
{
    if (!isbusy() && st.state != INSTALL_DONE)
        refresh();
    *q = st;
}
int install_begin_i(int d)
{
    if (isbusy() || st.state == INSTALL_DONE)
        return -1;
    refresh();
    if (!st.live || boot.recovery_len != NREC * 512ull || boot.install_len != NESP * 512ull || d < 0 ||
        d >= st.count || !st.disk[d].usable)
        return err("Disco non disponibile: nessuna modifica.");
    st.target = d;
    st.done = pos = 0;
    st.total = sata_size(d) + 2 * (2 * NESP + NREC + NDATA);
    note(INSTALL_SCAN, "Controllo del disco vuoto");
    return 0;
}
int install_cancel_i()
{
    if (st.state != INSTALL_SCAN && st.state != INSTALL_ERROR)
        return -1;
    memset(&st, 0, sizeof st);
    pos = 0;
    return 0;
}
static void guid(u8 *out, u8 tag)
{
    SHA256 h;
    u8 sum[32];
    sha_init(&h);
    sha_update(&h, "Orizzonte partition ID", 22);
    sha_update(&h, snapshot + 13 * 512 + 48, 16);
    sha_update(&h, &ticks, 8);
    sha_update(&h, &st.target, 4);
    sha_update(&h, &tag, 1);
    sha_final(&h, sum);
    memcpy(out, sum, 16);
    out[7] = (out[7] & 15) | 64;
    out[8] = (out[8] & 63) | 128;
    erase(sum, 32);
}
static void part(int i, const u8 *t, u32 l, u32 end, const char *n)
{
    Part *p = parts + i;
    memcpy(p->type, t, 16);
    guid(p->id, i + 1);
    p->start = l;
    p->end = end;
    for (int j = 0; n[j] && j < 35; j++)
        p->name[j] = n[j];
}
static void layout(u32 n)
{
    memset(parts, 0, sizeof parts);
    part(0, esp_type, LESP, LDATA - 1, "tinymacOS EFI");
    part(1, dat_type, LDATA, REC_AT(n) - 1, "Macintosh HD - Dati");
    part(2, esp_type, REC_AT(n), RESTORE_AT(n) - 1, "Recovery");
    part(3, src_type, RESTORE_AT(n), n - 34, "Installazione locale");
    primary = (Gpt) {
        .rev = 0x10000, .size = 92, .at = 1, .other = n - 1, .first = 34, .last = n - 34, .entries = 2, .num = 128,
        .entrysize = 128, .ecrc = crc32(parts, sizeof parts)
    };
    memcpy(primary.sig, "EFI PART", 8);
    guid(primary.guid, 0);
    backup = primary;
    backup.at = n - 1;
    backup.other = 1;
    backup.entries = n - 33;
    backup.crc = crc32(&backup, 92);
    primary.crc = crc32(&primary, 92);
    memset(mbr, 0, 512);
    mbr[450] = 0xee;
    mbr[448] = 2;
    mbr[451] = mbr[452] = mbr[453] = 255;
    *(u32 *)(mbr + 454) = 1;
    *(u32 *)(mbr + 458) = n - 1;
    mbr[510] = 85;
    mbr[511] = 170;
}
static int commit(int d, u32 l, u32 n, void *p)
{
    return sata_io(d, l, n, p, 1) || sata_flush(d) || sata_io(d, l, n, io, 0) || memcmp(io, p, n * 512);
}
int install_step_i()
{
    if (!isbusy())
        return -1;
    int d = st.target;
    u32 z = sata_size(d);
    for (int j = 0; j < (st.state == INSTALL_SCAN && z > 2097152 ? 32 : 8) && isbusy(); j++) {
        u32 span = 2 * NESP + NREC, limit = st.state == INSTALL_SCAN ? z : st.state == INSTALL_ESP ? span : st.state ==
                                            INSTALL_DATA ? NDATA : span + NDATA;
        u32 idx = st.state == INSTALL_DATA ? span + pos : pos,
            off = idx < NESP ? idx : idx < NESP + NREC ? idx - NESP : idx < span ? idx - NESP - NREC : idx - span;
        u32 n = MIN(CHUNK, limit - pos);
        if (st.state != INSTALL_SCAN)n = MIN(n,
                                                 (idx < NESP ? NESP : idx < NESP + NREC ? NREC : idx < span ? NESP : NDATA) - off);
        if (st.state == INSTALL_SCAN) {
            if (sata_io(d, pos, n, io, 0))
                return err("Errore di lettura: nessuna scrittura.");
            if (!iszero(io, n * 512))
                return err("Il disco contiene dati: installazione annullata senza scrivere.");
        } else {
            u8 *src = (idx < NESP ? (u8 *)boot.install_esp : idx < NESP + NREC ? (u8 *)boot.recovery_img : idx < span ?
                       (u8 *)boot.install_esp : snapshot) + (u64)off * 512;
            u32 l = (idx < NESP ? LESP : idx < NESP + NREC ? REC_AT(z) : idx < span ? RESTORE_AT(z) + 1 : LDATA) + off;
            if (st.state == INSTALL_VERIFY) {
                if (sata_io(d, l, n, io, 0) || memcmp(io, src, n * 512))
                    return err("Rilettura non riuscita: disco non attivato.");
            } else if (sata_io(d, l, n, src, 1))
                return err("Scrittura SATA non riuscita: disco non attivato.");
        }
        st.done += n;
        pos += n;
        if (pos != limit)
            continue;
        pos = 0;
        if (st.state == INSTALL_SCAN) {
            if (REC_AT(z) - 1 < LDATA + BACKUP_OFF + NDATA)
                return err("Disco troppo piccolo per il layout: servono almeno 96 MiB.");
            if (fs_snapshot(snapshot, sizeof snapshot))
                return err("Copia del volume utente non riuscita.");
            layout(z);
            note(INSTALL_ESP, "Installazione di tinymacOS e di Recupero");
        } else if (st.state == INSTALL_ESP)
            note(INSTALL_DATA, "Copia di account e documenti");
        else if (st.state == INSTALL_DATA) {
            if (sata_flush(d))
                return err("Impossibile sincronizzare il disco.");
            note(INSTALL_VERIFY, "Verifica dell’installazione");
        } else {
            Seal seal = {.sectors = NESP, .ver = 1};
            memcpy(seal.magic, "TMCSYS20", 8);
            memcpy(seal.disk, primary.guid, 16);
            sha_bytes((void *)boot.install_esp, boot.install_len, seal.digest);
            if (commit(d, RESTORE_AT(z), 1, &seal) || commit(d, z - 33, 32, parts) || commit(d, z - 1, 1, &backup) ||
                commit(d, 2, 32, parts) || commit(d, 1, 1, &primary)
                || commit(d, 0, 1, mbr))
                return err("Finalizzazione GPT non riuscita. Non usare questo disco.");
            erase(snapshot, sizeof snapshot);
            note(INSTALL_DONE, "Installazione completata e verificata");
        }
    }
    return 0;
}

int install_finished_i()
{
    return st.state == INSTALL_DONE;
}

int install_writing_i()
{
    return st.state >= INSTALL_ESP && st.state <= INSTALL_VERIFY;
}

static Layout layout_info;
void disk_info_i(DiskInfo *r)
{
    memset(r, 0, sizeof * r);
    r->count = sata_count();
    r->current = mounted;
    for (int d = 0; d < r->count; d++) {
        Medium m = {sata_io, sata_flush, d, sata_size(d)};
        r->disk[d].sectors = m.z;
        strset(r->disk[d].model, sata_model(d), 44);
        int t = layout_read(&m, &layout_info);
        r->disk[d].layout = t;
        r->disk[d].healthy = t && layout_info.pv && layout_info.bv;
        r->disk[d].backup = t == 2 && has_copy(&m);
    }
}
int disk_operation_i(int op, int d)
{
    if (isbusy() || d < 0 || d >= sata_count())return -1;
    Medium m = {sata_io, sata_flush, d, sata_size(d)};
    if (op == 4 || op == 1)return firstaid(&m, op == 1);
    if (d != mounted || boot.install_len || !persistent || layout_read(&m, &layout_info) != 2)return -1;
    if (op == 2) {
        if (fs_snapshot(snapshot, sizeof snapshot))return -1;
        int r = save_copy(&m, snapshot);
        erase(snapshot, sizeof snapshot);
        return r;
    }
    if (op == 3) {
        u8 flag[512] = {0};
        memcpy(flag, "TMRECOV2", 8);
        if (commit(d, LESP + 8, 1, flag))return -1;
        return power(1);
    }
    return -1;
}

// Public wrappers: one recursive filesystem gate serializes all disk state.
void install_status(InstallInfo *q)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    install_status_i(q);
    rec_unlock(&fs_gate, fl);
}

int install_begin(int d)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = install_begin_i(d);
    rec_unlock(&fs_gate, fl);
    return r;
}

int install_cancel(void)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = install_cancel_i();
    rec_unlock(&fs_gate, fl);
    return r;
}

int install_step(void)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = install_step_i();
    rec_unlock(&fs_gate, fl);
    return r;
}

int install_finished(void)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = install_finished_i();
    rec_unlock(&fs_gate, fl);
    return r;
}

int install_writing(void)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = install_writing_i();
    rec_unlock(&fs_gate, fl);
    return r;
}

void disk_info(DiskInfo *r)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    disk_info_i(r);
    rec_unlock(&fs_gate, fl);
}

int disk_operation(int op, int d)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = disk_operation_i(op, d);
    rec_unlock(&fs_gate, fl);
    return r;
}
