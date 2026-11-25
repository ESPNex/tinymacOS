#include "../kernel/k.h"
#include "sha256.h"
#include "disklayout.h"
#include "biosboot.h"
#include "container.h"
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
int volume_current(void){return mounted;}
static u32 offset;
static int bread(u32 l, void *p)
{
    return l < NPART ? volume_cipher_io(mounted,offset+l,1,p,0) : -1;
}
static int bwrite(u32 l, const void *p)
{
    return l < NPART ? volume_cipher_io(mounted,offset+l,1,(void*)p,1) : -1;
}
int volume_mount(Block *b)
{
    if(boot.recovery)return 0; /* never mount/write the installed home in Recovery */
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
            offset = map.data;
        }
        mounted = d;
        if(offset)volume_vault(d);
        *b = (Block) {
            bread, bwrite, NPART
        };
        return 1;
    }
    return 0;
}
static InstallInfo st;
static u32 pos;
static DiskMap inventory;
static int coexist;
static u32 install_start,install_end,install_esp,install_data,install_rec,install_source,install_duet;
static NativeContainer descriptor;
static Part oldparts[128];
static Gpt oldprimary,oldbackup;
static u8 oldmbr[512];

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
        Medium medium={sata_io,sata_flush,i,q->sectors};
        partition_scan(&medium,&inventory);q->scheme=inventory.scheme;q->start=inventory.start;q->available=inventory.sectors;
        strset(q->reason,inventory.reason,sizeof q->reason);q->formats[0]=0;
        int seen[FS_COUNT]={0};for(int j=0;j<inventory.count;j++){int f=inventory.part[j].fs;if(f>=0&&f<FS_COUNT&&!seen[f]){if(q->formats[0])append(q->formats,", ",sizeof q->formats);append(q->formats,filesystem_name(f),sizeof q->formats);seen[f]=1;}}
        q->usable=st.live&&i!=mounted&&inventory.safe&&inventory.slot>=0&&
            (inventory.scheme==MAP_EMPTY?q->sectors>=196608u:inventory.sectors>=TC_SECTORS);
        if(q->usable&&inventory.scheme!=MAP_EMPTY){
            static Layout existing;
            if(layout_read(&medium,&existing)){
                q->usable=0;strset(q->reason,"tinymacOS già presente: usa Recupero",sizeof q->reason);
            }
        }

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
    Medium medium={sata_io,sata_flush,d,sata_size(d)};
    if(!partition_scan(&medium,&inventory)||!inventory.safe)return err("Tabella cambiata: nessuna modifica.");
    coexist=inventory.scheme!=MAP_EMPTY;
    install_start=coexist?inventory.start:0;install_end=coexist?install_start+TC_SECTORS:medium.z;
    install_esp=coexist?install_start:LESP;install_data=coexist?install_start+TC_DATA:LDATA;
    install_rec=coexist?install_start+TC_REC:REC_AT(medium.z);install_source=coexist?install_start+TC_SOURCE:RESTORE_AT(medium.z);
    install_duet=coexist?install_start+TC_DUET:TMC_DUET_LBA;
    if(sata_io(d,0,1,oldmbr,0))return err("Lettura MBR non riuscita.");
    if(coexist&&inventory.scheme==MAP_GPT){
        if(sata_io(d,1,1,&oldprimary,0)||sata_io(d,medium.z-1,1,&oldbackup,0)||sata_io(d,oldprimary.entries,(oldprimary.num*128+511)/512,oldparts,0))return err("Lettura GPT non riuscita.");
    }
    st.total=install_end-install_start+2*(2*NESP+NREC+NDATA);
    note(INSTALL_SCAN,coexist?"Verifica dello spazio non allocato":"Controllo del disco vuoto");
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
static int unchanged(int d)
{
    if(sata_io(d,0,1,io,0)||memcmp(io,oldmbr,512))return 0;
    if(inventory.scheme==MAP_GPT){
        if(sata_io(d,1,1,io,0)||memcmp(io,&oldprimary,512)||sata_io(d,sata_size(d)-1,1,io,0)||memcmp(io,&oldbackup,512)||
           sata_io(d,oldprimary.entries,(oldprimary.num*128+511)/512,io,0)||memcmp(io,oldparts,oldprimary.num*128))return 0;
        if(sata_io(d,oldbackup.entries,(oldbackup.num*128+511)/512,io,0)||memcmp(io,oldparts,oldbackup.num*128))return 0;
    }
    return 1;
}
static int coexist_plan(void)
{
    if(!unchanged(st.target))return -1;
    descriptor=(NativeContainer){.version=1,.sectors=TC_SECTORS,.data=TC_DATA,.rec=TC_REC,.source=TC_SOURCE,.duet=TC_DUET};
    memcpy(descriptor.magic,TC_MAGIC,8);guid(descriptor.id,31);guid(descriptor.esp_id,32);
    descriptor.crc=crc32(&descriptor,sizeof descriptor);
    /* Reserved FAT sectors 9/10 are outside its allocation tables and files. */
    u8*esp=(u8*)boot.install_esp;
    memcpy(esp+TC_HEADER*512,&descriptor,512);memcpy(esp+TC_OLD_MBR*512,oldmbr,512);
    memcpy(mbr,oldmbr,512);memcpy(mbr,bios_coexist_mbr,440);
    for(int i=0;i<=432;i++){
        u64 v;memcpy(&v,mbr+i,8);
        if(v==0x1badb002aabbccddull){v=install_duet;memcpy(mbr+i,&v,8);}
        if(v==0x1badb003aabbccddull){v=install_start+TC_OLD_MBR;memcpy(mbr+i,&v,8);}
    }
    if(inventory.scheme==MAP_GPT){
        primary=oldprimary;backup=oldbackup;memcpy(parts,oldparts,sizeof parts);
        Part*p=parts+inventory.slot;memset(p,0,sizeof *p);memcpy(p->type,esp_type,16);memcpy(p->id,descriptor.esp_id,16);
        p->start=install_start;p->end=install_end-1;const char*n="tinymacOS";for(int j=0;n[j];j++)p->name[j]=n[j];
        primary.ecrc=backup.ecrc=crc32(parts,primary.num*128);primary.crc=backup.crc=0;
        primary.crc=crc32(&primary,primary.size);backup.crc=crc32(&backup,backup.size);
    }else{
        u8*p=mbr+446+inventory.slot*16;memset(p,0,16);p[1]=p[5]=254;p[2]=p[3]=p[6]=p[7]=255;p[4]=0xef;
        memcpy(p+8,&install_start,4);u32 n=TC_SECTORS;memcpy(p+12,&n,4);
    }
    return 0;
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
    memcpy(mbr,bios_boot_mbr,440);
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
static int install_bios(int d)
{
    u64 bytes=bios_duet_end-bios_duet;
    if(!bytes || bytes%512 || (coexist?bytes/512>install_end-install_duet:bytes/512>LESP-TMC_DUET_LBA))return -1;
    for(u32 j=0;j<bytes/512;){
        u32 n=MIN(CHUNK,(u32)(bytes/512)-j);
        if(commit(d,install_duet+j,n,(void *)(bios_duet+(u64)j*512)))return -1;
        j+=n;
    }
    return 0;
}
int install_step_i()
{
    if (!isbusy())
        return -1;
    int d = st.target;
    u32 z = sata_size(d);
    for (int j = 0; j < (st.state == INSTALL_SCAN && z > 2097152 ? 32 : 8) && isbusy(); j++) {
        u32 span = 2 * NESP + NREC, limit = st.state == INSTALL_SCAN ? install_end-install_start : st.state == INSTALL_ESP ? span : st.state ==
                                            INSTALL_DATA ? NDATA : span + NDATA;
        u32 idx = st.state == INSTALL_DATA ? span + pos : pos,
            off = idx < NESP ? idx : idx < NESP + NREC ? idx - NESP : idx < span ? idx - NESP - NREC : idx - span;
        u32 n = MIN(CHUNK, limit - pos);
        if (st.state != INSTALL_SCAN)n = MIN(n,
                                                 (idx < NESP ? NESP : idx < NESP + NREC ? NREC : idx < span ? NESP : NDATA) - off);
        if (st.state == INSTALL_SCAN) {
            if (sata_io(d, install_start+pos, n, io, 0))
                return err("Errore di lettura: nessuna scrittura.");
            if (!coexist && !iszero(io, n * 512))
                return err("Il disco contiene dati: installazione annullata senza scrivere.");
        } else {
            u8 *src = (idx < NESP ? (u8 *)boot.install_esp : idx < NESP + NREC ? (u8 *)boot.recovery_img : idx < span ?
                       (u8 *)boot.install_esp : snapshot) + (u64)off * 512;
            u32 l = (idx < NESP ? install_esp : idx < NESP + NREC ? install_rec : idx < span ? install_source + 1 : install_data) + off;
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
            if (install_rec - 1 < install_data + BACKUP_OFF + NDATA)
                return err("Disco troppo piccolo per il layout: servono almeno 96 MiB.");
            if (fs_snapshot(snapshot, sizeof snapshot))
                return err("Copia del volume utente non riuscita.");
            if(coexist){if(coexist_plan())return err("Tabella cambiata: nessuna scrittura eseguita.");}
            else layout(z);
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
            memcpy(seal.disk, coexist?descriptor.id:primary.guid, 16);
            sha_bytes((void *)boot.install_esp, boot.install_len, seal.digest);
            if(coexist&&!unchanged(d))return err("Tabella cambiata: attivazione annullata.");
            if(install_bios(d)||commit(d,install_source,1,&seal))return err("Payload di avvio non verificato.");
            if(coexist){
                if(inventory.scheme==MAP_GPT && (commit(d,backup.entries,(backup.num*128+511)/512,parts)||commit(d,backup.at,1,&backup)||commit(d,primary.entries,(primary.num*128+511)/512,parts)||commit(d,primary.at,1,&primary)))return err("Aggiornamento GPT interrotto: usa la copia GPT valida.");
                if(commit(d,0,1,mbr))return err("Aggiornamento avvio fallito: MBR originale conservato nel volume.");
            }else if(commit(d,z-33,32,parts)||commit(d,z-1,1,&backup)||commit(d,2,32,parts)||commit(d,1,1,&primary)||commit(d,0,1,mbr))return err("Finalizzazione GPT non riuscita. Non usare questo disco.");
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
        Medium m = {volume_cipher_io, sata_flush, d, sata_size(d),sata_io};
        r->disk[d].sectors = m.z;
        strset(r->disk[d].model, sata_model(d), 44);
        Medium raw={sata_io,sata_flush,d,m.z};partition_scan(&raw,&r->disk[d].map);
        int t = layout_read(&m, &layout_info);
        r->disk[d].layout = t;
        r->disk[d].healthy = t && layout_info.pv && layout_info.bv;
        r->disk[d].backup = t == 2 && has_copy(&m);
    }
}
int disk_operation_i(int op, int d)
{
    if (isbusy() || d < 0 || d >= sata_count())return -1;
    Medium m = {volume_cipher_io, sata_flush, d, sata_size(d),sata_io};
    if(boot.recovery && op>=5 && op<=9){
        if(layout_read(&m,&layout_info)!=2)return -1;
        if(op==5)return restore_copy(&m);
        if(op==6)return restore_os(&m);
        if(op==7)return reset_data(&m);
        if(op==9)return firstaid(&m,0);
        int ok=1;
        for(u32 i=0;i<NDATA;i+=CHUNK)if(volume_cipher_io(d,layout_info.data+i,MIN(CHUNK,NDATA-i),snapshot+512*i,0)){ok=0;break;}
        int r=ok?save_copy(&m,snapshot):-1;erase(snapshot,sizeof snapshot);return r;
    }
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
        if (commit(d, layout_info.esp + 8, 1, flag))return -1;
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

/* Only update the preallocated one-sector settings file in this boot disk's
 * Recovery FAT16. No arbitrary disk/path writes and no filesystem metadata edits.
 * The immutable/sealed system ESP and recovery DMG are never changed here. */
#include "bootconfig.h"
static Spin cfg_lock;
static Layout cfg_layout;
static u8 cfg_sector[512] ALIGNED(512);
static unsigned cfg16(const u8 *p){return p[0]|(unsigned)p[1]<<8;}
static u32 cfg32(const u8 *p){return cfg16(p)|(u32)cfg16(p+2)<<16;}
int bootconfig_disk(int write,u32 flags)
{
    if(boot.install_len || (write==1?flags>1:!bootconfig_flags_valid(flags)) || (write==3&&(!boot.recovery||(flags!=TMC_BOOT_CSR_OVERRIDE&&flags!=TMC_BOOT_CSR_MASK))))return -2;
    u32 irq;spin_lock(&cfg_lock,&irq);int result=-1;
    for(int d=0;d<sata_count();d++) {
        Medium medium={sata_io,sata_flush,d,sata_size(d)};
        if(layout_read(&medium,&cfg_layout)!=2 || (!boot.recovery&&memcmp(cfg_layout.parts[0].id,boot.esp_id,16)))continue;
        u64 base64=cfg_layout.parts[2].start;
        if(base64>0xffffffffu || cfg_layout.parts[2].end-base64+1!=NREC)break;
        u32 base=(u32)base64;
        if(sata_io(d,base,1,cfg_sector,0))break;
        if(cfg16(cfg_sector+510)!=0xaa55||cfg16(cfg_sector+11)!=512||cfg_sector[13]!=1||cfg_sector[16]!=2)break;
        unsigned reserved=cfg16(cfg_sector+14),fat=cfg16(cfg_sector+22),entries=cfg16(cfg_sector+17);
        unsigned total=cfg16(cfg_sector+19);if(!total)total=cfg32(cfg_sector+32);
        unsigned root=reserved+2*fat,rootsectors=(entries*32+511)/512,data=root+rootsectors;
        if(!reserved||!fat||!entries||entries>512||total!=NREC||data>=total)break;
        u32 at=0;
        for(unsigned sector=0;sector<rootsectors&&!at;sector++) {
            if(sata_io(d,base+root+sector,1,cfg_sector,0))break;
            for(unsigned j=0;j<512;j+=32) {
                const u8 *p=cfg_sector+j;
                if(memcmp(p,"BOOTCFG BIN",11)||(p[11]&0x18)||cfg32(p+28)!=512)continue;
                unsigned cluster=cfg16(p+26);
                if(cluster>=2 && cluster-2<total-data)at=base+data+cluster-2;
            }
        }
        if(!at||sata_io(d,at,1,cfg_sector,0)||!bootconfig_valid((BootConfig *)cfg_sector))break;
        result=((BootConfig *)cfg_sector)->flags;
        if(write) {
            u32 old=((BootConfig *)cfg_sector)->flags;
            u32 next=write==3?((old&~TMC_BOOT_CSR_MASK)|(flags&TMC_BOOT_CSR_MASK)):write==2?((old&~TMC_BOOT_DISPLAY_MASK)|(flags&TMC_BOOT_DISPLAY_MASK)):((old&~TMC_BOOT_VERBOSE)|flags);
            BootConfig want;bootconfig_make(&want,next);
            if(sata_io(d,at,1,&want,1)||sata_flush(d)||sata_io(d,at,1,cfg_sector,0)||memcmp(&want,cfg_sector,512))result=-1;
            else result=next;
        }
        break;
    }
    spin_unlock(&cfg_lock,irq);return result;
}

/* Offline edits share the same recursive gate as the installer, filesystem,
 * snapshots and FileVault. The mounted disk is rejected wholesale, including
 * its EFI/Recovery and extended partitions; reboot into RAM Recovery to edit.
 * Raw IO is intentional: never run partition metadata through FileVault. */
int disk_edit(DiskEdit *q,int apply)
{
    u32 fl;rec_lock(&fs_gate,&fl);int r;
    if(isbusy()||q->disk<0||q->disk>=sata_count()||q->disk==mounted){
        r=q->result=DE_PROTECTED;
        strset(q->message,"Disco in uso: avvia da Recupero o dal supporto live.",sizeof q->message);
    }else{
        Medium m={sata_io,sata_flush,q->disk,sata_size(q->disk)};
        if(apply)volume_forget(q->disk);
        r=partition_edit(&m,q,apply);
    }
    rec_unlock(&fs_gate,fl);return r;
}
