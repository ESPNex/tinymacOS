#include "k.h"
#include "../include/kext.h"
/* Kernel extension manager: bundles under /System/Library/Extensions carry a
 * binary Info.plist and an ELF64 relocatable (ET_REL) executable. The loader
 * lays the allocatable sections out, applies RELA relocations, resolves the
 * handful of libc-style undefined symbols against the kernel, then calls
 * kext_start(&kapi). Optional exports (kext_poll, kext_wifi_*) are bound by
 * name when present. */

#define MAXKEXT KEXT_MAX
static struct LoadedKext kexts[KEXT_MAX];
static int nkext;
struct kext_api kapi;            /* filled in kext_init() */

/* ---- minimal binary plist reader (CFBundleIdentifier / Version / Name) ---- */
typedef struct {
    const u8 *p;
    u64 nobj;
    u32 offsz, refsZ;
    u64 table;
    const u8 *base;
} Bp;
static u64 bnum(const u8 *p, u32 n)
{
    u64 v = 0;
    for (u32 i = 0; i < n; i++) v = (v << 8) | p[i];
    return v;
}
static int bp_init(Bp *b, const u8 *d, u64 n)
{
    if (n < 40 || memcmp(d, "bplist00", 8)) return -1;
    const u8 *t = d + n - 32;
    b->offsz = t[6];
    b->refsZ = t[7];
    b->nobj = bnum(t + 8, 8);
    b->table = bnum(t + 24, 8);
    b->base = d;
    b->p = d;
    if (!b->offsz || b->offsz > 8 || !b->refsZ || b->refsZ > 8 || b->table + b->nobj * b->offsz > n) return -1;
    return 0;
}
static const u8 *bp_obj(Bp *b, u64 idx)
{
    if (idx >= b->nobj) return 0;
    u64 off = bnum(b->base + b->table + idx * b->offsz, b->offsz);
    return b->base + off;
}
/* Read a scalar object: returns 0 and fills *s (NUL-terminated, capped). */
static int bp_str(Bp *b, u64 idx, char *s, int cap)
{
    const u8 *o = bp_obj(b, idx);
    if (!o) return -1;
    u8 m = o[0];
    if ((m & 0xF0) == 0x50 || (m & 0xF0) == 0x60) {
        u64 len = m & 0x0F, at = 1;
        if (len == 0x0F) {
            u8 e = o[1];
            if ((e & 0xF0) != 0x10) return -1;
            u32 nb = 1u << (e & 0x0F);
            len = bnum(o + 2, nb);
            at = 2 + nb;
        }
        if ((m & 0xF0) == 0x60) return -1;      /* UTF-16 keys not needed */
        if (len >= (u64)cap) len = cap - 1;
        memcpy(s, o + at, len);
        s[len] = 0;
        return 0;
    }
    if ((m & 0xF0) == 0x10) {                   /* int */
        u32 nb = 1u << (m & 0x0F);
        char z[24];
        u64 v = bnum(o + 1, nb > 8 ? 8 : nb);
        number(z, v);
        int i = 0;
        for (; z[i]; i++) s[i] = z[i];
        s[i] = 0;
        return 0;
    }
    if (m == 0x08 || m == 0x09) {               /* bool */
        s[0] = m == 0x09 ? '1' : '0';
        s[1] = 0;
        return 0;
    }
    return -1;
}
/* Walk a top-level dict: find string value for key. */
static int bp_dict_get(Bp *b, const u8 *dict, const char *key, char *out, int cap)
{
    u8 m = dict[0];
    if ((m & 0xF0) != 0xD0) return -1;
    u64 n = m & 0x0F, at = 1;
    if (n == 0x0F) {
        u8 e = dict[1];
        if ((e & 0xF0) != 0x10) return -1;
        u32 nb = 1u << (e & 0x0F);
        n = bnum(dict + 2, nb);
        at = 2 + nb;
    }
    u64 refsz = b->refsZ;
    const u8 *keys = dict + at;
    const u8 *vals = keys + n * refsz;
    for (u64 i = 0; i < n; i++) {
        u64 ki = bnum(keys + i * refsz, refsz);
        char kk[64];
        if (bp_str(b, ki, kk, sizeof kk) == 0 && eq(kk, key)) {
            u64 vi = bnum(vals + i * refsz, refsz);
            return bp_str(b, vi, out, cap);
        }
    }
    return -1;
}

