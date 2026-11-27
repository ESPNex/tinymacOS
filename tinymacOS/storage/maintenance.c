#include "disklayout.h"
#include "container.h"
#include "nativefs.h"
#include "nativecow.h"
#include "abi.h"   /* NSLOTS (384): inode slots of the TMCBFS1 subset volume */
#define FSDATA 144u /* TMCBFS1 subset header h[4] (fs/btrfs.c DATA) */
#define FNODE 64u   /* disk inode table location, in sectors (fs/btrfs.c NODE0) */
#define FSUPER 128u /* superblock location, in sectors (fs/btrfs.c SUPER) */
__attribute__((weak)) u32 sata_logical_bytes(int d){(void)d;return 512;}
static Part pt[128];
static Layout map;
static u8 buf[CHUNK * 512] ALIGNED(4096), data[NDATA * 512] ALIGNED(512);
static Seal seal;
static NativeCow maintenance_cow;
static Medium*cow_medium;
static int cow_read(void*x,u32 l,void*p){(void)x;return cow_medium->io(cow_medium->d,map.data+l,1,p,0);}
static int cow_write(void*x,u32 l,const void*p){(void)x;return cow_medium->io(cow_medium->d,map.data+l,1,(void*)p,1)||cow_medium->flush(cow_medium->d);}
static int cow_open(Medium*m,NativeGeometry*g,int repair){
    cow_medium=m;CowIo io={0,map.parts[1].end-map.data+1,cow_read,cow_write};
    return repair?native_cow_mount(&maintenance_cow,io,g->image,g->backup-1):native_cow_mount_readonly(&maintenance_cow,io,g->image,g->backup-1);
}

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
static int readh(Medium*m,u32 l,Gpt*h){
 u32 scale=medium_scale(m);if(!scale||m->z%scale)return 0;u32 z=m->z/scale,table=(32+scale-1)/scale;
 if(m->io(m->d,l,1,h,0)||memcmp(h->sig,"EFI PART",8)||h->size!=92||h->rev!=0x10000||h->res||h->at!=l/scale||h->other!=(l==scale?z-1:1)||h->first!=2+table||h->last!=z-2-table||h->entries!=(l==scale?2:z-1-table)||h->num!=128||h->entrysize!=128)return 0;
 u32 c=h->crc;h->crc=0;int ok=c==crc32(h,92);h->crc=c;
 if(!ok||m->io(m->d,h->entries*scale,32,pt,0)||crc32(pt,sizeof pt)!=h->ecrc)return 0;
 if(scale!=1)for(unsigned i=0;i<128;i++)if(memcmp(pt[i].type,(u8[16]){0},16)){if(pt[i].start>=z||pt[i].end>=z||pt[i].end<pt[i].start)return 0;pt[i].start*=scale;pt[i].end=(pt[i].end+1)*scale-1;}
 return 1;
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
            u32 a[]={x->esp,x->data,x->rec,x->source},n[]={NESP,h.rec-h.data,NREC,NESP+1};
            for(int j=0;j<4;j++){x->parts[j].start=a[j];x->parts[j].end=a[j]+n[j]-1;}
            return 2;
        }
    }
    if (m->z < MINSECT || m->z > TMC_DISK_MAX_SECTORS)return 0;
    Gpt h;
    x->pv = readh(m, medium_scale(m), &h);
    if (x->pv) {
        x->g = h;
        memcpy(x->parts, pt, sizeof pt);
    }
    x->bv = readh(m, m->z - medium_scale(m), &h);
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
    int type = !memcmp(p[2].type,esp_type,16)?2:1;
    if(p[1].end>medium_last(m)||p[1].end<LDATA+NF_MIN_VOLUME-1)return 0;
    if (type == 2 && (p[1].end >= medium_rec(m) ||
                      memcmp(p[2].type, esp_type, 16) || p[2].start != medium_rec(m) || p[2].end != medium_source(m) - 1 ||
                      memcmp(p[3].type, src_type, 16) || p[3].start != medium_source(m) || p[3].end != medium_last(m)))return 0;
    u8 zero[16] = {0};
    for (int i = type == 1 ? 2 : 4; i < 128; i++)if (memcmp(p[i].type, zero, 16))return 0;
    x->esp=LESP;x->data=LDATA;x->rec=medium_rec(m);x->source=medium_source(m);
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
    int subset = !memcmp(r, "TMCBFS1\0", 8) && h[2] == 2 && h[3]>=NF_MIN && h[3]<=NSLOTS && !(h[3]%8) && h[4] == FSDATA && h[5] <= 1;
    if (!legacy && !subset)return 0;
    if (legacy)return check_nodes(r + 512, 96);   /* legacy inode table lives at sector 1 */
    // Subset: superblock with BTRFS magic at sector FSUPER, disk inode table of
    // NSLOTS×64 B at sector FNODE (fs/btrfs.c NODE0); h[3] is the live slot count.
    const u8 *sb = r + FSUPER * 512;
    if (memcmp(sb + 0x40, "_BHRfS_M", 8))return 0;
    NativeGeometry geom;if(!native_geometry(v,&geom))return 0;
    return check_nodes(r + FNODE * 512, geom.slots);
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
static int data_geometry(Medium*,NativeGeometry*);
static int hashdisk(Medium *, u32, u32, u8[32]);
static int verify_system(Medium *m)
{
    u8 sum[32];
    u32 l = map.source;
    return getseal(m, l, "TMCSYS20", NESP) && !hashdisk(m, map.esp, NESP, sum) && !memcmp(sum, seal.digest, 32);
}
/* System seal validation is independent of the user-volume format. */
int verify_system_volume(Medium*m){return layout_read(m,&map)==2&&map.pv&&map.bv&&verify_system(m)?0:-1;}
int firstaid(Medium *m, int fix)
{
    if (!layout_read(m, &map) || bulk(m, map.data, NDATA, data, 0) || !tiny_check(data))return -1;
    NativeGeometry geometry;if(!data_geometry(m,&geometry))return -1;
    int cow=cow_open(m,&geometry,fix);if(cow<0)return -1;
    if(cow)for(int k=0;k<8;k++)if((maintenance_cow.active&(1u<<k))&&native_cow_verify(&maintenance_cow,k))return -1;
    if (map.type == 2 && !verify_system(m))return -1;
    if (map.pv && map.bv)return 0;
    if (!fix)return 1;
    if(medium_scale(m)!=1)return -1; /* native GPT repair is deliberately not a legacy-sector rewrite */
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
static int data_geometry(Medium*m,NativeGeometry*g){
    u8 h[512];return !m->io(m->d,map.data,1,h,0)&&native_geometry(h,g)&&g->payload<=map.parts[1].end-map.data+1;
}
int has_copy(Medium *m)
{
    NativeGeometry g;
    return layout_read(m,&map)==2&&data_geometry(m,&g)&&getseal(m,map.data+g.backup-1,"TMCSNAP2",g.image);
}
int save_copy(Medium *m, const void *p)
{
    NativeGeometry g;
    if(layout_read(m,&map)!=2||!map.pv||!map.bv||!tiny_check(p)||!native_geometry(p,&g)||g.payload>map.parts[1].end-map.data+1)return -1;
    u32 l=map.data+g.backup;
    memset(&seal,0,sizeof seal);
    if(commit(m,l-1,1,&seal)||bulk(m,l,g.image,(u8*)p,1)||bulk(m,l,g.image,data,0)||memcmp(p,data,g.image*512))return -1;
    memcpy(seal.magic,"TMCSNAP2",8);memcpy(seal.disk,map.g.guid,16);seal.sectors=g.image;seal.ver=1;
    sha_bytes(data,g.image*512,seal.digest);return commit(m,l-1,1,&seal);
}
int restore_copy(Medium *m)
{
    NativeGeometry g;
    if(!has_copy(m)||!map.pv||!map.bv||!data_geometry(m,&g)||bulk(m,map.data+g.backup,g.image,data,0)||!tiny_check(data))return -1;
    u8 sum[32];sha_bytes(data,g.image*512,sum);if(memcmp(sum,seal.digest,32))return -1;
    NativeGeometry saved;if(!native_geometry(data,&saved)||saved.slots!=g.slots||saved.image!=g.image||saved.backup!=g.backup)return -1;
    int cow=cow_open(m,&g,1);if(cow<0)return -1;
    if(cow){
        for(u32 j=0;j<g.image;j++){
            if(j==NF_CAT||j==NC_SUPER||j==NC_JOURNAL||j==NC_STAGED)continue;
            if(native_cow_write(&maintenance_cow,j,data+j*512))return -1;
        }
        return 0;
    }
    for(u32 j=0;j<g.image;j+=8){
        u32 k=MIN(8,g.image-j);
        /* Snapshot allocation belongs to the current geometry/catalog, not
         * to the historical live state captured by the local data backup. */
        if(j==NF_CAT){if(commit(m,map.data+j+1,k-1,data+(j+1)*512))return -1;}
        else if(commit(m,map.data+j,k,data+j*512))return -1;
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
    if (!getseal(m, l, "TMCSYS20", NESP) || hashdisk(m, l + medium_scale(m), NESP, sum) || memcmp(sum, seal.digest, 32))return -1;
    for (u32 j = 0; j < NESP; j += CHUNK) {
        u32 k = MIN(CHUNK, NESP - j);
        if (m->io(m->d, l + medium_scale(m) + j, k, buf, 0) || m->io(m->d, map.esp + j, k, buf, 1))return -1;
    }
    return m->flush(m->d) || hashdisk(m, map.esp, NESP, sum) || memcmp(sum, seal.digest, 32) ? -1 : 0;
}
int reset_data(Medium *m)
{
    if (layout_read(m, &map) != 2 || !map.pv || !map.bv)return -1;
    NativeGeometry g;if(!data_geometry(m,&g))return -1;
    memset(data, 0, sizeof data);
    if (bulk(m, map.data, g.image, data, 1) || commit(m, map.data + g.backup - 1, 1, data) ||
        bulk(m, map.data + g.backup, g.image, data, 1))return -1;
    memcpy(data, "TMCBFS1\0", 8);
    u32 *h = (void *)data;
    h[2] = 2;
    h[3] = g.slots;
    if(g.sectors)native_geometry_store(data,&g);
    h[4] = 144;
    memcpy(data + 15 * 512, "TMFACT20", 8);
    return commit(m, map.data + 15, 1, data + 15 * 512) || commit(m, map.data, 1, data) ? -1 : 0;
}
