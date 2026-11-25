#include "prefs_plist.h"
#include "../kernel/k.h"
#include "../include/disklayout.h"
#include "../include/sha256.h"
// tinymacOS BTRFS subset (APFS-style wrapper). Layout inside the data volume:
//   0 container header | 1..12 legacy | 13..14 accounts | 15 factory |
//   64.. node table (NODE0, NSLOTS/8 sectors) |
//   128..135 superblock at 64 KiB (metadata proprietari con magic Btrfs e CRC32C) | 136..143 snapshot catalog |
//   144.. file data (16 sectors/node) | IMG0.. snapshot images | BACKUP_OFF backup area.
#define DATA 144
#define SUPER 128
#define SNAPCAT 136
#define NODE0 64
#define NSNAP 8
#define SNAPREV 2
#define SECTORS (DATA+NSLOTS*16)
#define IMG0 SECTORS
typedef struct PACK {
    u8 kind, ro;
    u16 parent;
    u32 size, gen;
    char name[48];
    u32 reserved;
} Node;
_Static_assert(sizeof(Node) == 64, "disk inode");
static Node nodes[NSLOTS];
static u8 ram[NPART][512]; // full volume incl. snapshot area (RAM volumes snapshot too)
static Block block;
u32 persistent, fsrev, usednodes, theme, wall, dock, reduce_motion, opaque, dark;
static int seeding, configid;
static int ovl_slot = -1;
static u8 nodes_live[sizeof nodes];
#define SNAPMAX 8
typedef struct PACK {
    char name[28];
    u32 date, id, base, crc, gen;
} SnapEnt; // 48 B
_Static_assert(sizeof(SnapEnt) == 48, "snap ent");
static u8 catbuf[512];
static int cat_load(void);
static SnapEnt *cat_ent(int i);
static int cat_save(void);
static int ovl_open(const char *p, const char **inner);
static void ovl_close(void);
static u32 ovl_base(void);
// FileVault-style content encryption (BTRFS-subset APFS wrapper): per-file
// flag rides Node.reserved; keystream = SHA256(key||sector-rel||block).
// System paths stay plaintext so a locked volume still boots.
static int enc_state, enc_locked;
static u8 enc_key[32], enc_salt[16], enc_ver[16];
static void enc_xform(u8 *s, u32 rel, int off, int len);
static int enc_ex(const char *p);
static int idpath_n(Node *arr, int i, char *o, int cap);
static void enc_load(const u8 *h);
static int enc_hdrsave(void);
static int enc_scrub(int up);
static int mr(u32 l, void *b)
{
    if (l >= NPART)
        return -1;
    memcpy(b, ram[l], 512);
    return 0;
}
static int mw(u32 l, const void *b)
{
    if (l >= NPART)
        return -1;
    memcpy(ram[l], b, 512);
    return 0;
}
static int syncnode(int i)
{
    return block.write(NODE0 + i / 8, &nodes[i & ~7]);
}
static int child(int par, const char *name)
{
    for (int i = 1; i < NSLOTS; i++)
        if (nodes[i].kind && nodes[i].parent == par && eq(nodes[i].name, name))
            return i;
    return -1;
}
static int resolve(const char *p)
{
    if (!p || *p != '/')
        return -1;
    int a = 0;
    while (*p) {
        while (*p == '/')
            p++;
        if (!*p)
            break;
        char name[48];
        int l = 0;
        while (*p && *p != '/') {
            if (l >= 47)
                return -1;
            name[l++] = *p++;
        }
        name[l] = 0;
        if (nodes[a].kind != 1)
            return -1;
        if (eq(name, "."))
            continue;
        if (eq(name, "..")) {
            a = nodes[a].parent;
            continue;
        }
        a = child(a, name);
        if (a < 0)
            return -1;
    }
    return a;
}
static int split(const char *p, char *name)
{
    if (!p || *p != '/')
        return -1;
    int z = strlen(p), i = z;
    while (i > 0 && p[i - 1] != '/')
        i--;
    if (z >= PATHLEN || z == i || z - i >= 48)
        return -1;
    for (int j = i; j < z; j++)
        if ((unsigned char)p[j] < 32)
            return -1;
    strset(name, p + i, 48);
    if (eq(name, "..") || eq(name, "."))
        return -1;
    char buf[PATHLEN];
    memcpy(buf, p, i);
    buf[i] = 0;
    int par = resolve(buf);
    if (par < 0 || nodes[par].kind != 1 || (!seeding && nodes[par].ro))
        return -1;
    return par;
}
static void entry(int i, Dirent *d)
{
    Node *n = nodes + i;
    *d = (Dirent) {
        i, n->parent, n->kind == 1, n->ro, n->size, {0}
    };
    strset(d->name, n->name, 48);
}
// /Volumes contains a mount alias, not another copy of the system or the data.
static const char *mountedpath(const char *p)
{
    const char *k = boot.install_len ? "/Volumes/tinymacOS Live" : "/Volumes/Macintosh HD";
    size_t n = strlen(k);
    if (strlen(p) >= n && !memcmp(p, k, n) && (!p[n] || p[n] == '/'))return p[n] ? p + n : "/";
    return p;
}
int fs_stat_i(const char *p, Dirent *d)
{
    p = mountedpath(p);
    int v = bootfs_lookup(p);
    if (v != -2)return bootfs_stat(v, d);
    if (eq(p, "/.snapshots")) {
        *d = (Dirent) {900, 0, 1, 1, 0, {0}};
        strset(d->name, ".snapshots", 48);
        return 0;
    }
    const char *inner = 0;
    if (ovl_open(p, &inner) < 0)
        return -1;
    if (ovl_slot >= 0)
        p = inner;
    int a = resolve(p);
    if (a < 0) {
        ovl_close();
        return -1;
    }
    entry(a, d);
    ovl_close();
    return 0;
}
int fs_list_i(const char *p, Dirent *d, int cap, int skip)
{
    p = mountedpath(p);
    if (skip < 0)return -1;
    int v = bootfs_lookup(p);
    if (v != -2)return bootfs_list(v, d, cap, skip);
    if (eq(p, "/.snapshots")) {
        if (cat_load() < 0)
            return -1;
        int cnt = (int)((u32 *)catbuf)[3], n = 0;
        for (int j = 0; j < cnt && j < SNAPMAX; j++) {
            SnapEnt *e = cat_ent(j);
            if (!e->name[0])
                continue;
            if (skip) {
                skip--;
                continue;
            }
            if (n >= cap)
                break;
            d[n] = (Dirent) {901 + j, 900, 1, 1, 0, {0}};
            strset(d[n].name, e->name, 48);
            n++;
        }
        return n;
    }
    const char *inner = 0;
    if (ovl_open(p, &inner) < 0)
        return -1;
    if (ovl_slot >= 0)
        p = inner;
    int a = resolve(p);
    if (a < 0 || nodes[a].kind != 1 || skip < 0) {
        ovl_close();
        return -1;
    }
    int n = 0;
    if (eq(p, "/Volumes")) {
        if (skip)skip--;
        else if (cap) {
            entry(0, d);
            d->ro = 1;
            strset(d->name, boot.install_len ? "tinymacOS Live" : "Macintosh HD", 48);
            n++;
        }
    }
    if (eq(p, "/")) {
        if (skip)skip--;
        else if (cap) {
            Dirent *e = d + n;
            e->id = 900;
            e->parent = 0;
            e->dir = 1;
            e->ro = 1;
            e->size = 0;
            strset(e->name, ".snapshots", 48);
            n++;
        }
    }
    if (!a) {
        Dirent virt[12];
        int k = bootfs_list(1024, virt, 12, 0);
        for (int i = 0; i < k && n < cap; i++) {
            if (skip)skip--;
            else d[n++] = virt[i];
        }
    }
    for (int i = 1; i < NSLOTS && n < cap; i++)if (nodes[i].kind && nodes[i].parent == a) {
            if (!a) {
                char path[64] = "/";
                append(path, nodes[i].name, 64);
                if (bootfs_lookup(path) != -2)continue;
            }
            if (skip)skip--;
            else entry(i, d + n++);
        }
    ovl_close();
    return n;
}
int fs_create_i(const char *p, int dir)
{
    p = mountedpath(p);
    if (!seeding && bootfs_lookup(p) != -2)return -1;
    char name[48];
    int par = split(p, name);
    if (par < 0 || child(par, name) >= 0)
        return -1;
    for (int i = 1; i < NSLOTS; i++)
        if (!nodes[i].kind) {
            Node old = nodes[i];
            nodes[i] = (Node) {
                .kind = dir ? 1 : 2, .parent = par, .gen = old.gen + 1
            };
            strset(nodes[i].name, name, 48);
            if (enc_state == 1 && nodes[i].kind == 2) {
                char fp[128];
                if (idpath_n(nodes, i, fp, sizeof fp) == 0 && !enc_ex(fp))
                    nodes[i].reserved = 1;
            }
            if (syncnode(i) < 0) {
                nodes[i] = old;
                return -1;
            }
            usednodes++;
            fsrev++;
            return i;
        }
    return -1;
}
int fs_remove_flags_i(const char *p, unsigned flags)
{
    if (flags & ~(REMOVE_RECURSIVE|REMOVE_DIR_ONLY|REMOVE_FILE_ONLY))return -REMOVE_BAD_PATH;
    if ((flags&REMOVE_DIR_ONLY) && (flags&(REMOVE_FILE_ONLY|REMOVE_RECURSIVE)))return -REMOVE_BAD_PATH;
    if ((flags&REMOVE_FILE_ONLY) && (flags&REMOVE_RECURSIVE))return -REMOVE_BAD_PATH;
    p = mountedpath(p);
    if (bootfs_lookup(p) != -2)return -REMOVE_READ_ONLY;
    int i = resolve(p);
    if (i <= 0)return -REMOVE_BAD_PATH; /* root and unresolved paths are never removable */
    if (nodes[i].ro || nodes[nodes[i].parent].ro)return -REMOVE_READ_ONLY;
    if (((flags&REMOVE_DIR_ONLY) && nodes[i].kind!=1) ||
        ((flags&REMOVE_FILE_ONLY) && nodes[i].kind!=2))return -REMOVE_WRONG_TYPE;
    u8 selected[NSLOTS]={0},sectors[NSLOTS/8]={0};
    u16 pending[NSLOTS];unsigned count=1;
    pending[0]=i;selected[i]=1;
    /* Discover and validate the ENTIRE subtree before the first mutation.
     * No recursion or unbounded user-controlled kernel stack consumption. */
    for (unsigned at=0;at<count;at++) {
        unsigned parent=pending[at];
        if (nodes[parent].ro)return -REMOVE_READ_ONLY;
        sectors[parent/8]=1;
        for(unsigned j=1;j<NSLOTS;j++)if(nodes[j].kind && nodes[j].parent==parent){
            if(!(flags&REMOVE_RECURSIVE))return -REMOVE_NOT_EMPTY;
            if(selected[j] || count==NSLOTS)return -REMOVE_BAD_PATH;
            selected[j]=1;pending[count++]=j;
        }
    }
    Node *saved=kmalloc(sizeof nodes);
    if(!saved)return -REMOVE_IO;
    memcpy(saved,nodes,sizeof nodes);
    for(unsigned at=0;at<count;at++)nodes[pending[at]].kind=0;
    for(unsigned sector=0;sector<NSLOTS/8;sector++)if(sectors[sector]){
        if(block.write(NODE0+sector,&nodes[sector*8])<0){
            memcpy(nodes,saved,sizeof nodes);
            /* Best-effort on-disk rollback. Persistent device failure is still
             * an I/O error, not a claimed atomic/journalled transaction. */
            for(unsigned k=0;k<NSLOTS/8;k++)if(sectors[k])block.write(NODE0+k,&nodes[k*8]);
            kfree(saved);return -REMOVE_IO;
        }
    }
    kfree(saved);usednodes-=count;fsrev++;
    return 0;
}
int fs_remove_i(const char *p){return fs_remove_flags_i(p,0);}
int fs_move_i(const char *p, const char *q)
{
    p = mountedpath(p);
    q = mountedpath(q);
    if (bootfs_lookup(p) != -2 || bootfs_lookup(q) != -2)return -1;
    int i = resolve(p);
    char name[48];
    int par = split(q, name);
    if (i <= 0 || nodes[i].ro || nodes[nodes[i].parent].ro || par < 0 || child(par, name) >= 0)
        return -1;
    for (int j = par; j; j = nodes[j].parent)
        if (j == i)
            return -1;
    Node old = nodes[i];
    nodes[i].parent = par;
    strset(nodes[i].name, name, 48);
    if (syncnode(i) < 0) {
        nodes[i] = old;
        return -1;
    }
    fsrev++;
    return 0;
}
// Full path of a node inside an arbitrary node table (live or snapshot image).
static int idpath_n(Node *arr, int i, char *o, int cap)
{
    if (i <= 0 || i >= NSLOTS || !arr[i].kind || cap < 2)
        return -1;
    const char *stk[16];
    int d = 0;
    for (int c = i; c > 0 && d < 16; c = arr[c].parent)
        if (c < 0 || c >= NSLOTS || !arr[c].kind)
            return -1;
        else
            stk[d++] = arr[c].name;
    int u = 0;
    o[u++] = '/';
    for (int j = d - 1; j >= 0; j--) {
        int l = (int)strlen(stk[j]);
        if (u + l + (j ? 1 : 0) >= cap)
            return -1;
        if (u > 1)o[u++] = '/';
        memcpy(o + u, stk[j], l);
        u += l;
    }
    o[u] = 0;
    return 0;
}
// Exempt: system/OS paths plus anything under a Library/ folder (preferences).
static int enc_ex(const char *p)
{
    static const char *pre[] = {
        "/System", "/Applications", "/Library", "/bin", "/sbin", "/usr", "/Recovery"
    };
    if (!p)
        return 1;
    for (int j = 0; j < 7; j++) {
        int l = (int)strlen(pre[j]);
        if (!memcmp(p, pre[j], l) && (p[l] == 0 || p[l] == '/'))
            return 1;
    }
    for (const char *q = p; *q; q++)
        if (q[0] == '/' && !memcmp(q, "/Library/", 9))
            return 1;
    return 0;
}
static void enc_ks(u32 rel, u32 blk, u8 out[32])
{
    u8 m[40];
    memcpy(m, enc_key, 32);
    m[32] = (u8)rel;
    m[33] = (u8)(rel >> 8);
    m[34] = (u8)(rel >> 16);
    m[35] = (u8)(rel >> 24);
    m[36] = (u8)blk;
    m[37] = (u8)(blk >> 8);
    m[38] = (u8)(blk >> 16);
    m[39] = (u8)(blk >> 24);
    sha_bytes(m, 40, out);
    erase(m, 40);
}
static void enc_xform(u8 *s, u32 rel, int off, int len)
{
    u32 cblk = ~0u;
    u8 ks[32];
    for (int j = 0; j < len; j++) {
        u32 g = (u32)(off + j), b = g >> 5;
        if (b != cblk) {
            enc_ks(rel, b, ks);
            cblk = b;
        }
        s[off + j] ^= ks[g & 31];
    }
    erase(ks, 32);
}
static int bytes(int i, int at, void *buf, int n, int wr)
{
    if (n < 0 || at < 0 || n > MAXFILE - at)
        return -1;
    int enc = enc_state && i > 0 && i < NSLOTS && nodes[i].reserved;
    if (enc && enc_locked)
        return -1;
    int done = 0;
    u8 s[512];
    while (done < n) {
        int pos = at + done, part = MIN(n - done, 512 - pos % 512);
        u32 rel = DATA + i * 16 + pos / 512;
        u32 l = rel + ovl_base();
        if (!wr || part != 512) {
            if (block.read(l, s) < 0)
                return -1;
        }
        if (wr) {
            memcpy(s + pos % 512, (u8 *)buf + done, part);
            if (enc)enc_xform(s, rel, pos % 512, part);
            if (block.write(l, s) < 0)
                return -1;
        } else {
            if (enc)enc_xform(s, rel, pos % 512, part);
            memcpy((u8 *)buf + done, s + pos % 512, part);
        }
        done += part;
    }
    return done;
}
int fs_open_i(Process *p, const char *path, int w)
{
    path = mountedpath(path);
    int v = bootfs_lookup(path);
    if (v != -2)return bootfs_open(p, v, w);
    const char *inner = 0;
    if (ovl_open(path, &inner) < 0)
        return -1;
    if (ovl_slot >= 0)
        path = inner;
    int i = resolve(path);
    int k = ovl_slot;
    if (i < 0 || nodes[i].kind != 2 || (w && (nodes[i].ro || k >= 0))) {
        ovl_close();
        return -1;
    }
    u32 gen = nodes[i].gen;
    ovl_close();
    for (int d = 0; d < 12; d++)
        if (!p->fd[d].id) {
            p->fd[d] = (FD) {
                k >= 0 ? (0x40000000 | (k << 16) | i) : i, gen, 0, !!w
            };
            return d;
        }
    return -1;
}
int fs_close_i(Process *p, int fd)
{
    if (fd < 0 || fd >= 12 || !p->fd[fd].id)
        return -1;
    p->fd[fd].id = 0;
    return 0;
}
static int valid(FD *f)
{
    int i = f->id;
    return i > 0 && i < NSLOTS && nodes[i].kind == 2 && (u32)f->gen == nodes[i].gen;
}
static int snap_prep(FD *f)
{
    // Load the snapshot image referenced by an encoded fd; -2 stale, -1 media.
    int k = (f->id >> 16) & 255, node = f->id & 255;
    if (cat_load())
        return -1;
    int c = (int)((u32 *)catbuf)[3];
    if (k >= SNAPMAX || k >= c || !cat_ent(k)->name[0])
        return -1;
    u32 base = cat_ent(k)->base;
    memcpy(nodes_live, nodes, sizeof nodes);
    for (int j = 0; j < NSLOTS / 8; j++)
        if (block.read(NODE0 + j + base, nodes + j * 8) < 0) {
            memcpy(nodes, nodes_live, sizeof nodes);
            return -1;
        }
    ovl_slot = k;
    if (node <= 0 || node >= NSLOTS || nodes[node].kind != 2 || (u32)f->gen != nodes[node].gen)
        return -2;
    return node;
}
int fs_read_i(Process *p, int d, void *b, int n)
{
    if (d < 0 || d >= 12 || n < 0)
        return -1;
    FD *f = p->fd + d;
    if (f->id & 0x40000000) {
        int i = snap_prep(f);
        if (i < 0) {
            ovl_close();
            return -1;
        }
        n = MIN(n, MAX(0, (int)nodes[i].size - f->at));
        int r = bytes(i, f->at, b, n, 0);
        if (r >= 0)
            f->at += r;
        ovl_close();
        return r;
    }
    if (f->id >= 1024)return bootfs_read(f, b, n);
    if (!valid(f))
        return -1;
    n = MIN(n, MAX(0, (int)nodes[f->id].size - f->at));
    int r = bytes(f->id, f->at, b, n, 0);
    if (r >= 0)
        f->at += r;
    return r;
}
int fs_write_i(Process *p, int d, const void *b, int n)
{
    if (d < 0 || d >= 12 || n < 0)
        return -1;
    FD *f = p->fd + d;
    if (f->id & 0x40000000)
        return -1;
    if (!valid(f) || !f->write || nodes[f->id].ro)
        return -1;
    int r = bytes(f->id, f->at, (void *)b, n, 1);
    if (r < 0)
        return -1;
    int end = f->at + n, old = nodes[f->id].size;
    nodes[f->id].size = end;
    if (syncnode(f->id) < 0) {
        nodes[f->id].size = old;
        return -1;
    }
    f->at = end;
    fsrev++;
    logstr("[fs] write committed: ");
    logstr(nodes[f->id].name);
    logstr(" bytes=");
    lognum(n);
    logstr("\n");
    return n;
}
static int seed(const char *p, int dir, int ro, const char *text)
{
    int i = fs_create(p, dir);
    if (i < 0)
        panic("filesystem seed failed", 0, 0);
    nodes[i].ro = ro;
    if (text) {
        nodes[i].size = strlen(text);
        if (bytes(i, 0, (void *)text, nodes[i].size, 1) < 0)
            panic("filesystem data error", i, 0);
    }
    if (syncnode(i) < 0)
        panic("filesystem metadata error", i, 0);
    return i;
}
static u32 c32c_tab[256];
static int c32c_ready;
static void c32c_init(void)
{
    for (u32 i = 0; i < 256; i++) {
        u32 c = i;
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (0x82F63B78u & (0u - (c & 1)));
        c32c_tab[i] = c;
    }
    c32c_ready = 1;
}
static u32 crc32c_cont(u32 c, const void *p, u32 n)
{
    if (!c32c_ready)
        c32c_init();
    const u8 *b = p;
    for (u32 i = 0; i < n; i++)
        c = c32c_tab[(c ^ b[i]) & 255] ^ (c >> 8);
    return c;
}
static u32 crc32c(const void *p, u32 n)
{
    return ~crc32c_cont(~0u, p, n);
}
// Proprietary metadata at 64 KiB carrying the Btrfs magic and CRC32C.
// Signature recognition is NOT Linux Btrfs compatibility: no Linux tree/chunk
// implementation exists here. Do not mount or repair this format with btrfs-progs.
static int super_write(u32 gen)
{
    u8 sb[4096];
    memset(sb, 0, sizeof sb);
    u64 total = (u64)block.sectors * 512, used = (u64)SECTORS * 512;
    for (int i = 0; i < 16; i++)
        sb[0x20 + i] = (u8)(0x7b + i * 5); // stable subset fsid
    *(u64 *)(sb + 0x30) = 65536;
    memcpy(sb + 0x40, "_BHRfS_M", 8);
    *(u64 *)(sb + 0x48) = gen;
    *(u64 *)(sb + 0x50) = 65536 + 4096; // subset root (this volume)
    *(u64 *)(sb + 0x58) = 65536 + 8192; // subset chunk root marker
    *(u64 *)(sb + 0x70) = total;
    *(u64 *)(sb + 0x78) = used;
    *(u64 *)(sb + 0x80) = 6;
    *(u64 *)(sb + 0x88) = 1;
    *(u32 *)(sb + 0x90) = 4096;  // sectorsize
    *(u32 *)(sb + 0x94) = 4096;  // nodesize
    *(u32 *)(sb + 0x98) = 4096;  // __unused_leafsize
    *(u32 *)(sb + 0x9c) = 4096;  // stripesize
    *(u32 *)(sb + 0x10b) = 0;    // csum type = CRC32
    // dev_item at 0xCb0? btrfs dev_item starts at offset 0xCb0 = 3248
    u8 *dv = sb + 3248;
    *(u64 *)(dv + 0) = 1;        // devid
    *(u64 *)(dv + 8) = total;
    *(u64 *)(dv + 16) = used;
    *(u32 *)(dv + 24) = 4096;
    *(u32 *)(dv + 28) = 4096;
    *(u32 *)(dv + 32) = 4096;
    for (int i = 0; i < 16; i++)
        dv[48 + i] = sb[0x20 + i];
    for (int i = 0; i < 16; i++)
        dv[64 + i] = sb[0x20 + i];
    *(u32 *)sb = crc32c(sb + 32, 4096 - 32);
    for (int i = 0; i < 8; i++)
        if (block.write(SUPER + i, sb + i * 512) < 0)
            return -1;
    return 0;
}
// The superblock lives on the candidate volume, not in the live RAM image:
// check it through the disk Block that volume_mount produced.
static int super_ok(const Block *d)
{
    u8 sb[4096];
    for (int i = 0; i < 8; i++)
        if (d->read(SUPER + i, sb + i * 512) < 0)
            return 0;
    if (memcmp(sb + 0x40, "_BHRfS_M", 8))
        return 0;
    return *(u32 *)sb == crc32c(sb + 32, 4096 - 32);
}
static int header(u8 *h)
{
    return !memcmp(h, "TMCBFS1\0", 8) && ((u32 *)h)[2] == SNAPREV && ((u32 *)h)[3] == NSLOTS
           && ((u32 *)h)[4] == DATA && ((u32 *)h)[5] <= 1;
}
static int legacy_header(u8 *h)
{
    return !memcmp(h, "TINYFS1\0", 8) && ((u32 *)h)[2] == 1 && ((u32 *)h)[3] == NSLOTS && ((u32 *)h)[4] == 16
           && ((u32 *)h)[5] <= 1;
}
// Relocate a legacy v1 image (data at 16) into the BTRFS-subset layout
// (data at 144) inside `out`, preserving nodes, accounts and factory marker.
// In-place on ram[]: legacy data at 16 → subset data at 144, descending so
// dst(i) never clobbers a not-yet-copied src (dst(i) = src(i+8), i+8 first).
static void relocate_ram(void)
{
    for (int i = NSLOTS - 1; i >= 0; i--)
        memcpy(ram[DATA + i * 16], ram[16 + i * 16], 16 * 512);
    memcpy(ram[0], "TMCBFS1\0", 8);
    u32 *h = (u32 *)ram[0];
    h[2] = SNAPREV;
    h[3] = NSLOTS;
    h[4] = DATA;
    memset(h + 6, 0, 40); // no encryption on a migrated legacy volume
}
static void cat_init(u8 *cat)
{
    memset(cat, 0, 512);
    memcpy(cat, "SNAPCAT1", 8);
    ((u32 *)cat)[2] = SNAPREV;
    ((u32 *)cat)[3] = 0;
    ((u32 *)cat)[4] = 0;
}
static int cat_write(void)
{
    u8 cat[512];
    cat_init(cat);
    return block.write(SNAPCAT, cat);
}
// Snapshot catalog (APFS-style): immutable point-in-time images of the whole
// volume stored at IMG0 + slot*SECTORS; browse read-only under /.snapshots.
#define SNAPMAX 8