/* ---- ELF64 ET_REL loader ---- */
typedef struct {
    u32 name, type;
    u64 flags, addr, off, size;
    u32 link, info;
    u64 addralign, entsize;
} Shdr;
typedef struct {
    u32 name;
    u8 info, other;
    u16 shndx;
    u64 value, size;
} Sym;
typedef struct {
    u64 off, info;
    i64 addend;
} Rela;

static void *kern_sym(const char *n)
{
    if (eq(n, "memcpy")) return memcpy;
    if (eq(n, "memset")) return memset;
    if (eq(n, "memmove")) return memmove;
    if (eq(n, "memcmp")) return memcmp;
    if (eq(n, "strlen")) return strlen;
    if (eq(n, "eq")) return eq;
    if (eq(n, "number")) return number;
    if (eq(n, "append")) return append;
    if (eq(n, "strset")) return strset;
    if (eq(n, "val")) return val;
    return 0;
}

static int elf_load(const u8 *img, u64 len, struct LoadedKext *K, char *err, int errcap)
{
    if (len < 64 || memcmp(img, "\177ELF", 4) || img[4] != 2) {
        strset(err, "formato ELF64 non valido", errcap);
        return -1;
    }
    u64 e_shoff = 0;
    memcpy(&e_shoff, img + 40, 8);
    u16 e_shentsize = 0, e_shnum = 0, e_shstrndx = 0;
    memcpy(&e_shentsize, img + 58, 2);
    memcpy(&e_shnum, img + 60, 2);
    memcpy(&e_shstrndx, img + 62, 2);
    u16 e_type = 0, e_machine = 0;
    memcpy(&e_type, img + 16, 2);
    memcpy(&e_machine, img + 18, 2);
    if (e_type != 1 || e_machine != 62 || e_shentsize != 64 || !e_shnum || e_shoff + (u64)e_shnum * 64 > len) {
        strset(err, "kext: serve un ET_REL x86-64", errcap);
        return -1;
    }
    const Shdr *sh = (const void *)(img + e_shoff);
    /* lay out SHF_ALLOC sections */
    u64 cursor = 0;
    u64 *imgoff = kmalloc(e_shnum * 8);
    if (!imgoff) {
        strset(err, "kext: memoria", errcap);
        return -1;
    }
    for (int i = 0; i < e_shnum; i++) {
        imgoff[i] = ~(u64)0;
        if (!(sh[i].flags & 2) || !sh[i].size) continue;
        u64 al = sh[i].addralign ? sh[i].addralign : 1;
        if (al > 16) al = 16;
        cursor = (cursor + al - 1) & ~(al - 1);
        imgoff[i] = cursor;
        cursor += sh[i].size;
    }
    if (!cursor || cursor > 2u * 1024 * 1024) {
        kfree(imgoff);
        strset(err, "kext: dimensione immagine", errcap);
        return -1;
    }
    void *raw = kmalloc(cursor + 32);
    if (!raw) {
        kfree(imgoff);
        strset(err, "kext: memoria", errcap);
        return -1;
    }
    u8 *base = (u8 *)(((u64)raw + 15) & ~(u64)15);
    memset(base, 0, cursor);
    for (int i = 0; i < e_shnum; i++) {
        if (imgoff[i] == ~(u64)0 || sh[i].type == 8 /*NOBITS*/) continue;
        if (sh[i].off + sh[i].size > len) continue;
        memcpy(base + imgoff[i], img + sh[i].off, sh[i].size);
    }
    kexec_x((u64)base, cursor + 32);   /* heap is NX: grant execute to the image */
    /* symbol table */
    const Sym *sym = 0;
    const char *strtab = 0;
    u32 nsym = 0;
    for (int i = 0; i < e_shnum; i++)
        if (sh[i].type == 2 /*SYMTAB*/) {
            sym = (const void *)(img + sh[i].off);
            nsym = sh[i].entsize ? (u32)(sh[i].size / sh[i].entsize) : 0;
            if (sh[i].link < e_shnum) strtab = (const char *)(img + sh[sh[i].link].off);
        }
    if (!sym || !strtab) {
        kfree(imgoff);
        kfree(raw);
        strset(err, "kext: simboli mancanti", errcap);
        return -1;
    }
    /* relocations */
    for (int i = 0; i < e_shnum; i++) {
        if (sh[i].type != 4 /*RELA*/) continue;
        u32 tgt = sh[i].info;
        if (tgt >= e_shnum || imgoff[tgt] == ~(u64)0) continue;
        const Rela *r = (const void *)(img + sh[i].off);
        u32 nr = sh[i].entsize ? (u32)(sh[i].size / sh[i].entsize) : 0;
        for (u32 j = 0; j < nr; j++) {
            u32 si = (u32)(r[j].info >> 32);
            u32 type = (u32)r[j].info;
            if (!si || si >= nsym) continue;
            u64 S;
            if (sym[si].shndx == 0 /*UNDEF*/) {
                const char *nm = strtab + sym[si].name;
                void *ks = kern_sym(nm);
                if (!ks) {
                    kfree(imgoff);
                    kfree(raw);
                    strset(err, "kext: simbolo kernel mancante: ", errcap);
                    int L = 0;
                    while (err[L]) L++;
                    for (int c = 0; nm[c] && L < errcap - 1; c++) err[L++] = nm[c];
                    err[L] = 0;
                    return -1;
                }
                S = (u64)ks;
            } else {
                u32 sec = sym[si].shndx;
                if (sec >= e_shnum || imgoff[sec] == ~(u64)0) continue;
                S = (u64)(base + imgoff[sec] + sym[si].value);
            }
            u8 *loc = base + imgoff[tgt] + r[j].off;
            u64 P = (u64)loc;
            i64 A = r[j].addend;
            if (type == 1 /*R_X86_64_64*/) {
                u64 v = S + (u64)A;
                memcpy(loc, &v, 8);
            } else if (type == 2 || type == 4 /*PC32/PLT32*/) {
                i64 v = (i64)(S + (u64)A) - (i64)P;
                if (v < -2147483648LL || v > 2147483647LL) {
                    kfree(imgoff);
                    kfree(raw);
                    strset(err, "kext: rilocalizzazione fuori intervallo", errcap);
                    return -1;
                }
                u32 v32 = (u32)v;
                memcpy(loc, &v32, 4);
            } else if (type == 10 /*R_X86_64_32*/) {
                u32 v32 = (u32)(S + (u64)A);
                memcpy(loc, &v32, 4);
            } else if (type == 11 /*R_X86_64_32S*/) {
                i64 v = (i64)(S + (u64)A);
                if (v < -2147483648LL || v > 2147483647LL) {
                    kfree(imgoff);
                    kfree(raw);
                    strset(err, "kext: rilocalizzazione fuori intervallo", errcap);
                    return -1;
                }
                u32 v32 = (u32)v;
                memcpy(loc, &v32, 4);
            } else {
                kfree(imgoff);
                kfree(raw);
                strset(err, "kext: tipo rilocalizzazione non supportato", errcap);
                return -1;
            }
        }
    }
    /* bind entry points */
    K->start = 0;
    K->stop = 0;
    K->poll = 0;
    K->wifi_scan = 0;
    K->wifi_status = 0;
    K->wifi_connect = 0;
    for (u32 i = 0; i < nsym; i++) {
        if (!sym[i].shndx || sym[i].shndx >= e_shnum || imgoff[sym[i].shndx] == ~(u64)0) continue;
        const char *nm = strtab + sym[i].name;
        void *addr = base + imgoff[sym[i].shndx] + sym[i].value;
        if (eq(nm, "kext_start")) K->start = addr;
        else if (eq(nm, "kext_stop")) K->stop = addr;
        else if (eq(nm, "kext_poll")) K->poll = addr;
        else if (eq(nm, "kext_wifi_scan")) K->wifi_scan = addr;
        else if (eq(nm, "kext_wifi_status")) K->wifi_status = addr;
        else if (eq(nm, "kext_wifi_connect")) K->wifi_connect = addr;
    }
    kfree(imgoff);
    if (!K->start) {
        kfree(raw);
        strset(err, "kext: kext_start assente", errcap);
        return -1;
    }
    K->image = base;
    K->raw = raw;
    K->size = (u32)cursor;
    return 0;
}

