#include "disklayout.h"
#include "container.h"
#include "abi.h"   /* NSLOTS (384): inode slots of the TMCBFS1 subset volume */
#define FSDATA 144u /* TMCBFS1 subset header h[4] (fs/btrfs.c DATA) */
#define FNODE 64u   /* disk inode table location, in sectors (fs/btrfs.c NODE0) */
#define FSUPER 128u /* superblock location, in sectors (fs/btrfs.c SUPER) */
static Part pt[128];
static Layout map;
static u8 buf[CHUNK * 512] ALIGNED(4096), data[NDATA * 512] ALIGNED(512);
static Seal seal;
u32 crc32(const void *v, size_t n)
{
    static u32 table[256];
    static int ready;
    if (!ready) {
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int j = 0; j < 8; j++)c = (c >> 1) ^ (0xedb88320u & -(c & 1));
            table[i] = c;
        }
        ready = 1;
    }
    u32 c = ~0u;
    const u8 *p = v;
    while (n--)c = table[(c^*p++) & 255] ^ (c >> 8);
    return ~c;
}
static int readh(Medium *m, u32 l, Gpt *h)
{
    if (m->io(m->d, l, 1, h, 0) || memcmp(h->sig, "EFI PART", 8) || h->size != 92 || h->rev != 0x10000 ||
        h->at != l || h->other != (l == 1 ? m->z - 1 : 1) || h->first != 34 || h->last != m->z - 34 ||
        h->entries != (l == 1 ? 2 : m->z - 33) || h->num != 128 || h->entrysize != 128)return 0;
    u32 c = h->crc;
    h->crc = 0;
    int ok = c == crc32(h, 92);
    h->crc = c;
    return ok && !m->io(m->d, h->entries, 32, pt, 0) && crc32(pt, sizeof pt) == h->ecrc;
}
int layout_read(Medium *m, Layout *x)
{
    Medium metadata=*m;if(m->raw){metadata.io=m->raw;metadata.raw=0;m=&metadata;}
    memset(x, 0, sizeof * x);
    static DiskMap inventory;
    if(partition_scan(m,&inventory)&&inventory.safe){
        for(int i=0;i<inventory.count;i++){
            DiskPartition*p=inventory.part+i;NativeContainer h;
            if(p->logical||(inventory.scheme==MAP_GPT?memcmp(p->type,esp_type,16):p->type[0]!=0xef))continue;
            if(p->sectors<TC_SECTORS||m->io(m->d,p->start+TC_HEADER,1,&h,0)||!container_bounds(&h,p->sectors))continue;
            u32 c=h.crc;h.crc=0;if(c!=crc32(&h,sizeof h))continue;
            x->pv=x->bv=1;x->type=2;x->container=1;
            x->esp=p->start;x->data=p->start+h.data;x->rec=p->start+h.rec;x->source=p->start+h.source;
            memcpy(x->g.guid,h.id,16);memcpy(x->parts[0].id,h.esp_id,16);
            u32 a[]={x->esp,x->data,x->rec,x->source},n[]={NESP,TC_DATA_N,NREC,NESP+1};
            for(int j=0;j<4;j++){x->parts[j].start=a[j];x->parts[j].end=a[j]+n[j]-1;}
            return 2;
        }
    }
    if (m->z < MINSECT || m->z > 268435456u)return 0;
    Gpt h;
    x->pv = readh(m, 1, &h);
    if (x->pv) {
        x->g = h;
        memcpy(x->parts, pt, sizeof pt);
    }
    x->bv = readh(m, m->z - 1, &h);
    if (x->bv) {
        if (x->pv && (memcmp(x->parts, pt, sizeof pt) || memcmp(x->g.guid, h.guid, 16)))return 0;
        x->g = h;
        memcpy(x->parts, pt, sizeof pt);
    }
    if (!x->pv && !x->bv)return 0;
    Part *p = x->parts;
    u32 z = m->z;
    if (memcmp(p[0].type, esp_type, 16) || p[0].start != LESP || p[0].end != LDATA - 1 ||
        memcmp(p[1].type, dat_type, 16) || p[1].start != LDATA)return 0;
    int type = p[1].end == z - 34 ? 1 : 2;
    if (type == 2 && (p[1].end != REC_AT(z) - 1 || p[1].end < LDATA + BACKUP_OFF + NDATA ||
                      memcmp(p[2].type, esp_type, 16) || p[2].start != REC_AT(z) || p[2].end != RESTORE_AT(z) - 1 ||
                      memcmp(p[3].type, src_type, 16) || p[3].start != RESTORE_AT(z) || p[3].end != z - 34))return 0;
    u8 zero[16] = {0};
    for (int i = type == 1 ? 2 : 4; i < 128; i++)if (memcmp(p[i].type, zero, 16))return 0;
    x->esp=LESP;x->data=LDATA;x->rec=REC_AT(m->z);x->source=RESTORE_AT(m->z);
    x->type = type;
    return type;
}
static int commit(Medium *m, u32 l, u32 n, const void *p)
{
    return m->io(m->d, l, n, (void *)p, 1) || m->flush(m->d) || m->io(m->d, l, n, buf, 0) ||
           memcmp(p, buf, n * 512) ? -1 : 0;
}
static int check_nodes(const u8 *n, int slots)
{
    if (n[0] != 1 || *(const u16 *)(n + 2))return 0;
    for (int i = 1; i < slots; i++) {
        const u8 *t = n + i * 64;
        if (!t[0])continue;
        if (t[0] > 2 || *(const u32 *)(t + 4) > 8192 || !t[12] || t[59])return 0;
        int j = i, k = 0;
        while (j && k++ < slots) {
            j = *(const u16 *)(n + j * 64 + 2);
            if (j >= slots || n[j * 64] != 1)return 0;
        }
        if (j)return 0;
    }
    return 1;
}
int tiny_check(const void *v)
{
    const u8 *r = v;
    const u32 *h = v;
    // BTRFS-subset header (sectors 0) or legacy v1 (import sources).
    // Legacy v1 (96 slots, h[4]=16) vs TMCBFS1 subset (NSLOTS slots, h[4]=DATA).
    int legacy = !memcmp(r, "TINYFS1\0", 8) && h[2] == 1 && h[3] == 96 && h[4] == 16 && h[5] <= 1;
    int subset = !memcmp(r, "TMCBFS1\0", 8) && h[2] == 2 && h[3] == NSLOTS && h[4] == FSDATA && h[5] <= 1;
    if (!legacy && !subset)return 0;
    if (legacy)return check_nodes(r + 512, 96);   /* legacy inode table lives at sector 1 */
    // Subset: superblock with BTRFS magic at sector FSUPER, disk inode table of
    // NSLOTS×64 B at sector FNODE (fs/btrfs.c NODE0); h[3] is the live slot count.
    const u8 *sb = r + FSUPER * 512;
    if (memcmp(sb + 0x40, "_BHRfS_M", 8))return 0;
    return check_nodes(r + FNODE * 512, NSLOTS);
}
static int bulk(Medium *m, u32 at, u32 n, u8 *p, int wr)
{
    for (u32 j = 0; j < n;) {
        u32 k = MIN(CHUNK, n - j);
        if (m->io(m->d, at + j, k, p + 512 * j, wr))return -1;
        j += k;
    }
    return wr ? m->flush(m->d) : 0;
}
static int getseal(Medium *, u32, const char *, u32);
static int hashdisk(Medium *, u32, u32, u8[32]);
static int verify_system(Medium *m)
{
    u8 sum[32];
    u32 l = map.source;
    return getseal(m, l, "TMCSYS20", NESP) && !hashdisk(m, map.esp, NESP, sum) && !memcmp(sum, seal.digest, 32);
}
int firstaid(Medium *m, int fix)
{
    if (!layout_read(m, &map) || bulk(m, map.data, NDATA, data, 0) || !tiny_check(data))return -1;
    if (map.type == 2 && !verify_system(m))return -1;
    if (map.pv && map.bv)return 0;
    if (!fix)return 1;
    u32 l = map.pv ? m->z - 1 : 1;
    Gpt h = map.g;
    h.at = l;
    h.other = l == 1 ? m->z - 1 : 1;
    h.entries = l == 1 ? 2 : m->z - 33;
    h.crc = 0;
    h.crc = crc32(&h, 92);
    return commit(m, h.entries, 32, map.parts) || commit(m, l, 1, &h) || !layout_read(m, &map) || !map.pv ||
           !map.bv ? -1 : 0;
}
static int getseal(Medium *m, u32 l, const char *magic, u32 n)
{
    return !m->io(m->d, l, 1, &seal, 0) && !memcmp(seal.magic, magic, 8) && seal.ver == 1 && seal.sectors == n &&
           !memcmp(seal.disk, map.g.guid, 16);
}
int has_copy(Medium *m)
{
    return layout_read(m, &map) == 2 && getseal(m, map.data + BACKUP_OFF - 1, "TMCSNAP2", NDATA);
}
int save_copy(Medium *m, const void *p)
{
    if (layout_read(m, &map) != 2 || !map.pv || !map.bv || !tiny_check(p))return -1;
    u32 l = map.data + BACKUP_OFF;
    memset(&seal, 0, sizeof seal);
    if (commit(m, l - 1, 1, &seal) || bulk(m, l, NDATA, (u8 *)p, 1) || bulk(m, l, NDATA, data, 0) ||
        memcmp(p, data, sizeof data))return -1;
    memcpy(seal.magic, "TMCSNAP2", 8);
    memcpy(seal.disk, map.g.guid, 16);
    seal.sectors = NDATA;
    seal.ver = 1;
    sha_bytes(data, sizeof data, seal.digest);
    return commit(m, l - 1, 1, &seal);
}
int restore_copy(Medium *m)
{
    if (!has_copy(m) || !map.pv || !map.bv || bulk(m, map.data + BACKUP_OFF, NDATA, data, 0) ||
        !tiny_check(data))return -1;
    u8 sum[32];
    sha_bytes(data, sizeof data, sum);
    if (memcmp(sum, seal.digest, 32))return -1;
    for (u32 j = 0; j < NDATA; j += CHUNK) {
        u32 k = MIN(CHUNK, NDATA - j);
        if (commit(m, map.data + j, k, data + j * 512))return -1;
    }
    return 0;
}
static int hashdisk(Medium *m, u32 l, u32 n, u8 out[32])
{
    SHA256 h;
    sha_init(&h);
    for (u32 j = 0; j < n; j += CHUNK) {
        u32 k = MIN(CHUNK, n - j);
        if (m->io(m->d, l + j, k, buf, 0))return -1;
        sha_update(&h, buf, k * 512);
    }
    sha_final(&h, out);
    return 0;
}
int restore_os(Medium *m)
{
    if (layout_read(m, &map) != 2 || !map.pv || !map.bv)return -1;
    u32 l = map.source;
    u8 sum[32];
    if (!getseal(m, l, "TMCSYS20", NESP) || hashdisk(m, l + 1, NESP, sum) || memcmp(sum, seal.digest, 32))return -1;
    for (u32 j = 0; j < NESP; j += CHUNK) {
        u32 k = MIN(CHUNK, NESP - j);
        if (m->io(m->d, l + 1 + j, k, buf, 0) || m->io(m->d, map.esp + j, k, buf, 1))return -1;
    }
    return m->flush(m->d) || hashdisk(m, map.esp, NESP, sum) || memcmp(sum, seal.digest, 32) ? -1 : 0;
}
int reset_data(Medium *m)
{
    if (layout_read(m, &map) != 2 || !map.pv || !map.bv)return -1;
    memset(data, 0, sizeof data);
    if (bulk(m, map.data, NDATA, data, 1) || commit(m, map.data + BACKUP_OFF - 1, 1, data) ||
        bulk(m, map.data + BACKUP_OFF, NDATA, data, 1))return -1;
    memcpy(data, "TMCBFS1\0", 8);
    u32 *h = (void *)data;
    h[2] = 2;
    h[3] = NSLOTS;   /* TMCBFS1 subset: live inode slot count (kernel requires 384) */
    h[4] = 144;
    memcpy(data + 15 * 512, "TMFACT20", 8);
    return commit(m, map.data + 15, 1, data + 15 * 512) || commit(m, map.data, 1, data) ? -1 : 0;
}