static int cat_load(void)
{
    if (block.read(SNAPCAT, catbuf) < 0)
        return -1;
    if (memcmp(catbuf, "SNAPCAT1", 8) || ((u32 *)catbuf)[2] != SNAPREV)
        return -1;
    if (((u32 *)catbuf)[3] > SNAPMAX)
        ((u32 *)catbuf)[3] = 0;
    return 0;
}
static SnapEnt *cat_ent(int i)
{
    // Header: magic, rev, count, nextid (u32[2..4]); entries start at 32.
    return (SnapEnt *)(catbuf + 32 + i * sizeof(SnapEnt));
}
static int cat_save(void)
{
    return block.write(SNAPCAT, catbuf);
}
// Read-only view swap: overlay the image's node table for one gated call.
static int ovl_open(const char *p, const char **inner)
{
    // p = /.snapshots/<name>[/...]
    static const char pref[] = "/.snapshots/";
    ovl_slot = -1;
    if (memcmp(p, pref, sizeof pref - 1))
        return 0;
    const char *q = p + sizeof pref - 1;
    char nm[28];
    int i = 0;
    while (*q && *q != '/' && i < 27)
        nm[i++] = *q++;
    nm[i] = 0;
    if (!i) {
        logstr("[ovl] noname\n");
        return -1;
    }
    if (cat_load()) {
        logstr("[ovl] catload\n");
        return -1;
    }
    int c = (int)((u32 *)catbuf)[3];
    int k = -1;
    for (int j = 0; j < c && j < SNAPMAX; j++)
        if (cat_ent(j)->name[0] && eq(cat_ent(j)->name, nm)) {
            k = j;
            break;
        }
    if (k < 0) {
        logstr("[ovl] find\n");
        return -1;
    }
    u32 base = cat_ent(k)->base;
    memcpy(nodes_live, nodes, sizeof nodes);
    for (int j = 0; j < NSLOTS / 8; j++)
        if (block.read(NODE0 + j + base, nodes + j * 8) < 0) {
            memcpy(nodes, nodes_live, sizeof nodes);
            return -1;
        }
    ovl_slot = k;
    *inner = *q ? q : "/";
    return 0;
}
static void ovl_close(void)
{
    if (ovl_slot >= 0) {
        memcpy(nodes, nodes_live, sizeof nodes);
        ovl_slot = -1;
    }
}
static u32 ovl_base(void)
{
    return ovl_slot >= 0 ? cat_ent(ovl_slot)->base : 0;
}
static int validnodes(void)
{
    if (nodes[0].kind != 1 || nodes[0].parent)
        return 0;
    for (int i = 1; i < NSLOTS; i++)
        if (nodes[i].kind) {
            Node *n = nodes + i;
            if (n->kind > 2 || n->parent >= NSLOTS || nodes[n->parent].kind != 1 || n->size > MAXFILE || n->name[47] != 0
                || !n->name[0])
                return 0;
            int a = i, j = 0;
            while (a && j++ < NSLOTS) {
                if (a >= NSLOTS)
                    return 0;
                a = nodes[a].parent;
            }
            if (a)
                return 0;
        }
    return 1;
}
static void libraries(void)
{
    const char *dirs[] = {"/Library", "/Library/Application Support", "/Library/Preferences", HOME "/Library", HOME "/Library/Preferences", HOME "/Library/Application Support"};
    seeding = 1;
    for (u32 j = 0; j < COUNT(dirs); j++)if (resolve(dirs[j]) < 0)fs_create(dirs[j], 1);
    if (resolve("/Library/Preferences/com.tinymacos.system.plist") < 0) {
        const char *s =
            "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>ComputerName</key><string>Tramontana</string></dict></plist>\n";
        int i = fs_create("/Library/Preferences/com.tinymacos.system.plist", 0);
        if (i > 0 && bytes(i, 0, (void *)s, strlen(s), 1) == (int)strlen(s)) {
            nodes[i].size = strlen(s);
            nodes[i].ro = 1;
            syncnode(i);
        }
    }
    const char *path = HOME "/Library/Preferences/com.tinymacos.appearance.plist";
    int id = resolve(path);
    if (id < 0)id = fs_create(path, 0);
    if (id > 0) {
        nodes[id].ro = 1;
        syncnode(id);
        configid = id;
        fs_saveprefs();
    }
    seeding = 0;
}
void fs_init(void)
{
    persistent=usednodes=0;configid=-1;ovl_slot=-1;
    erase(nodes,sizeof nodes);erase(ram,sizeof ram);
    enc_state=enc_locked=0;erase(enc_key,sizeof enc_key);
    wall = 12;theme = 7;
    int imported = 0;
    u8 h[512], factory[512];
    Block disk;
    block = (Block) {
        mr, mw, NPART
    };
#ifdef TMC_DEBUG
    extern int sata_count(void);
    logstr("[fsdbg] sata=");
    lognum(sata_count());
    logstr("\n");
#endif
    int vm = volume_mount(&disk);
#ifdef TMC_DEBUG
    logstr("[fsdbg] vm=");
    lognum(vm);
    logstr(" sec=");
    lognum(vm ? disk.sectors : 0);
    if (vm && disk.read(0, h)) logstr(" rd=err\n");
    else if (vm) {
        logstr(" magic=");
        for (int i = 0; i < 8; i++) {
            u8 c = h[i];
            char t[3] = {(char)("0123456789abcdef"[c >> 4]), (char)("0123456789abcdef"[c & 15]), 0};
            logstr(t);
        }
        logstr(" act=");
        lognum(((u32 *)h)[5]);
        logstr("\n");
    }
#endif
    if (vm && disk.sectors >= SECTORS && !disk.read(0, h)) {
        if (legacy_header(h)) {
            // Legacy v1 on this volume: import it into the subset layout in
            // RAM; on an installed disk write the migrated image back in place.
            int ok = 1;
            for (u32 i = 0; i < 1552; i++)
                if (disk.read(i, ram[i])) {
                    ok = 0;
                    break;
                }
            int v = 0, active = 0;
            if (ok) {
                active = ((u32 *)h)[5] == 1 || boot.install_len ||
                         (!disk.read(15, factory) && !memcmp(factory, "TMFACT20", 8));
                memcpy(nodes, ram[1], sizeof nodes);
                v = validnodes();
                memset(nodes, 0, sizeof nodes);
            }
            if (ok && v && active) {
                relocate_ram();
                if (boot.install_len) {
                    imported = 1;
                } else {
                    int w = 1;
                    for (u32 i = 0; i < SECTORS; i++)
                        if (disk.write(i, ram[i])) {
                            w = 0;
                            break;
                        }
                    if (w) {
                        block = disk;
                        persistent = 1;
                    } else
                        memset(ram, 0, sizeof ram);
                }
            } else if (!boot.install_len && !imported && ok && active) {
                // Factory-style empty legacy image: adopt the volume, the
                // format path below rewrites it in the subset layout.
                block = disk;
                persistent = 1;
            }
            if (!imported && !persistent)
                memset(ram, 0, sizeof ram);
        } else if (header(h) &&
                   ((((u32 *)h)[5] == 1 && super_ok(&disk)) ||
                    (!disk.read(15, factory) && !memcmp(factory, "TMFACT20", 8)))) {
            // Active volumes need a valid superblock; factory images
            // (reset_data: header + TMFACT20, super wiped) are adopted so the
            // format path below can rewrite them in place.
            block = disk;
            persistent = 1;
        }
    }
    if ((persistent || imported) && ((u32 *)h)[5] == 1) {
        int ok = 1;
        for (int j = 0; j < NSLOTS / 8; j++)
            if (block.read(NODE0 + j, nodes + j * 8) < 0)
                ok = 0;
        if (ok && validnodes()) {
            for (int i = 0; i < NSLOTS; i++)
                usednodes += !!nodes[i].kind;
            {
                u8 eh[512];
                if (block.read(0, eh) == 0)
                    enc_load(eh);
            }
            configid = resolve(HOME "/Library/Preferences/com.tinymacos.appearance.plist");
            if (configid < 0)configid = resolve("/System/Configuration/Appearance");
            if(configid<0)configid=resolve(HOME "/Library/Preferences/com.tinymacos.appearance");
            u8 conf[6] = {0};char xml[1024]={0};
            int cn = configid > 0 ? nodes[configid].size : 0;
            int valid=0;
            if(configid>0&&cn>0&&cn<(int)sizeof xml&&bytes(configid,0,xml,cn,0)==cn){
                if(cn==3||cn==5||cn==6){memcpy(conf,xml,cn);valid=1;}
                else if(prefs_plist_read(xml,conf)){cn=6;valid=1;}
            }
            if (valid) {
                theme = conf[0] % NTHEME;
                wall = conf[1] % NWALL;
                reduce_motion = conf[3] & 1;
                opaque = conf[4] & 1;
                dock = conf[2] & 1;
                dark = cn >= 6 ? (conf[5] & 1) : darktheme(theme);
            }
            // Retire old manifest inodes on the imported copy, including the live installer.
            int apps = resolve("/Applications");
            for (int i = 1; i < NSLOTS; i++)if (nodes[i].kind == 2 && nodes[i].parent == apps && nodes[i].ro) {
                    u8 zeros[512] = {0};
                    for (int j = 0; j < 16; j++)block.write(DATA + i * 16 + j, zeros);
                    memset(nodes + i, 0, sizeof(Node));
                    syncnode(i);
                    usednodes--;
                }
            if (resolve(HOME "/Library/Preferences/com.tinymacos.appearance.plist") < 0)libraries();
            logstr("[prefs] loaded theme,wall,dock,reduce,opaque: ");
            lognum(theme);
            lognum(wall);
            lognum(dock);
            lognum(reduce_motion);
            lognum(opaque);
            logstr("\n");
            device_add(imported ? "Imported volume (RAM)" : "SATA / AHCI", "block", &block);
            logstr(imported ? "[fs] legacy volume imported read-only into live RAM\n" :
                   "[fs] mounted persistent BTRFS subset volume on SATA / AHCI\n");
            return;
        }
        logstr("[fs] invalid disk metadata; disk left untouched\n");
        persistent = 0;
        block = (Block) {
            mr, mw, NPART
        };
    }
    memset(nodes, 0, sizeof nodes);
    seeding = 1;
    nodes[0] = (Node) {
        .kind = 1, .ro = 1, .gen = 1
    };
    usednodes = 1;
    syncnode(0);
    seed("/Users", 1, 1, 0);
    seed(HOME, 1, 0, 0);
    seed(HOME "/Desktop", 1, 0, 0);
    seed(DOCS, 1, 0, 0);
    seed(HOME "/Downloads", 1, 0, 0);
    seed(HOME "/Pictures", 1, 0, 0);
    seed(HOME "/Welcome.txt", 0, 0,
         "Benvenuti in tinymacOS 10.1.7 Tramontana.\n\nQuesta cartella personale è condivisa dalle app. Conserva i file in Documents. Finder mostra il sistema realmente avviato in /System/Library/KernelCollections.\n\nCtrl+Spazio apre Cerca. Dal Centro di Controllo puoi regolare vetro e movimento.\n");
    seed("/Volumes", 1, 1, 0);
    libraries();
    seeding = 0;
    if (super_write(persistent ? 1 : 1) < 0)
        panic("btrfs superblock write failed", 0, 0);
    if (cat_write() < 0)
        panic("snapshot catalog write failed", 0, 0);
    if (persistent) {
        memset(h, 0, 512);
        memcpy(h, "TMCBFS1\0", 8);
        u32 *hh = (u32 *)h;
        hh[2] = SNAPREV;
        hh[3] = NSLOTS;
        hh[4] = DATA;
        hh[5] = 1;
        if (block.write(0, h) < 0)
            panic("filesystem activation failed", 0, 0);
    }
    device_add(persistent ? "SATA / AHCI" : "RAM disk", "block", &block);
    logstr(persistent ? "[fs] BTRFS subset volume active (APFS-style wrapper)\n" :
           "[fs] BTRFS subset live RAM volume (no compatible data disk)\n");
}
int fs_saveprefs_i()
{
    if (configid <= 0)return -1;
    u8 values[6]={theme,wall,dock,reduce_motion,opaque,dark};char c[1024];
    int size=prefs_plist_write(c,sizeof c,values);if(size<0)return -1;
    if (bytes(configid, 0, c, size, 1) == size) {
        u32 old = nodes[configid].size;
        nodes[configid].size = size;
        if (!syncnode(configid))return 0;
        nodes[configid].size = old;
    }
    return -1;
}