/* Read a whole file into a kmalloc buffer via a throwaway process. */
static u8 *readall(const char *path, u64 *out_len)
{
    Process tmp;
    memset(&tmp, 0, sizeof tmp);
    int fd = fs_open(&tmp, path, 0);
    if (fd < 0) return 0;
    Dirent d;
    if (fs_stat(path, &d) || d.dir || d.size <= 0 || d.size > 1024 * 1024) {
        fs_close(&tmp, fd);
        return 0;
    }
    u8 *buf = kmalloc(d.size);
    if (!buf) {
        fs_close(&tmp, fd);
        return 0;
    }
    int n = fs_read(&tmp, fd, buf, d.size);
    fs_close(&tmp, fd);
    if (n < 0) {
        kfree(buf);
        return 0;
    }
    *out_len = n;
    return buf;
}

static int load_bundle(const char *dir)
{
    if (nkext >= MAXKEXT) return -1;
    char p[160], err[128];
    char exec[64] = "MacOS";
    strset(p, dir, sizeof p);
    append(p, "/Contents/Info.plist", sizeof p);
    u64 n = 0;
    u8 *pl = readall(p, &n);
    if (!pl) {
        logstr("[kext] ");
        logstr(dir);
        logstr(": Info.plist non leggibile\n");
        return -1;
    }
    Bp b;
    struct LoadedKext *K = kexts + nkext;
    memset(K, 0, sizeof *K);
    int bad = bp_init(&b, pl, n);
    const u8 *top = 0;
    if (!bad) {
        top = bp_obj(&b, 0);
        bad = !top;
        if (!bad) bad = bp_dict_get(&b, top, "CFBundleIdentifier", K->id, sizeof K->id);
    }
    if (bad) {
        kfree(pl);
        logstr("[kext] ");
        logstr(dir);
        logstr(": Info.plist binario non valido\n");
        return -1;
    }
    if (bp_dict_get(&b, top, "CFBundleVersion", K->ver, sizeof K->ver)) strset(K->ver, "?", sizeof K->ver);
    if (bp_dict_get(&b, top, "CFBundleName", K->name, sizeof K->name)) strset(K->name, "?", sizeof K->name);
    if (bp_dict_get(&b, top, "CFBundleExecutable", exec, sizeof exec)) strset(exec, "a.out", sizeof exec);
    kfree(pl);
    /* duplicate id? */
    for (int i = 0; i < nkext; i++)
        if (eq(kexts[i].id, K->id)) return -1;
    strset(p, dir, sizeof p);
    append(p, "/Contents/MacOS/", sizeof p);
    append(p, exec, sizeof p);
    u64 ln = 0;
    u8 *img = readall(p, &ln);
    if (!img) {
        strset(err, "eseguibile mancante", sizeof err);
        logstr("[kext] ");
        logstr(dir);
        logstr(": ");
        logstr(err);
        logstr("\n");
        return -1;
    }
    int r = elf_load(img, ln, K, err, sizeof err);
    kfree(img);
    if (r) {
        logstr("[kext] ");
        logstr(dir);
        logstr(": ");
        logstr(err);
        logstr("\n");
        return -1;
    }
    K->state = 1;
    nkext++;
    K->start(&kapi);
    logstr("[kext] loaded ");
    logstr(K->id);
    logstr(" ");
    logstr(K->ver);
    logstr("\n");
    return 0;
}

