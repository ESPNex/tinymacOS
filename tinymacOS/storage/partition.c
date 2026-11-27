/* Read-only partition inventory. Never infer free space from a filesystem signature.
 * Conservative 512-byte / 64-bit block-device ABI: reject unsupported metadata,
 * hybrids, overlaps and degraded GPT for allocation. EBR containers stay reserved.
 */
#include "disklayout.h"
static u8 sector[512] ALIGNED(512), other[512] ALIGNED(512);
static Part entries[128], mirror[128];
static u16 rd16(const u8*p){return p[0]|(u16)p[1]<<8;}
static u32 rd32(const u8*p){return rd16(p)|(u32)rd16(p+2)<<16;}
static int nz(const void*v,size_t n){const u8*p=v;while(n--)if(*p++)return 1;return 0;}
int filesystem_probe(Medium*m,u64 at,u64 n)
{
    u8 b[512] ALIGNED(512);
    if(!n||at>=m->z||n>m->z-at||m->io(m->d,at,1,b,0))return FS_UNREADABLE;
    if(!memcmp(b,"TMCBFS2\0",8))return FS_TMC2;
    if(!memcmp(b,"TMCBFS1\0",8)||!memcmp(b,"TINYFS1\0",8))return FS_TMC;
    if(b[510]==85&&b[511]==170){
        if(!memcmp(b+3,"NTFS    ",8)&&(rd16(b+11)==512||rd16(b+11)==4096)&&b[13]&&!(b[13]&(b[13]-1))&&b[13]<=128){
            u64 total;memcpy(&total,b+40,8);if(total&&total<=n/(rd16(b+11)/512)&&rd32(b+48)<total)return FS_NTFS;
        }
        if(!memcmp(b+3,"EXFAT   ",8)&&b[108]>=9&&b[108]<=12)return FS_EXFAT;
        u32 bps=rd16(b+11),factor=bps==4096?8:1;
        u32 spc=b[13],r=rd16(b+14),nf=b[16],roots=rd16(b+17),f=rd16(b+22),total=rd16(b+19);
        if(!total)total=rd32(b+32);
        int fat32=!f;if(fat32)f=rd32(b+36);
        u64 first=(u64)r+(u64)nf*f+((u64)roots*32+bps-1)/(bps?bps:512);
        if((bps==512||bps==4096)&&spc&&!(spc&(spc-1))&&spc<=128&&r&&nf&&nf<=2&&f&&(u64)total*factor<=n&&total>first){
            u32 c=(total-first)/spc;
            if(fat32&&!roots&&c>=65525&&c<0x0ffffff5&&rd32(b+44)>=2&&rd32(b+44)<c+2&&(u64)f*bps>=(u64)(c+2)*4)return FS_FAT32;
            if(!fat32&&roots&&c>=4085&&c<65525&&(u64)f*bps>=(c+2)*2)return FS_FAT16;
            if(!fat32&&roots&&c<4085&&(u64)f*bps*2>=(c+2)*3)return FS_FAT12;
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
static int header(Medium*m,u64 l,Gpt*h,Part*p){
 u32 scale=medium_scale(m);if(!scale||m->z%scale)return 0;u64 z=m->z/scale;
 if(m->io(m->d,l,1,h,0)||memcmp(h->sig,"EFI PART",8)||h->size<92||h->size>512||h->rev!=0x10000||h->at!=l/scale||h->other!=(l==scale?z-1:1)||h->res||!h->num||h->num>128||h->entrysize!=128)return 0;
 u32 size=(h->num*128+511)/512,physical=(size+scale-1)/scale;
 if(h->first<2+physical||h->last>=z-1-physical||h->first>h->last||h->entries<2||h->entries>=z-1||physical>z-1-h->entries||(l==scale?h->entries+physical>h->first:h->entries<=h->last))return 0;
 u32 sum=h->crc;h->crc=0;int good=crc32(h,h->size)==sum;h->crc=sum;
 if(!good)return 0;memset(p,0,128*sizeof(Part));
 if(m->io(m->d,h->entries*scale,size,p,0)||crc32(p,h->num*128)!=h->ecrc)return 0;
 if(scale!=1)for(u32 i=0;i<h->num;i++)if(nz(p[i].type,16)){if(p[i].start>=z||p[i].end>=z||p[i].end<p[i].start)return 0;p[i].start*=scale;p[i].end=(p[i].end+1)*scale-1;}
 return 1;
}
static void gap(DiskMap*x,u64 a,u64 end)
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
    u32 scale=medium_scale(m);if(!scale||m->z%scale||m->z<4*scale||m->z>TMC_DISK_MAX_SECTORS)return bad(x,"Geometria non supportata (massimo 16 TiB)");
    if(m->io(m->d,0,1,sector,0)||m->io(m->d,scale,1,other,0))return bad(x,"Errore di lettura della tabella");
    if(!nz(sector,512)&&!nz(other,512)){
        /* A zero first sector is NOT proof of an empty disk. The legacy blank
         * path scans every sector before its first write. */
        if(m->io(m->d,m->z-scale,1,other,0)||nz(other,512))return bad(x,"Metadati residui: disco protetto");
        x->scheme=MAP_EMPTY;x->safe=1;x->slot=0;gap(x,2048,m->z-(1+(32+scale-1)/scale)*scale);return 1;
    }
    int mbr_signature_ok=sector[510]==85&&sector[511]==170;
    u8 mbr[512];memcpy(mbr,sector,512);int protective=0;
    for(int i=0;i<4;i++)if(mbr[446+16*i+4]==0xee)protective++;
    if(!mbr_signature_ok&&!protective&&memcmp(other,"EFI PART",8))return bad(x,"Tabella non riconosciuta: disco protetto");
    u64 lo=1,hi=m->z;int pmbr_repair=0;const char *unsafe_reason=0;
    if(protective||!memcmp(other,"EFI PART",8)){
        x->scheme=MAP_GPT;Gpt a,b;int av=header(m,scale,&a,entries),bv=header(m,m->z-scale,&b,mirror);
        if(!av&&!bv)return bad(x,"GPT non valida: nessuna modifica consentita");
        Gpt*h=av?&a:&b;Part*p=av?entries:mirror;lo=h->first*scale;hi=(h->last+1)*scale;
        x->safe=protective==1&&av&&bv&&a.first==b.first&&a.last==b.last&&a.num==b.num&&a.entrysize==b.entrysize&&!memcmp(a.guid,b.guid,16)&&!memcmp(entries,mirror,a.num*128);
        if(!av)unsafe_reason="GPT primaria non verificabile: disco protetto";
        else if(!bv)unsafe_reason="GPT di backup non verificabile: disco protetto";
        else if(protective!=1)unsafe_reason="Voce MBR protettiva assente o duplicata: disco protetto";
        else if(!x->safe)unsafe_reason="Copie GPT discordanti: disco protetto";
        for(int i=0;i<4;i++){u8*t=mbr+446+16*i;
            if(!t[4]&&nz(t,16)){x->safe=0;unsafe_reason="Voce MBR residua: disco protetto";}
            if(t[4]&&t[4]!=0xee){x->safe=0;unsafe_reason="MBR ibrido non supportato: disco protetto";}
            if(t[4]==0xee){
                if(t[0]||rd32(t+8)!=1){x->safe=0;unsafe_reason="MBR protettivo: avvio o LBA iniziale ambiguo, disco protetto";}
                if(rd32(t+12)!=MIN(m->z/scale-1,0xffffffffull))pmbr_repair|=2;
            }
        }
        for(u32 i=0;i<h->num;i++){
            if(!nz(p[i].type,16)){if(x->slot<0)x->slot=i;continue;}
            if(!nz(p[i].id,16))return bad(x,"Identificatore GPT vuoto");
            for(u32 j=0;j<i;j++)if(nz(p[j].type,16)&&!memcmp(p[i].id,p[j].id,16))return bad(x,"Identificatori GPT duplicati");
            if(p[i].start<lo||p[i].end>=hi||p[i].end<p[i].start||!add(m,x,p[i].start,p[i].end-p[i].start+1,i+1,p[i].type,16))return bad(x,"GPT con estensioni non valide");
            memset(x->part[x->count-1].name,0,sizeof x->part[x->count-1].name);
            for(int j=0;j<35&&p[i].name[j];j++)x->part[x->count-1].name[j]=p[i].name[j]<128?p[i].name[j]:'?';
            static const u8 msr[16]={0x16,0xe3,0xc9,0xe3,0x5c,0x0b,0xb8,0x4d,0x81,0x7d,0xf9,0x2d,0xf0,0x02,0x15,0xae};
            if(!memcmp(p[i].type,msr,16)){DiskPartition*q=x->part+x->count-1;q->fs=FS_OTHER;strset(q->name,"Microsoft Reserved (MSR)",sizeof q->name);}
        }
        /* Keep the validated copy's read-only inventory, including gaps, even
         * when its peer/PMBR is unsafe. Allocation stays fail-closed below. */
    }else{
        if(scale!=1)return bad(x,"MBR su 4Kn non supportato: nessuna scrittura");
        x->scheme=MAP_MBR;x->safe=1;hi=MIN(hi,0x100000000ull);
        for(int i=0;i<4;i++){
            const u8*p=mbr+446+i*16;u32 a=rd32(p+8),n=rd32(p+12);
            if(!p[4]){if(n||a)return bad(x,"Voce MBR ambigua");if(x->slot<0)x->slot=i;continue;}
            if((p[0]!=0&&p[0]!=128)||(u64)a+n>0x100000000ull||!add(m,x,a,n,i+1,p+4,1))return bad(x,"MBR non valida");
            if(p[4]==0x42||p[4]==0xee)return bad(x,"Disco dinamico o ibrido: protetto");
        }
    }
    /* Allocation uses all occupied extents, irrespective of filesystem/type. */
    for(int i=0;i<x->count;i++)for(int j=i+1;j<x->count;j++)if(x->part[j].start<x->part[i].start){DiskPartition t=x->part[i];x->part[i]=x->part[j];x->part[j]=t;}
    u64 cursor=lo;
    for(int i=0;i<x->count;i++){DiskPartition*p=x->part+i;if(p->start<cursor)return bad(x,"Partizioni sovrapposte: disco protetto");gap(x,cursor,p->start);cursor=p->start+p->sectors;}
    gap(x,cursor,hi);
    if(x->scheme==MAP_MBR){int logical=5,primary=x->count;
        for(int i=0;i<primary;i++){DiskPartition*p=x->part+i;int t=p->type[0];if(t==5||t==15||t==0x85){p->fs=FS_OTHER;strset(p->name,"Partizione estesa",sizeof p->name);if(!extended(m,x,p->start,p->sectors,&logical))return 0;}}
    }
    if(!x->safe)return bad(x,unsafe_reason?unsafe_reason:"Tabella non verificata: disco protetto");
    if(x->scheme==MAP_GPT){
        if(!mbr_signature_ok)pmbr_repair|=1;
        if(pmbr_repair){
            /* Validated identical GPT copies, unambiguous sole EE entry,
             * valid nonoverlapping extents; only installer may normalize
             * coverage/signature, after explicit consent and fresh recheck. */
            x->pmbr_signature_repairable=x->slot>=0?pmbr_repair:0;
            return bad(x,(pmbr_repair&2)?"GPT verificata; copertura MBR protettivo da ripristinare con consenso":"GPT verificata; firma MBR assente: serve conferma ripristino");
        }
    }
    if(x->slot<0){strset(x->reason,"Tabella piena: nessuna voce libera",sizeof x->reason);return 1;}
    strset(x->reason,x->sectors?"Spazio non allocato disponibile":"Nessuno spazio non allocato",sizeof x->reason);return 1;
}