int fs_seek_i(Process *p, int d, u64 off)
{
    if (d < 0 || d >= 12)return -1;
    FD *f = p->fd + d;
    Dirent e;
    if (f->id & 0x40000000) {
        int i = snap_prep(f);
        if (i < 0) {
            ovl_close();
            return -1;
        }
        int bad = off > nodes[i].size;
        ovl_close();
        if (bad)return -1;
        f->at = off;
        return 0;
    }
    if (f->id >= 1024) {
        if (bootfs_stat(f->id, &e) < 0 || off > (u32)e.size)return -1;
    } else if (!valid(f) || off > nodes[f->id].size)return -1;
    f->at = off;
    return 0;
}
// Private metadata I/O restricted to the two reserved account sectors. No user fd.
int account_sector_i(int slot, void *buf, int wr)
{
    if ((!boot.install_len && !persistent) || slot < 0 || slot > 1)return -1;
    return wr ? block.write(13 + slot, buf) : block.read(13 + slot, buf);
}

// Make a consistent point-in-time copy while inside one kernel call.
int fs_snapshot_i(void *buf, u32 n)
{
    if (n < SECTORS * 512)return -1;
    u8 *out = buf;
    for (u32 i = 0; i < SECTORS; i++)if (block.read(i, out + i * 512) < 0)return -1;
    memset(out + SECTORS * 512, 0, n - SECTORS * 512);
    memcpy(out, "TMCBFS1\0", 8);
    u32 *h = (void *)out;
    h[2] = SNAPREV;
    h[3] = NSLOTS;
    h[4] = DATA;
    h[5] = 1;
    return 0;
}