int kext_load_path(const char *dir, char *err, int errcap)
{
    /* SIP: only system-owned extensions may load when SIP is on. */
    if ((boot.csr & 1) && !eq(dir, "/System/Library/Extensions") ) {
        const char *pre = "/System/Library/Extensions/";
        int okp = 1;
        for (int i = 0; pre[i]; i++)
            if (dir[i] != pre[i]) {
                okp = 0;
                break;
            }
        if (!okp) {
            strset(err, "SIP: caricare i kext solo da /System/Library/Extensions", errcap);
            return -1;
        }
    }
    for (int i = 0; i < nkext; i++)
        if (kexts[i].state && eq(kexts[i].id, dir)) {
            strset(err, "gia caricato", errcap);
            return 0;
        }
    if (load_bundle(dir)) {
        strset(err, "kext non caricato: bundle non valido o gia presente", errcap);
        return -1;
    }
    strset(err, "ok", errcap);
    return 0;
}

int kext_unload(const char *id, char *err, int errcap)
{
    for (int i = 0; i < nkext; i++) {
        if (!eq(kexts[i].id, id)) continue;
        if (kexts[i].stop) kexts[i].stop();
        kexts[i].state = 0;
        if (kexts[i].raw) kfree(kexts[i].raw);
        /* compact */
        struct LoadedKext K = kexts[i];
        memmove(kexts + i, kexts + i + 1, (nkext - i - 1) * sizeof K);
        nkext--;
        strset(err, "ok", errcap);
        return 0;
    }
    strset(err, "kext non trovato", errcap);
    return -1;
}

