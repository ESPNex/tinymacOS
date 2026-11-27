/* Offline GPT/MBR partition editor. All writes are verified, not power-fail
 * atomic. GPT backup is committed first; a torn update is detected by scan and
 * blocks subsequent edits. The caller serializes this static workspace and
 * rejects the mounted disk. No Microsoft boot files or MBR bootstrap edits.
 * FAT32 resize supports a free tail with an unchanged FAT/data origin only;
 * it is NOT a relocator. Unsupported volumes fail in preview, before writes.
 */
#include "disklayout.h"
#include "diskedit.h"
#include "nativefs.h"
#include "tmcbfs2.h"
extern void*kmalloc(size_t);
extern void kfree(void*);
extern int ntfs_resize_native(Medium*,DiskEdit*,int);
#define NATIVE_MIN 65536u
static const u8 win_type[16]={0xa2,0xa0,0xd0,0xeb,0xe5,0xb9,0x33,0x44,0x87,0xc0,0x68,0xb6,0xb7,0x26,0x99,0xc7};
static DiskMap editmap;
static u8 bootsector[512],verifybuf[4096],mbrbuf[512],zero[4096];
static Part table[128];
static Gpt primary_edit,backup_edit;
static int say(DiskEdit*q,int r,const char*s){q->result=r;strset(q->message,s,sizeof q->message);return r;}
static u16 r16(const u8*p){return p[0]|(u16)p[1]<<8;}
static u32 r32(const u8*p){return r16(p)|(u32)r16(p+2)<<16;}
static void w32(u8*p,u32 n){memcpy(p,&n,4);}
static int nonzero(const u8*p,unsigned n){while(n--)if(*p++)return 1;return 0;}
static int readat(Medium*m,u64 at,u32 n,void*p){return at>=m->z||n>m->z-at||m->io(m->d,at,n,p,0);}
static int writeat(Medium*m,u64 at,u32 n,const void*p){
    if(!n||n>8||at>=m->z||n>m->z-at)return -1;
    return m->io(m->d,at,n,(void*)p,1)||m->flush(m->d)||readat(m,at,n,verifybuf)||memcmp(p,verifybuf,n*512)?-1:0;
}
static int readtable(Medium*m){
    if(readat(m,0,1,mbrbuf))return -1;
    if(editmap.scheme!=MAP_GPT)return 0;
    if(readat(m,1,1,&primary_edit)||readat(m,m->z-1,1,&backup_edit))return -1;
    memset(table,0,sizeof table);return readat(m,primary_edit.entries,(primary_edit.num+3)/4,table);
}
static int fingerprint(Medium*m,u8 hash[32]){
    SHA256 s;sha_init(&s);sha_update(&s,&m->z,sizeof m->z);sha_update(&s,mbrbuf,512);
    if(editmap.scheme==MAP_GPT){sha_update(&s,&primary_edit,512);sha_update(&s,&backup_edit,512);sha_update(&s,table,primary_edit.num*128);}
    if(editmap.scheme==MAP_MBR){
        for(int i=0;i<editmap.count;i++){
            DiskPartition*p=editmap.part+i;if(p->logical)continue;int t=p->type[0];if(t!=5&&t!=15&&t!=0x85)continue;
            u32 at=p->start;for(int j=0;j<128;j++){
                u8 b[512];if(readat(m,at,1,b))return -1;sha_update(&s,&at,4);sha_update(&s,b,512);
                if(!b[466])break;at=p->start+r32(b+470);
            }
        }
    }
    sha_final(&s,hash);return 0;
}
static int tablewrite(Medium*m){
    if(editmap.scheme!=MAP_GPT)return writeat(m,0,1,mbrbuf);
    primary_edit.ecrc=backup_edit.ecrc=crc32(table,primary_edit.num*128);
    primary_edit.crc=backup_edit.crc=0;primary_edit.crc=crc32(&primary_edit,primary_edit.size);backup_edit.crc=crc32(&backup_edit,backup_edit.size);
    u32 n=(primary_edit.num+3)/4;
    /* Preserve padding beyond the last GPT entry for non-multiple-of-four tables. */
    u8 tail[512];u32 rem=primary_edit.num%4;
    for(int copy=0;copy<2;copy++){
        Gpt*h=copy?&primary_edit:&backup_edit;
        for(u32 j=0;j<n;j++){
            const void*p=(const u8*)table+j*512;
            if(rem&&j==n-1){if(readat(m,h->entries+j,1,tail))return -1;memcpy(tail,p,rem*128);p=tail;}
            if(writeat(m,h->entries+j,1,p))return -1;
        }
        if(writeat(m,h->at,1,h))return -1;
    }
    return 0;
}
static int logical_delete(Medium*m,DiskPartition*target){
    u32 base=0;
    for(int i=0;i<editmap.count;i++){DiskPartition*p=editmap.part+i;int t=p->type[0];if(!p->logical&&(t==5||t==15||t==0x85)&&target->start>p->start&&target->start-p->start<p->sectors){base=p->start;break;}}
    if(!base)return -1;
    u32 at=base,prev=0;
    for(int j=0;j<128;j++){
        u8 b[512];if(readat(m,at,1,b))return -1;
        if(at+r32(b+454)==target->start){
            if(prev){u8 parent[512];if(readat(m,prev,1,parent))return -1;memcpy(parent+462,b+462,16);return writeat(m,prev,1,parent);}
            if(!b[466]){memset(b+446,0,32);return writeat(m,at,1,b);}
            u32 next=base+r32(b+470);u8 child[512];if(readat(m,next,1,child))return -1;
            u32 relative=next+r32(child+454)-base;
            memcpy(b+446,child+446,32);w32(b+454,relative);return writeat(m,base,1,b);
        }
        if(!b[466])break;prev=at;at=base+r32(b+470);
    }
    return -1;
}
/* The SAME CoW core and 16KiB on-disk format used by the installer.
 * New partitions only; never a conversion of an existing native volume. */
