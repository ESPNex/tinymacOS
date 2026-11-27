/* SPDX-License-Identifier: GPL-2.0-or-later
 * A new REAL exFAT partition in verified free space. Source and existing
 * partitions never receive writes. Publication is LAST, after readback.
 */
#include "../kernel/k.h"
#include "fs2_runtime.h"
#include "volumes.h"
#include "volume_export.h"
#include "exfat_copy.h"
#include "disklayout.h"
typedef struct Source {struct Source*next;u64 id;char path[PATHLEN];} Source;
typedef struct {Process files;Source*source;u64 nextid;VolumeExport*q;Medium medium;int preview;u8 sector[512],verify[4096];} Export;
static DiskMap export_map;
static int message(VolumeExport*q,int result,const char*s){q->result=result;strset(q->message,s,sizeof q->message);return result;}
static Source*find(Export*x,u64 id){for(Source*s=x->source;s;s=s->next)if(s->id==id)return s;return 0;}
static Source*add(Export*x,const char*path){Source*s=kmalloc(sizeof *s);if(!s)return 0;memset(s,0,sizeof *s);s->id=++x->nextid;strset(s->path,path,PATHLEN);s->next=x->source;x->source=s;return s;}
static int next(void*context,u64 parent,u64*cookie,ExEntry*out){
 Export*x=context;Source*s=find(x,parent);if(!s||*cookie>0x7fffffffu)return -1;Dirent e;int n=fs_list(s->path,&e,1,*cookie);if(n<=0)return n;
 char path[PATHLEN];if(strlen(s->path)+1+strlen(e.name)>=PATHLEN||eq(e.name,".")||eq(e.name,".."))return -1;
 strset(path,s->path,PATHLEN);if(!eq(path,"/"))append(path,"/",PATHLEN);append(path,e.name,PATHLEN);Source*child=add(x,path);if(!child)return -1;
 *out=(ExEntry){.id=child->id,.size=e.size,.dir=e.dir};strset(out->name,e.name,48);(*cookie)++;return 1;
}
static i64 read_source(void*context,u64 id,u64 off,void*p,size_t bytes){Export*x=context;Source*s=find(x,id);if(!s)return -1;int fd=fs_open(&x->files,s->path,0);if(fd<0)return -1;i64 n=fs_seek(&x->files,fd,off)?-1:fs_read(&x->files,fd,p,bytes);fs_close(&x->files,fd);return n;}
static int write_copy(void*context,u64 at,const void*p,size_t bytes){
 Export*x=context;DiskEdit*q=&x->q->part;u64 limit=q->size*512;if(at>limit||bytes>limit-at)return -1;
 while(bytes){u64 lba=q->start+at/512;unsigned offset=at%512,n=MIN(bytes,512-offset);if(offset||n<512){if(sata_io(q->disk,lba,1,x->sector,0))return -1;memcpy(x->sector+offset,p,n);}else memcpy(x->sector,p,512);
  if(sata_io(q->disk,lba,1,x->sector,1)||sata_flush(q->disk)||sata_io(q->disk,lba,1,x->verify,0)||memcmp(x->sector,x->verify,512))return -1;
  at+=n;p=(const u8*)p+n;bytes-=n;
 }return 0;
}
static void*allocate(void*x,size_t bytes){(void)x;return kmalloc(bytes);}static void deallocate(void*x,void*p){(void)x;kfree(p);}
static int prepare(void*context,u64 logical,u64 stored){
 (void)stored;Export*x=context;VolumeExport*q=x->q;DiskEdit old=q->part;int apply=old.confirm==DISK_EDIT_CONFIRM;
 if(!partition_scan(&x->medium,&export_map)||!export_map.safe||sata_logical_bytes(old.disk)!=512){message(q,-1,"Tabella non verificabile o disco 4Kn: nessuna scrittura.");return -1;}
 if(!apply){
  memset(&q->part,0,sizeof q->part);q->part.disk=old.disk;q->part.op=DISK_EDIT_EXFAT_PUBLISH;q->part.size=logical/512;strset(q->part.name,"Copia exFAT",48);
  int found=0;for(int i=0;i<export_map.gaps;i++){DiskGap*g=export_map.gap+i;u64 a=(g->start+2047)&~2047ull;if(a>=g->start&&a-g->start<=g->sectors&&q->part.size<=g->sectors-(a-g->start)){q->part.start=a;found=1;break;}}
  if(!found||partition_edit(&x->medium,&q->part,0)){message(q,-1,"Spazio non allocato verificato insufficiente o nessuna voce libera. Nessuna scrittura.");return -1;}
  q->bytes=logical;x->preview=1;message(q,1,"Anteprima pronta: crea una nuova partizione exFAT nello spazio non allocato. Conferma per copiare; originali e snapshot invariati.");return -1;
 }
 if(old.op!=DISK_EDIT_EXFAT_PUBLISH||old.size!=logical/512){message(q,-1,"Sorgente o dimensione cambiata: ripeti l’anteprima.");return -1;}
 DiskEdit now=old;now.confirm=0;if(partition_edit(&x->medium,&now,0)||memcmp(now.stamp,old.stamp,32)){message(q,-1,"Tabella cambiata: ripeti l’anteprima. Nessuna scrittura.");return -1;}
 /* Never rely on old free-space contents being zero. Writes stay bounded to
  * the checked gap, before any partition table entry is made visible. */
 u8 zeros[512]={0};for(u64 at=0;at<logical;at+=512)if(write_copy(x,at,zeros,MIN(512ull,logical-at))){message(q,-1,"Errore nello spazio libero: partizione non pubblicata.");return -1;}q->bytes=logical;return 0;
}
static u64 origin(void*context){return ((Export*)context)->q->part.start;}
static int snapshot_root(const char*p){const char*prefix="/.snapshots/";size_t n=strlen(prefix);if(strlen(p)<=n||memcmp(p,prefix,n))return 0;const char*name=p+n;if(eq(name,"Installazione"))return 0;for(const char*s=name;*s;s++)if(*s=='/')return 0;SnapInfo page[16];int skip=0,r;while((r=fs_snap_page(page,16,skip))>0){for(int i=0;i<r;i++)if(eq(page[i].name,name))return 1;if(r<16)break;if(skip>0x7fffffff-16)return 0;skip+=r;}return 0;}
static int run(VolumeExport*q){
 q->result=-1;q->message[0]=0;q->source[PATHLEN-1]=q->destination[PATHLEN-1]=0;char source[PATHLEN];
 if(!fs2_active||boot.recovery||boot.install_len)return message(q,-1,"Esporta dal sistema installato, senza modificare live o Recupero.");
 if(vol_normalize(q->source,source)||(!snapshot_root(source)&&!vol_exportable(source)))return message(q,-1,"Seleziona uno snapshot utente o un volume dati nativo distinto dal volume di avvio.");
 int disk=-1;const char*d=q->destination;if(!memcmp(d,"disk",4)&&d[4]>='0'&&d[4]<='9'){disk=0;for(d+=4;*d;d++){if(*d<'0'||*d>'9'||disk>100){disk=-1;break;}disk=disk*10+*d-'0';}}
 if(disk<0||disk>=sata_count())return message(q,-1,"Indica il disco di destinazione: disk0, disk1… Nessuna partizione esistente sarà formattata.");
 if(q->part.confirm==DISK_EDIT_CONFIRM&&q->part.disk!=disk)return message(q,-1,"Disco cambiato: ripeti l’anteprima.");q->part.disk=disk;
 Export*x=kmalloc(sizeof *x);if(!x)return message(q,-1,"Memoria insufficiente.");memset(x,0,sizeof *x);x->q=q;x->medium=(Medium){sata_io,sata_flush,disk,sata_size(disk),0,sata_logical_bytes(disk)};
 Source*root=add(x,source);int result=-1;u64 logical=0;if(root)result=exfat_copy((ExCopyIo){x,next,read_source,write_copy,allocate,deallocate,(u32)ticks,prepare,origin},root->id,&logical);
 if(!result){if(partition_edit(&x->medium,&q->part,1))message(q,-1,q->part.message);else{char n[24];strset(q->destination,"/Volumes/disk",PATHLEN);number(n,disk);append(q->destination,n,PATHLEN);append(q->destination,"s",PATHLEN);number(n,q->part.number);append(q->destination,n,PATHLEN);vol_refresh();message(q,0,"Nuova partizione exFAT creata, copiata e verificata. Originali e snapshot invariati. Nessun file VHD.");}}
 else if(!q->message[0])message(q,result,result==-2?"Nomi incompatibili con Windows: nessuna partizione pubblicata.":"Copia interrotta: nessun originale modificato; partizione non pubblicata.");
 while(x->source){Source*n=x->source->next;kfree(x->source);x->source=n;}kfree(x);return q->result;
}
int volume_export(VolumeExport*q){if(!q)return -1;u32 flags;rec_lock(&fs_gate,&flags);int r=run(q);rec_unlock(&fs_gate,flags);return r;}