int kext_stat(KextInfo *out, int cap)
{
    int n = nkext < cap ? nkext : cap;
    for (int i = 0; i < n; i++) {
        strset(out[i].id, kexts[i].id, sizeof out[i].id);
        strset(out[i].ver, kexts[i].ver, sizeof out[i].ver);
        strset(out[i].name, kexts[i].name, sizeof out[i].name);
        out[i].state = kexts[i].state;
        out[i].size = kexts[i].size;
    }
    return n;
}

/* Hooks used by net / S_NET */
void kext_net_tick(void)
{
    for (int i = 0; i < nkext; i++)
        if (kexts[i].state && kexts[i].poll) kexts[i].poll();
}
struct LoadedKext *kext_find_wifi(void)
{
    for (int i = 0; i < nkext; i++)
        if (kexts[i].state && kexts[i].wifi_status) return kexts + i;
    return 0;
}

static u64 kapi_ticks(void)
{
    return ticks;
}
static void kapi_udelay(unsigned us)
{
    /* Coarse bounded spin: drivers only need short post-reset waits. */
    volatile unsigned n = us * 40 + 100;
    while (n--) __asm__ volatile("pause");
}

/* logstr is a function-like macro in retail builds (no serial channel);
 * kapi.log needs a real function pointer, so wrap it. */
static void kext_log(const char *t)
{
    logstr(t);
}

void kext_init(void)
{
    net_init();                   /* lwIP before any driver registers */
    memset(&kapi, 0, sizeof kapi);
    kapi.log = kext_log;
    kapi.alloc = kmalloc;
    kapi.free = kfree;
    kapi.ticks = kapi_ticks;
    kapi.udelay = kapi_udelay;
    kapi.pci_find = pci_find;
    kapi.pci_mmio = pci_mmio;
    kapi.pci_reg = pci_reg;
    kapi.net_register = net_register;
    kapi.net_rx = net_rx;
    kapi.net_set_link = net_set_link;
    Dirent dirs[32];
    int n = fs_list("/System/Library/Extensions", dirs, 32, 0);
    char path[160];
    for (int i = 0; i < n; i++) {
        if (!dirs[i].dir) continue;
        const char *pre = "/System/Library/Extensions/";
        int L = 0;
        while (pre[L]) {
            path[L] = pre[L];
            L++;
        }
        for (int c = 0; dirs[i].name[c] && L < 150; c++) path[L++] = dirs[i].name[c];
        path[L] = 0;
        if (L > 10 && eq(path + L - 5, ".kext")) load_bundle(path);
    }
}
