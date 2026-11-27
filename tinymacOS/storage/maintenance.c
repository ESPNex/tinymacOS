#include "disklayout.h"
#include "container.h"
__attribute__((weak)) u32 sata_logical_bytes(int d){(void)d;return 512;}
static Part pt[128];
static Layout map;
static u8 buf[CHUNK * 512] ALIGNED(4096);
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
static int readh(Medium*m,u64 l,Gpt*h){
 u32 scale=medium_scale(m);if(!scale||m->z%scale)return 0;u64 z=m->z/scale,table=(32+scale-1)/scale;
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
            x->esp=p->start;x->data=p->start+h.data;x->rec=p->start+container_rec_at(&h);x->source=p->start+container_source_at(&h);
            memcpy(x->g.guid,h.id,16);memcpy(x->parts[0].id,h.esp_id,16);
            u64 a[]={x->esp,x->data,x->rec,x->source},n[]={NESP,container_rec_at(&h)-h.data,NREC,NESP+1};
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
    u64 z = m->z;
    if (memcmp(p[0].type, esp_type, 16) || p[0].start != LESP || p[0].end != LDATA - 1 ||
        memcmp(p[1].type, dat_type, 16) || p[1].start != LDATA)return 0;
    int type = !memcmp(p[2].type,esp_type,16)?2:1;
    if(p[1].end>medium_last(m)||p[1].end<LDATA+16384u-1)return 0;
    if (type == 2 && (p[1].end >= medium_rec(m) ||
                      memcmp(p[2].type, esp_type, 16) || p[2].start != medium_rec(m) || p[2].end != medium_source(m) - 1 ||
                      memcmp(p[3].type, src_type, 16) || p[3].start != medium_source(m) || p[3].end != medium_last(m)))return 0;
    u8 zero[16] = {0};
    for (int i = type == 1 ? 2 : 4; i < 128; i++)if (memcmp(p[i].type, zero, 16))return 0;
    x->esp=LESP;x->data=LDATA;x->rec=medium_rec(m);x->source=medium_source(m);
    x->type = type;
    return type;
}
static int commit(Medium *m, u64 l, u32 n, const void *p)
{
    return m->io(m->d, l, n, (void *)p, 1) || m->flush(m->d) || m->io(m->d, l, n, buf, 0) ||
           memcmp(p, buf, n * 512) ? -1 : 0;
}
static int getseal(Medium *, u64, const char *, u32);
static int hashdisk(Medium *, u64, u32, u8[32]);
static int verify_system(Medium *m)
{
    u8 sum[32];
    u64 l = map.source;
    return getseal(m, l, "TMCSYS20", NESP) && !hashdisk(m, map.esp, NESP, sum) && !memcmp(sum, seal.digest, 32);
}
/* System seal validation is independent of the user-volume format. */
int verify_system_volume(Medium*m){return layout_read(m,&map)==2&&map.pv&&map.bv&&verify_system(m)?0:-1;}
static int getseal(Medium *m, u64 l, const char *magic, u32 n)
{
    return !m->io(m->d, l, 1, &seal, 0) && !memcmp(seal.magic, magic, 8) && seal.ver == 1 && seal.sectors == n &&
           !memcmp(seal.disk, map.g.guid, 16);
}
static int hashdisk(Medium *m, u64 l, u32 n, u8 out[32])
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
    u64 l = map.source;
    u8 sum[32];
    if (!getseal(m, l, "TMCSYS20", NESP) || hashdisk(m, l + medium_scale(m), NESP, sum) || memcmp(sum, seal.digest, 32))return -1;
    for (u32 j = 0; j < NESP; j += CHUNK) {
        u32 k = MIN(CHUNK, NESP - j);
        if (m->io(m->d, l + medium_scale(m) + j, k, buf, 0) || m->io(m->d, map.esp + j, k, buf, 1))return -1;
    }
    return m->flush(m->d) || hashdisk(m, map.esp, NESP, sum) || memcmp(sum, seal.digest, 32) ? -1 : 0;
}