typedef struct {Medium*m;DiskEdit*q;} Format2;
static int format_read(void*x,u64 block,void*p){Format2*f=x;if(block>=f->q->size/32)return -1;return readat(f->m,f->q->start+block*32,32,p);}
static int format_write(void*x,u64 block,const void*p){Format2*f=x;if(block>=f->q->size/32)return -1;for(unsigned n=0;n<32;n+=8)if(writeat(f->m,f->q->start+block*32+n,8,(const u8*)p+n*512))return -1;return 0;}
static int format_flush(void*x){return ((Format2*)x)->m->flush(((Format2*)x)->m->d);}
static void*format_alloc(void*x,size_t n){(void)x;return kmalloc(n);}
static void format_free(void*x,void*p){(void)x;kfree(p);}
static int native_format(Medium*m,DiskEdit*q){
 Format2 f={m,q};T2Io io={&f,q->size/32,format_read,format_write,format_flush,format_alloc,format_free};Tmc2 fs;
 /* Invalidate both old superblock positions in the verified FREE extent. */
 memset(zero,0,sizeof zero);for(unsigned n=0;n<64;n+=8)if(writeat(m,q->start+n,8,zero))return -1;
 if(tmc2_format(&fs,io))return -1;int r=tmc2_verify(&fs);tmc2_close(&fs);if(r)return -1;
 if(tmc2_mount(&fs,io))return -1;r=tmc2_verify(&fs);tmc2_close(&fs);return r;
}
static int fattail(Medium*m,DiskEdit*q,DiskPartition*p,int apply){
    u64 a=p->start;if(readat(m,a,1,bootsector))return DE_IO;
    u32 r=r16(bootsector+14),fat=r32(bootsector+36),spc=bootsector[13],nf=bootsector[16],old=r32(bootsector+32);
    u32 info=r16(bootsector+48),backup=r16(bootsector+50),oldcl,newcl;
    if(r16(bootsector+19)||r16(bootsector+40)||nf!=2||!spc||!r||!fat||q->size<=r+(u64)nf*fat||old<=r+(u64)nf*fat)return DE_UNSUPPORTED;
    oldcl=(old-r-nf*fat)/spc;newcl=(q->size-r-nf*fat)/spc;
    if(newcl<65525||newcl>=0x0ffffff5||(u64)newcl+2>(u64)fat*128||info<1||info>=r||backup<1||backup>=r||info+backup>=r||info==backup||r32(bootsector+44)>=newcl+2)return DE_UNSUPPORTED;
    u8 b[512],c[512];u32 freeclusters=newcl>oldcl?newcl-oldcl:0;
    if(readat(m,a+backup,1,b)||memcmp(b,bootsector,512))return DE_PROTECTED;
    for(int copy=0;copy<2;copy++){
        if(readat(m,a+info+copy*backup,1,b))return DE_IO;
        if(r32(b)!=0x41615252||r32(b+484)!=0x61417272||r32(b+508)!=0xaa550000)return DE_PROTECTED;
    }
    /* Refuse dirty/hard-error volumes; compare every FAT sector. Reject any
     * allocated cluster or link that would end outside the shrunken volume. */
    for(u32 s=0;s<fat;s++){
        if(readat(m,a+r+s,1,b)||readat(m,a+r+fat+s,1,c))return DE_IO;
        if(memcmp(b,c,512))return DE_PROTECTED;
        if(!s&&((r32(b+4)&0x0c000000)!=0x0c000000))return DE_PROTECTED;
        for(u32 k=0;k<128;k++){
            u32 index=s*128+k,v=r32(b+4*k)&0x0fffffff;
            if(index<2||index>=oldcl+2)continue;
            if(index<newcl+2&&!v)freeclusters++;
            if(index>=newcl+2&&v)return DE_OCCUPIED;
            if(v>=2&&v<0x0ffffff0&&v>=newcl+2)return DE_OCCUPIED;
            if(v==1||(v>=0x0ffffff0&&v<0x0ffffff7))return DE_PROTECTED;
        }
    }
    if(!apply)return DE_OK;
    if(newcl>oldcl){
        /* FAT capacity must already exist. New bits are made free before the
         * boot sector exposes them; all currently addressable entries survive. */
        for(u32 s=(oldcl+2)/128;s<=(newcl+1)/128;s++){
            if(readat(m,a+r+s,1,b))return DE_IO;
            for(u32 k=0;k<128;k++){u32 i=s*128+k;if(i>=oldcl+2&&i<newcl+2)w32(b+4*k,0);}
            if(writeat(m,a+r+s,1,b)||writeat(m,a+r+fat+s,1,b))return DE_IO;
        }
    }
    for(int copy=1;copy>=0;copy--){
        if(readat(m,a+info+copy*backup,1,b))return DE_IO;w32(b+488,freeclusters);w32(b+492,0xffffffffu);
        if(writeat(m,a+info+copy*backup,1,b))return DE_IO;
    }
    w32(bootsector+32,q->size);
    return writeat(m,a+backup,1,bootsector)||writeat(m,a,1,bootsector)?DE_IO:DE_OK;
}
int partition_edit(Medium*m,DiskEdit*q,int apply){
    if(medium_scale(m)!=1)return say(q,DE_UNSUPPORTED,"Modifica/resize 4Kn non supportati: nessuna scrittura.");
    if(q->op<DISK_EDIT_CREATE||q->op>DISK_EDIT_EXFAT_PUBLISH||!m->flush)return say(q,DE_INVALID,"Richiesta non valida.");
    if(apply&&q->confirm!=DISK_EDIT_CONFIRM)return say(q,DE_PROTECTED,"Conferma esplicita obbligatoria.");
    if(!partition_scan(m,&editmap)||!editmap.safe)return say(q,DE_PROTECTED,"Tabella degradata, sovrapposta o non supportata: nessuna modifica.");
    if(editmap.scheme!=MAP_GPT&&editmap.scheme!=MAP_MBR)return say(q,DE_UNSUPPORTED,"Crea prima una tabella GPT con l’installer su disco vuoto.");
    if(readtable(m))return say(q,DE_IO,"Lettura metadati non riuscita.");
    u8 hash[32];if(fingerprint(m,hash))return say(q,DE_IO,"Lettura metadati non riuscita.");
    if(apply&&memcmp(hash,q->stamp,32))return say(q,DE_STALE,"La tabella è cambiata: seleziona di nuovo la partizione.");
    memcpy(q->stamp,hash,32);DiskPartition*p=0;
    if(q->op!=DISK_EDIT_CREATE&&q->op!=DISK_EDIT_EXFAT_PUBLISH){
        for(int i=0;i<editmap.count;i++)if(editmap.part[i].number==q->number)p=editmap.part+i;
        if(!p||p->start!=q->start||p->sectors!=q->sectors)return say(q,DE_STALE,"La selezione è cambiata: aggiorna l’elenco.");
    }
    if(q->op==DISK_EDIT_CREATE||q->op==DISK_EDIT_EXFAT_PUBLISH){
        int inside=0;for(int i=0;i<editmap.gaps;i++){DiskGap*g=editmap.gap+i;if(q->start>=g->start&&q->start-g->start<g->sectors&&q->size<=g->sectors-(q->start-g->start))inside=1;}
        if(!inside||(editmap.scheme==MAP_MBR&&(q->start>0xffffffffull||q->size>0xffffffffull))||q->size<NATIVE_MIN||q->start%2048||q->size%2048||editmap.slot<0||!q->name[0])return say(q,DE_INVALID,"Servono almeno 33.5 MB allineati, una voce libera e un nome.");
        for(unsigned j=0;j<sizeof q->name;j++){unsigned char c=q->name[j];if(!c)break;if(c<32||c>126||j==sizeof q->name-1)return say(q,DE_INVALID,"Usa un nome ASCII di massimo 47 caratteri.");}
        if(!apply)return say(q,0,q->op==DISK_EDIT_EXFAT_PUBLISH?"Nuova partizione exFAT nello spazio non allocato verificato. Nessuna partizione esistente verrà formattata.":"Crea TMCFS2: lo stesso filesystem proprietario CoW del sistema, non APFS né Btrfs Linux.");
        if(q->op==DISK_EDIT_EXFAT_PUBLISH){if(readat(m,q->start,1,bootsector)||memcmp(bootsector+3,"EXFAT   ",8)||bootsector[510]!=85||bootsector[511]!=170)return say(q,DE_INVALID,"Copia exFAT non verificata: partizione non pubblicata.");}
        if(q->op==DISK_EDIT_CREATE&&native_format(m,q))return say(q,DE_IO,"Errore di formattazione: partizione non pubblicata. Non riprovare senza verifica.");
        int slot=editmap.slot;
        if(editmap.scheme==MAP_GPT){
            Part*t=table+slot;memset(t,0,sizeof *t);memcpy(t->type,q->op==DISK_EDIT_EXFAT_PUBLISH?win_type:dat_type,16);
            SHA256 s;u8 id[32];sha_init(&s);sha_update(&s,q->stamp,32);sha_update(&s,&q->start,sizeof q->start);sha_update(&s,q->name,strlen(q->name));sha_final(&s,id);
            memcpy(t->id,id,16);t->id[6]=(t->id[6]&15)|64;t->id[8]=(t->id[8]&63)|128;
            t->start=q->start;t->end=(u64)q->start+q->size-1;for(int i=0;i<35&&q->name[i];i++)t->name[i]=(u8)q->name[i];
        }else{u8*t=mbrbuf+446+slot*16;memset(t,0,16);t[4]=q->op==DISK_EDIT_EXFAT_PUBLISH?7:0xda;t[1]=t[5]=0xfe;t[2]=t[3]=t[6]=t[7]=0xff;w32(t+8,q->start);w32(t+12,q->size);}
        q->number=slot+1;
    }else if(q->op==DISK_EDIT_DELETE){
        if(!apply)return say(q,0,"La partizione e l’accesso ai suoi dati saranno eliminati. Non è una cancellazione sicura.");
        if(p->logical){if(logical_delete(m,p))return say(q,DE_IO,"Errore durante l’aggiornamento EBR. Interrompi le modifiche e verifica il disco.");return say(q,0,"Partizione logica eliminata; contenitore esteso conservato.");}
        if(editmap.scheme==MAP_GPT)memset(table+p->number-1,0,sizeof(Part));else memset(mbrbuf+446+16*(p->number-1),0,16);
    }else{
        if(p->fs==FS_TMC2)return say(q,DE_UNSUPPORTED,"Il ridimensionamento TMCFS2 non è implementato: nessuna scrittura. La creazione usa tutta la capacità selezionata.");
        if(p->logical||(p->fs!=FS_FAT32&&p->fs!=FS_NTFS&&p->fs!=FS_TMC))return say(q,DE_UNSUPPORTED,"Servono partizioni primarie native, FAT32 o NTFS.");
        u64 end=(u64)p->start+q->size,max=(u64)p->start+p->sectors;
        for(int j=0;j<editmap.gaps;j++)if(editmap.gap[j].start==max){max+=editmap.gap[j].sectors;break;}
        if(!q->size||q->size%2048||end>max||q->size==p->sectors)return say(q,DE_INVALID,"Dimensione non valida o spazio contiguo insufficiente.");
        if(q->size>0xffffffffull||p->sectors>0xffffffffull)return say(q,DE_UNSUPPORTED,"Resize del formato legacy oltre 2 TiB non supportato: nessuna scrittura.");
        int ntfs=p->fs==FS_NTFS,native=p->fs==FS_TMC;
        int r=native?native_resize(m,q,0):ntfs?ntfs_resize_native(m,q,0):fattail(m,q,p,0);
        if(r&&native)return say(q,r,r==DE_OCCUPIED?"Nodi allocati nella coda di dati o snapshot: libera spazio prima di ridurre.":"Nativo: capacità 8.3–33.5 MB con numero di slot variabile; decifra FileVault e verifica dati/snapshot prima di riprovare.");
        if(!apply&&native&&!r)return say(q,0,"Ridimensiona la capacità nativa e ricolloca snapshot e copia locale; conserva tutti i file. Backup esterno obbligatorio.");
        if(r&&ntfs)return say(q,r,"NTFS non idoneo: volume sporco, ibernato, incoerente o risorse insufficienti. Nessuna scrittura.");
        if(r)return say(q,r,r==DE_OCCUPIED?"La coda contiene cluster allocati: nessuna scrittura. Serve uno strumento con rilocazione.":"FAT32 non idonea: controlla pulizia, copie FAT, backup e capacità della FAT.");
        if(!apply&&ntfs)return say(q,0,"NTFS verificato: rilocazione dei dati se necessaria. Dopo il resize esegui il controllo Windows.");
        if(!apply)return say(q,0,"Ridimensiona realmente FAT32 senza spostare dati; la coda rimossa deve essere libera.");
        if(editmap.scheme==MAP_GPT)table[p->number-1].end=end-1;else w32(mbrbuf+446+16*(p->number-1)+12,q->size);
        if(q->size>p->sectors&&tablewrite(m))return say(q,DE_IO,"Errore tabella durante la crescita: interrompi e verifica il disco.");
        r=native?native_resize(m,q,1):ntfs?ntfs_resize_native(m,q,1):fattail(m,q,p,1);if(r)return say(q,DE_IO,"Aggiornamento filesystem interrotto: non usare il volume prima di una verifica indipendente.");
        if(q->size>p->sectors)return say(q,0,"Filesystem e partizione ridimensionati; verifica il volume prima dell’uso.");
    }
    if(tablewrite(m))return say(q,DE_IO,"Aggiornamento tabella interrotto: interrompi e verifica entrambe le copie GPT/MBR.");
    return say(q,0,q->op==DISK_EDIT_CREATE?"Partizione nativa creata e verificata.":q->op==DISK_EDIT_DELETE?"Partizione eliminata. I dati non sono stati sovrascritti.":"Filesystem e partizione ridimensionati. NTFS richiede il controllo Windows.");
}