int fs_touch_i(const char *p)
{
    p = mountedpath(p);
    if (bootfs_lookup(p) != -2)return -1;
    int i = resolve(p);
    if (i < 0)i = fs_create(p, 0);
    if (i <= 0 || nodes[i].ro)return -1;
    SysInfo t = {0};
    clock_info(&t);
    u32 old = nodes[i].reserved;
    nodes[i].reserved = ((t.year - 1980) << 25) | (t.month << 21) | (t.day << 16) | (t.hour << 11) |
                        (t.minute << 5) | (t.second / 2);
    if (syncnode(i)) {
        nodes[i].reserved = old;
        return -1;
    }
    fsrev++;
    return 0;
}

// ---- APFS-style snapshot operations (run inside the gate wrappers below) ----
static int snap_find(const char *name)
{
    if (!name || cat_load() < 0)
        return -1;
    for (int j = 0; j < SNAPMAX; j++) {
        SnapEnt *e = cat_ent(j);
        if (e->name[0] && eq(e->name, name))
            return j;
    }
    return -1;
}
static void cat_recount(void)
{
    int c = 0;
    for (int j = 0; j < SNAPMAX; j++)
        if (cat_ent(j)->name[0])
            c = j + 1;
    ((u32 *)catbuf)[3] = c;
}
static u32 snap_crc(u32 base)
{
    u8 buf[512];
    u32 st = ~0u;
    for (u32 i = 0; i < SECTORS; i++) {
        if (block.read(base + i, buf) < 0)
            return 0;
        st = crc32c_cont(st, buf, 512);
    }
    return ~st ? ~st : 1;
}
int fs_snap_create_i(const char *name)
{
    if (!name || !name[0] || strlen(name) > 27 || cat_load() < 0)
        return -1;
    for (const char *q = name; *q; q++)
        if ((unsigned char)*q < 33 || *q == '/')
            return -1;
    int k = -1;
    for (int j = 0; j < SNAPMAX; j++) {
        SnapEnt *e = cat_ent(j);
        if (e->name[0]) {
            if (eq(e->name, name))
                return -1;
        } else if (k < 0)
            k = j;
    }
    if (k < 0)
        return -1;
    u32 base = IMG0 + k * SECTORS;
    u8 buf[512];
    u32 st = ~0u;
    for (u32 i = 0; i < SECTORS; i++) {
        if (block.read(i, buf) < 0)
            return -1;
        st = crc32c_cont(st, buf, 512);
        if (block.write(base + i, buf) < 0)
            return -1;
    }
    u32 crc = ~st;
    st = ~0u;
    for (u32 i = 0; i < SECTORS; i++) {
        if (block.read(base + i, buf) < 0)
            return -1;
        st = crc32c_cont(st, buf, 512);
    }
    if (~st != crc)
        return -1;
    SnapEnt *e = cat_ent(k);
    memset(e, 0, sizeof *e);
    strset(e->name, name, 28);
    SysInfo t = {0};
    clock_info(&t);
    e->date = ((t.year - 1980) << 25) | (t.month << 21) | (t.day << 16) | (t.hour << 11) |
              (t.minute << 5) | (t.second / 2);
    e->id = ((u32 *)catbuf)[4] + 1;
    if (!e->id)
        e->id = 1;
    ((u32 *)catbuf)[4] = e->id;
    e->base = base;
    e->crc = crc;
    e->gen = (u32)fsrev + 1;
    cat_recount();
    if (cat_save() < 0) {
        memset(e, 0, sizeof *e);
        cat_recount();
        cat_save();
        return -1;
    }
    fsrev++;
    logstr("[fs] snapshot created: ");
    logstr(name);
    logstr(" id=");
    lognum(e->id);
    logstr("\n");
    return 0;
}
int fs_snap_list_i(void *out, int cap)
{
    if (cap < 0 || cat_load() < 0)
        return -1;
    SnapInfo *o = out;
    int n = 0;
    for (int j = 0; j < SNAPMAX && n < cap; j++) {
        SnapEnt *e = cat_ent(j);
        if (!e->name[0])
            continue;
        strset(o[n].name, e->name, 28);
        o[n].date = e->date;
        o[n].id = e->id;
        n++;
    }
    return n;
}
static void prefs_reload(void)
{
    configid = resolve(HOME "/Library/Preferences/com.tinymacos.appearance.plist");
    if (configid < 0)
        configid = resolve("/System/Configuration/Appearance");
    if(configid<0)configid=resolve(HOME "/Library/Preferences/com.tinymacos.appearance");
    u8 conf[6]={0};char xml[1024]={0};int cn=configid>0?nodes[configid].size:0,valid=0;
    if(configid>0&&cn>0&&cn<(int)sizeof xml&&bytes(configid,0,xml,cn,0)==cn){
        if(cn==3||cn==5||cn==6){memcpy(conf,xml,cn);valid=1;}
        else if(prefs_plist_read(xml,conf)){cn=6;valid=1;}
    }
    if(valid){
        theme = conf[0] % NTHEME;
        wall = conf[1] % NWALL;
        reduce_motion = conf[3] & 1;
        opaque = conf[4] & 1;
        dock = conf[2] & 1;
        dark = cn >= 6 ? (conf[5] & 1) : darktheme(theme);
    }
}
int fs_snap_restore_i(const char *name)
{
    int k = snap_find(name);
    if (k < 0)
        return -1;
    SnapEnt *e = cat_ent(k);
    if (snap_crc(e->base) != e->crc)
        return -1;
    u8 buf[512];
    for (u32 i = 0; i < SECTORS; i++) {
        if (block.read(e->base + i, buf) < 0)
            return -1;
        if (block.write(i, buf) < 0)
            return -1;
    }
    int ok = 1;
    for (int j = 0; j < NSLOTS / 8; j++)
        if (block.read(NODE0 + j, nodes + j * 8) < 0)
            ok = 0;
    if (!ok || !validnodes()) {
        // Best effort: keep the image we already wrote; next boot revalidates.
        logstr("[fs] restore: node reload needs reboot\n");
    }
    {
        u8 eh[512];
        if (block.read(0, eh) == 0)
            enc_load(eh);
    }
    prefs_reload();
    fsrev++;
    logstr("[fs] snapshot restored: ");
    logstr(name);
    logstr("\n");
    return 0;
}
int fs_snap_verify_i(const char *name)
{
    int k = snap_find(name);
    if (k < 0) {
        logstr("[vdbg] notfound\n");
        return -1;
    }
    u32 got = snap_crc(cat_ent(k)->base), want = cat_ent(k)->crc;
    logstr("[vdbg] k=");
    lognum(k);
    logstr(" base=");
    lognum(cat_ent(k)->base);
    logstr(" want=");
    loghex(want);
    logstr(" got=");
    loghex(got);
    logstr("\n");
    return got == want ? 0 : -1;
}
// ---- encryption control -------------------------------------------------
static void enc_mksalt(void)
{
    SysInfo t = {0};
    u8 m[40], o[32];
    memset(m, 0, sizeof m);
    clock_info(&t);
    u32 *w = (u32 *)m;
    w[0] = (u32)t.ticks;
    w[1] = (u32)(t.ticks >> 32);
    w[2] = ((u32)t.year << 16) | ((u32)t.month << 8) | (u32)t.day;
    w[3] = ((u32)t.hour << 24) | ((u32)t.minute << 16) | ((u32)t.second << 8) | (t.ticks & 0xff);
    w[4] = fsrev;
    w[5] = usednodes;
    w[6] = (u32)(u64)&t;
    w[7] = (u32)(u64)m;
    sha_bytes(m, 40, o);
    memcpy(enc_salt, o, 16);
    erase(o, 32);
    erase(m, 40);
}
static int enc_derive(const char *pass, u8 k[32], u8 v[16])
{
    if (!pass)
        return -1;
    int l = (int)strlen(pass);
    if (l < 4 || l > 63)
        return -1;
    PBKDF pb;
    pbk_begin(&pb, (const u8 *)pass, l, enc_salt);
    pbk_run(&pb, 12000);
    memcpy(k, pb.sum, 32);
    erase(&pb, sizeof pb);
    u8 t[32];
    sha_bytes(k, 32, t);
    memcpy(v, t, 16);
    erase(t, 32);
    return 0;
}
static int enc_hdrsave(void)
{
    u8 h[512];
    if (block.read(0, h) < 0)
        return -1;
    ((u32 *)h)[6] = (u32)enc_state;
    memcpy(h + 28, enc_salt, 16);
    memcpy(h + 44, enc_ver, 16);
    if (block.write(0, h) < 0)
        return -1;
    return 0;
}
static void enc_load(const u8 *h)
{
    u32 st = ((const u32 *)h)[6];
    if (st > 1)
        st = 0;
    if (!st) {
        enc_state = 0;
        enc_locked = 0;
        erase(enc_key, 32);
        memset(enc_salt, 0, 16);
        memset(enc_ver, 0, 16);
        return;
    }
    int same = enc_state && !memcmp(h + 28, enc_salt, 16);
    if (!same) {
        memcpy(enc_salt, h + 28, 16);
        memcpy(enc_ver, h + 44, 16);
        erase(enc_key, 32);
        enc_locked = 1;
    }
    enc_state = 1;
}
// Re-key every snapshot image to match the live per-file flags: encrypt or
// decrypt image extents (relative keystream = same as live), patch image node
// reserved bits and mirror the header enc block; refresh catalog crc.
static int enc_scrub(int up)
{
    static Node im[NSLOTS];
    u8 buf[512];
    for (int k = 0; k < SNAPMAX; k++) {
        SnapEnt *e = cat_ent(k);
        if (!e->name[0])
            continue;
        for (int s = 0; s < NSLOTS / 8; s++) {
            if (block.read(e->base + 1 + s, buf) < 0)
                return -1;
            memcpy((u8 *)im + s * 512, buf, 512);
        }
        for (int i = 1; i < NSLOTS; i++) {
            if (im[i].kind != 2)
                continue;
            int dox = 0;
            if (up && !im[i].reserved) {
                char fp[128];
                if (idpath_n(im, i, fp, sizeof fp) == 0 && !enc_ex(fp))
                    dox = 1;
            } else if (!up && im[i].reserved)
                dox = 1;
            if (!dox)
                continue;
            for (int j = 0; j < 16; j++) {
                u32 rel = DATA + i * 16 + j;
                if (block.read(e->base + rel, buf) < 0)
                    return -1;
                enc_xform(buf, rel, 0, 512);
                if (block.write(e->base + rel, buf) < 0)
                    return -1;
            }
            im[i].reserved = up ? 1 : 0;
        }
        for (int s = 0; s < NSLOTS / 8; s++) {
            memcpy(buf, (u8 *)im + s * 512, 512);
            if (block.write(e->base + 1 + s, buf) < 0)
                return -1;
        }
        if (block.read(e->base, buf) < 0)
            return -1;
        ((u32 *)buf)[6] = up ? 1 : 0;
        if (up) {
            memcpy(buf + 28, enc_salt, 16);
            memcpy(buf + 44, enc_ver, 16);
        } else
            memset(buf + 28, 0, 36);
        if (block.write(e->base, buf) < 0)
            return -1;
        e->crc = snap_crc(e->base);
    }
    return cat_save();
}
static int enc_enable_i(const char *pass)
{
    if (enc_state)
        return -1;
    if (cat_load() < 0)
        return -1;
    enc_mksalt();
    u8 k[32], v[16];
    if (enc_derive(pass, k, v))
        return -1;
    memcpy(enc_key, k, 32);
    memcpy(enc_ver, v, 16);
    erase(k, 32);
    erase(v, 16);
    enc_state = 1;
    enc_locked = 0;
    if (enc_hdrsave() < 0)
        return -1;
    int ch = 0;
    for (int i = 1; i < NSLOTS; i++) {
        if (nodes[i].kind != 2 || nodes[i].reserved)
            continue;
        char fp[128];
        if (idpath_n(nodes, i, fp, sizeof fp) || enc_ex(fp))
            continue;
        for (int j = 0; j < 16; j++) {
            u8 s[512];
            u32 l = DATA + i * 16 + j;
            if (block.read(l, s) < 0)
                return -1;
            enc_xform(s, l, 0, 512);
            if (block.write(l, s) < 0)
                return -1;
        }
        nodes[i].reserved = 1;
        if (syncnode(i) < 0)
            return -1;
        ch++;
    }
    if (enc_scrub(1) < 0)
        return -1;
    logstr("[fs] volume encrypted; user files=");
    lognum(ch);
    logstr("\n");
    return 0;
}
static int enc_disable_i(const char *pass)
{
    if (!enc_state)
        return -1;
    u8 k[32], v[16];
    if (enc_derive(pass, k, v) || memcmp(v, enc_ver, 16)) {
        erase(k, 32);
        erase(v, 16);
        return -1;
    }
    memcpy(enc_key, k, 32);
    erase(k, 32);
    erase(v, 16);
    if (cat_load() < 0)
        return -1;
    for (int i = 1; i < NSLOTS; i++) {
        if (nodes[i].kind != 2 || !nodes[i].reserved)
            continue;
        for (int j = 0; j < 16; j++) {
            u8 s[512];
            u32 l = DATA + i * 16 + j;
            if (block.read(l, s) < 0)
                return -1;
            enc_xform(s, l, 0, 512);
            if (block.write(l, s) < 0)
                return -1;
        }
        nodes[i].reserved = 0;
        if (syncnode(i) < 0)
            return -1;
    }
    if (enc_scrub(0) < 0)
        return -1;
    enc_state = 0;
    enc_locked = 0;
    erase(enc_key, 32);
    memset(enc_salt, 0, 16);
    memset(enc_ver, 0, 16);
    if (enc_hdrsave() < 0)
        return -1;
    logstr("[fs] volume decrypted\n");
    return 0;
}
// sub: 0 status, 1 encrypt, 2 decrypt, 3 lock, 4 unlock (FileVault-style).
int fs_enc_i(int sub, const char *pass, EncInfo *out)
{
    Vault *vault=volume_vault(volume_current());
    if(vault&&vault->present){if(sub||!out)return -1;*out=(EncInfo){.state=1,.locked=!vault->unlocked,.files=0};return 0;}
    if(sub==1)return -1; /* New encryption uses the authenticated volume format only. */
    if (sub == 0) {
        if (!out)
            return -1;
        out->state = enc_state ? 1 : 0;
        out->locked = (enc_state && enc_locked) ? 1 : 0;
        u32 f = 0;
        for (int i = 1; i < NSLOTS; i++)
            if (nodes[i].kind == 2 && nodes[i].reserved)
                f++;
        out->files = f;
        return 0;
    }
    if (sub == 3) {
        if (!enc_state || enc_locked)
            return -1;
        erase(enc_key, 32);
        enc_locked = 1;
        return 0;
    }
    if (sub == 4) {
        if (!enc_state || !enc_locked)
            return -1;
        u8 k[32], v[16];
        if (enc_derive(pass, k, v) || memcmp(v, enc_ver, 16)) {
            erase(k, 32);
            erase(v, 16);
            return -1;
        }
        memcpy(enc_key, k, 32);
        erase(k, 32);
        erase(v, 16);
        enc_locked = 0;
        return 0;
    }
    if (sub == 1)
        return enc_enable_i(pass);
    if (sub == 2)
        return enc_disable_i(pass);
    return -1;
}
int fs_snap_info_i(FsInfo *f)
{
    if (cat_load() < 0)
        return -1;
    int c = 0;
    for (int j = 0; j < SNAPMAX; j++)
        if (cat_ent(j)->name[0])
            c++;
    f->total = (u64)NPART * 512;
    f->used = (u64)(SECTORS + c * SECTORS) * 512;
    f->snaps = c;
    f->gen = (u32)fsrev;
    return 0;
}
int fs_snap_delete_i(const char *name)
{
    int k = snap_find(name);
    if (k < 0)
        return -1;
    memset(cat_ent(k), 0, sizeof(SnapEnt));
    cat_recount();
    if (cat_save() < 0)
        return -1;
    fsrev++;
    logstr("[fs] snapshot deleted: ");
    logstr(name);
    logstr("\n");
    return 0;
}

