/* Read-only partition inventory. Never infer free space from a filesystem signature.
 * Conservative 512-byte / 32-bit block-device ABI: reject unsupported metadata,
 * hybrids, overlaps and degraded GPT for allocation. EBR containers stay reserved.
 */
#include "disklayout.h"
static u8 sector[512] ALIGNED(512), other[512] ALIGNED(512);
static Part entries[128], mirror[128];
static u16 rd16(const u8*p){return p[0]|(u16)p[1]<<8;}
static u32 rd32(const u8*p){return rd16(p)|(u32)rd16(p+2)<<16;}
static int nz(const void*v,size_t n){const u8*p=v;while(n--)if(*p++)return 1;return 0;}
int filesystem_probe(Medium*m,u32 at,u32 n)
{
    u8 b[512] ALIGNED(512);
    if(!n||at>=m->z||n>m->z-at||m->io(m->d,at,1,b,0))return FS_UNREADABLE;
    if(!memcmp(b,"TMCBFS1\0",8)||!memcmp(b,"TINYFS1\0",8))return FS_TMC;
    if(b[510]==85&&b[511]==170){
        if(!memcmp(b+3,"NTFS    ",8)&&rd16(b+11)==512&&b[13]&&!(b[13]&(b[13]-1))&&b[13]<=128){
            u64 total;memcpy(&total,b+40,8);if(total&&total<=n&&rd32(b+48)<total)return FS_NTFS;
        }
        if(!memcmp(b+3,"EXFAT   ",8)&&b[108]==9)return FS_EXFAT;
        u32 spc=b[13],r=rd16(b+14),nf=b[16],roots=rd16(b+17),f=rd16(b+22),total=rd16(b+19);
        if(!total)total=rd32(b+32);
        int fat32=!f;if(fat32)f=rd32(b+36);
        u64 first=(u64)r+(u64)nf*f+((u64)roots*32+511)/512;
        if(rd16(b+11)==512&&spc&&!(spc&(spc-1))&&spc<=128&&r&&nf&&nf<=2&&f&&total<=n&&total>first){
            u32 c=(total-first)/spc;
            if(fat32&&!roots&&c>=65525&&c<0x0ffffff5&&rd32(b+44)>=2&&rd32(b+44)<c+2&&(u64)f*512>=(u64)(c+2)*4)return FS_FAT32;
            if(!fat32&&roots&&c>=4085&&c<65525&&(u64)f*512>=(c+2)*2)return FS_FAT16;
            if(!fat32&&roots&&c<4085&&(u64)f*1024>=(c+2)*3)return FS_FAT12;
        }
    }
    if(n>128&&!m->io(m->d,at+128,1,b,0)&&!memcmp(b+64,"_BHRfS_M",8))return FS_BTRFS;
    return FS_RAW;
}
static int bad(DiskMap*x,const char*s){x->safe=0;strset(x->reason,s,sizeof x->reason);return 0;}
static int add(Medium*m,DiskMap*x,u64 a,u64 n,int num,const u8*t,int tn)
{
    if(!n||!a||a>=m->z||n>m->z-a||x->count>=DISK_PARTS)return bad(x,"Partizioni fuori limite o troppe partizioni");
    DiskPartition*p=x->part+x->count++;p->start=a;p->sectors=n;p->number=num;memcpy(p->type,t,tn);
    p->fs=filesystem_probe(m,a,n);strset(p->name,filesystem_name(p->fs),sizeof p->name);return 1;
}
static int header(Medium*m,u32 l,Gpt*h,Part*p)
{
    if(m->io(m->d,l,1,h,0)||memcmp(h->sig,"EFI PART",8)||h->size<92||h->size>512||h->rev!=0x10000||h->at!=l||h->other!=(l==1?m->z-1:1)||h->res||h->num<1||h->num>128||h->entrysize!=128||h->first<34||h->last>=m->z-33||h->first>h->last||h->entries>0xffffffffu)return 0;
    u32 crc=h->crc;h->crc=0;int ok=crc32(h,h->size)==crc;h->crc=crc;
    u32 sectors=(h->num*128+511)/512;
    if(!ok||h->entries<2||h->entries+sectors>m->z-1||
       (l==1?h->entries+sectors>h->first:h->entries<=h->last))return 0;
    memset(p,0,128*sizeof(Part));
    return !m->io(m->d,h->entries,sectors,p,0)&&crc32(p,h->num*128)==h->ecrc;
}
static void gap(DiskMap*x,u32 a,u32 end)
{
    if(a>=end||x->gaps>=DISK_GAPS)return;
    x->gap[x->gaps++]=(DiskGap){a,end-a};u64 aligned=((u64)a+2047)&~2047ull;
    if(aligned<end&&end-aligned>x->sectors){x->start=aligned;x->sectors=end-aligned;}
}
static int extended(Medium*m,DiskMap*x,u32 base,u32 size,int *number)
{
    u32 ebr=base,seen[128];int used=0;
    for(;;){
        if(used>=127||ebr<base||ebr>=base+size)return bad(x,"Catena EBR fuori limite");
        for(int j=0;j<used;j++)if(seen[j]==ebr)return bad(x,"Ciclo nella catena EBR");seen[used++]=ebr;
        u8 b[512];if(m->io(m->d,ebr,1,b,0)||b[510]!=85||b[511]!=170||nz(b+478,32))return bad(x,"EBR non valida");
        u8*p=b+446,*next=b+462;u32 rel=rd32(p+8),n=rd32(p+12);u64 a=(u64)ebr+rel;
        if(ebr==base&&!nz(p,32))break; /* empty retained extended container */
        if(!p[4]||!rel||a<base||a+n>(u64)base+size||!add(m,x,a,n,(*number)++,p+4,1))return bad(x,"Volume logico fuori limite");
        x->part[x->count-1].logical=1;
        if(!next[4]){if(nz(next+8,8))return bad(x,"Collegamento EBR ambiguo");break;}
        if(next[4]!=5&&next[4]!=15&&next[4]!=0x85)return bad(x,"Collegamento EBR non supportato");
        u32 r=rd32(next+8),s=rd32(next+12);if(!r||r>=size||!s||s>size-r)return bad(x,"Collegamento EBR fuori limite");ebr=base+r;
    }
    for(int i=0;i<x->count;i++)if(x->part[i].logical){DiskPartition*p=x->part+i;
        for(int k=0;k<used;k++)if(seen[k]>=p->start&&seen[k]-p->start<p->sectors)return bad(x,"EBR sovrapposta ai dati");
        for(int j=i+1;j<x->count;j++)if(x->part[j].logical&&p->start<(u64)x->part[j].start+x->part[j].sectors&&x->part[j].start<(u64)p->start+p->sectors)return bad(x,"Volumi logici sovrapposti");
    }
    return 1;
}
int partition_scan(Medium*m,DiskMap*x)
{
    memset(x,0,sizeof *x);x->slot=-1;
    if(m->z<4||m->z>TMC_DISK_MAX_SECTORS)return bad(x,"Disco oltre i limiti del driver (512 B, 1 TiB)");
    if(m->io(m->d,0,1,sector,0)||m->io(m->d,1,1,other,0))return bad(x,"Errore di lettura della tabella");
    if(!nz(sector,512)&&!nz(other,512)){
        /* A zero first sector is NOT proof of an empty disk. The legacy blank
         * path scans every sector before its first write. */
        if(m->io(m->d,m->z-1,1,other,0)||nz(other,512))return bad(x,"Metadati residui: disco protetto");
        x->scheme=MAP_EMPTY;x->safe=1;x->slot=0;gap(x,2048,m->z-33);return 1;
    }
    if(sector[510]!=85||sector[511]!=170)return bad(x,"Tabella non riconosciuta: disco protetto");
    u8 mbr[512];memcpy(mbr,sector,512);int protective=0;
    for(int i=0;i<4;i++)if(mbr[446+16*i+4]==0xee)protective++;
    u32 lo=1,hi=m->z;
    if(protective||!memcmp(other,"EFI PART",8)){
        x->scheme=MAP_GPT;Gpt a,b;int av=header(m,1,&a,entries),bv=header(m,m->z-1,&b,mirror);
        if(!av&&!bv)return bad(x,"GPT non valida: nessuna modifica consentita");
        Gpt*h=av?&a:&b;Part*p=av?entries:mirror;lo=h->first;hi=h->last+1;
        x->safe=av&&bv&&a.first==b.first&&a.last==b.last&&a.num==b.num&&a.entrysize==b.entrysize&&!memcmp(a.guid,b.guid,16)&&!memcmp(entries,mirror,a.num*128);
        for(int i=0;i<4;i++){u8*t=mbr+446+16*i;if(t[4]&&t[4]!=0xee)x->safe=0;if(t[4]==0xee&&(t[0]||rd32(t+8)!=1||rd32(t+12)!=m->z-1))x->safe=0;}
        for(u32 i=0;i<h->num;i++){
            if(!nz(p[i].type,16)){if(x->slot<0)x->slot=i;continue;}
            if(!nz(p[i].id,16))return bad(x,"Identificatore GPT vuoto");
            for(u32 j=0;j<i;j++)if(nz(p[j].type,16)&&!memcmp(p[i].id,p[j].id,16))return bad(x,"Identificatori GPT duplicati");
            if(p[i].start<lo||p[i].end>=hi||p[i].end<p[i].start||!add(m,x,p[i].start,p[i].end-p[i].start+1,i+1,p[i].type,16))return bad(x,"GPT con estensioni non valide");
            memset(x->part[x->count-1].name,0,sizeof x->part[x->count-1].name);
            for(int j=0;j<35&&p[i].name[j];j++)x->part[x->count-1].name[j]=p[i].name[j]<128?p[i].name[j]:'?';
        }
        if(!x->safe)return bad(x,"GPT degradata, ibrida o copie discordanti: disco protetto");
    }else{
        x->scheme=MAP_MBR;x->safe=1;
        for(int i=0;i<4;i++){
            const u8*p=mbr+446+i*16;u32 a=rd32(p+8),n=rd32(p+12);
            if(!p[4]){if(n||a)return bad(x,"Voce MBR ambigua");if(x->slot<0)x->slot=i;continue;}
            if((p[0]!=0&&p[0]!=128)||!add(m,x,a,n,i+1,p+4,1))return bad(x,"MBR non valida");
            if(p[4]==0x42||p[4]==0xee)return bad(x,"Disco dinamico o ibrido: protetto");
        }
    }
    /* Allocation uses all occupied extents, irrespective of filesystem/type. */
    for(int i=0;i<x->count;i++)for(int j=i+1;j<x->count;j++)if(x->part[j].start<x->part[i].start){DiskPartition t=x->part[i];x->part[i]=x->part[j];x->part[j]=t;}
    u32 cursor=lo;
    for(int i=0;i<x->count;i++){DiskPartition*p=x->part+i;if(p->start<cursor)return bad(x,"Partizioni sovrapposte: disco protetto");gap(x,cursor,p->start);cursor=p->start+p->sectors;}
    gap(x,cursor,hi);
    if(x->scheme==MAP_MBR){int logical=5,primary=x->count;
        for(int i=0;i<primary;i++){DiskPartition*p=x->part+i;int t=p->type[0];if(t==5||t==15||t==0x85){p->fs=FS_OTHER;strset(p->name,"Partizione estesa",sizeof p->name);if(!extended(m,x,p->start,p->sectors,&logical))return 0;}}
    }
    if(x->slot<0){strset(x->reason,"Tabella piena: nessuna voce libera",sizeof x->reason);return 1;}
    strset(x->reason,x->sectors?"Spazio non allocato disponibile":"Nessuno spazio non allocato",sizeof x->reason);return 1;
}