RecSpin fs_gate;

// Public wrappers: one recursive filesystem gate serializes all disk state.
int fs_stat(const char *p, Dirent *d)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_stat_i(p, d);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_list(const char *p, Dirent *d, int cap, int skip)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_list_i(p, d, cap, skip);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_create(const char *p, int dir)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_create_i(p, dir);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_remove(const char *p)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_remove_i(p);
    rec_unlock(&fs_gate, fl);
    return r;
}
int fs_remove_flags(const char *p,unsigned flags)
{
    u32 fl;rec_lock(&fs_gate,&fl);
    int r=fs_remove_flags_i(p,flags);
    rec_unlock(&fs_gate,fl);return r;
}

int fs_move(const char *p, const char *q)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_move_i(p, q);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_open(Process *p, const char *path, int w)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_open_i(p, path, w);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_close(Process *p, int fd)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_close_i(p, fd);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_read(Process *p, int d, void *b, int n)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_read_i(p, d, b, n);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_write(Process *p, int d, const void *b, int n)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_write_i(p, d, b, n);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_saveprefs(void)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_saveprefs_i();
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_seek(Process *p, int d, u64 off)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_seek_i(p, d, off);
    rec_unlock(&fs_gate, fl);
    return r;
}

int account_sector(int slot, void *buf, int wr)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = account_sector_i(slot, buf, wr);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snapshot(void *buf, u32 n)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_snapshot_i(buf, n);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_create(const char *name)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_snap_create_i(name);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_list(void *out, int cap)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_snap_list_i(out, cap);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_restore(const char *name)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_snap_restore_i(name);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_verify(const char *name)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_snap_verify_i(name);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_delete(const char *name)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_snap_delete_i(name);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_snap_info(FsInfo *f)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_snap_info_i(f);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_enc(int sub, const char *pass, EncInfo *out)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_enc_i(sub, pass, out);
    rec_unlock(&fs_gate, fl);
    return r;
}

int fs_touch(const char *p)
{
    u32 fl;
    rec_lock(&fs_gate, &fl);
    int r = fs_touch_i(p);
    rec_unlock(&fs_gate, fl);
    return r;
}
