/* SPDX-License-Identifier: MIT
 * File-backed image devices. Memory use is independent of image size:
 * positional reads + bounded 64 KiB source cache, never whole-file loading.
 * Stable source views support native/snapshot, external and nested backing
 * without holding a generational external FD or re-entering FatFs/NTFS.
 */
#include "../kernel/k.h"
#include "image_memory.h"
#include "fs2_runtime.h"
#include "image8.h"
#include "disklayout.h"
#include "volumes.h"
#include "foreign_volume.h"
#include "image_source.h"
#include "foreignfs.h"
#include "tmcbfs2.h"
extern int block_image_add(void*,u64,int(*)(void*,u64,void*,size_t)),block_image_remove(int);
extern int vol_disk_busy(int,const Process*,int);
#define IMAGE_SOURCE_CACHE 65536u
typedef struct ImageCache {
 struct ImageCache*next;Image8 image;ImageSource *reader;u8 *cache;u64 bytes,cache_at;unsigned cache_bytes;
 int disk,active;char source[PATHLEN];
 u8 *decoded;const Image8Run*decoded_run;u64 decoded_at;unsigned decoded_bytes;
} ImageCache;
static ImageCache*caches,*probing;
int image_native_busy(u64 inode){return image_source_native_busy(inode);}
int image_native_attached(void){return image_source_attached();}
static void*image_page_alloc(void*x,size_t n){(void)x;return n==4096?(void*)page_get():NULL;}
static void image_page_free(void*x,void*p){(void)x;if(p)page_put((u64)p);}
static int cached_read(void*ctx,u64 off,void*out,size_t n){
 ImageCache*c=ctx;if(!c||!c->active||!c->reader||!out||off>c->bytes||n>c->bytes-off)return -1;
 /* fs_gate is recursive on the same kernel thread. This must NEVER run
  * while disk_lock is held: fetching source blocks may use physical disks. */
 u32 flags;rec_lock(&fs_gate,&flags);int result=-1;
 while(n){
  if(!c->cache_bytes||off<c->cache_at||off-c->cache_at>=c->cache_bytes){
   u64 base=off&~(u64)(IMAGE_SOURCE_CACHE-1);unsigned want=MIN((u64)IMAGE_SOURCE_CACHE,c->bytes-base);c->cache_bytes=0;
   if(image_source_read(c->reader,base,c->cache,want))goto done;
   c->cache_at=base;c->cache_bytes=want;
  }
  unsigned at=off-c->cache_at,take=MIN((u64)n,c->cache_bytes-at);
  memcpy(out,c->cache+at,take);off+=take;out=(u8*)out+take;n-=take;
 }result=0;
done:rec_unlock(&fs_gate,flags);return result;
}
static int decoded_read(Image8*i,const Image8Run*r,u64 off,void*out,size_t n){
 ImageCache*c=i->context;if(!image8_codec_supported(r->type))return -1;
 while(n){
  if(c->decoded_run!=r||off<c->decoded_at||off-c->decoded_at>=c->decoded_bytes){
   u64 base=off&~(u64)(IMAGE_SOURCE_CACHE-1);unsigned want=MIN((u64)IMAGE_SOURCE_CACHE,r->count*512-base);
   c->decoded_bytes=0;c->decoded_run=NULL;
   if(image8_codec_read(i,r,base,c->decoded,want))return -1;
   c->decoded_at=base;c->decoded_bytes=want;c->decoded_run=r;
  }
  unsigned at=off-c->decoded_at,take=MIN((u64)n,c->decoded_bytes-at);
  memcpy(out,c->decoded+at,take);off+=take;out=(u8*)out+take;n-=take;
 }return 0;
}
static void release_image(ImageCache*c){
 if(!c)return;ImageCache**link=&caches;while(*link&&*link!=c)link=&(*link)->next;if(*link)*link=c->next;
 if(probing==c)probing=NULL;image8_release(&c->image);image_source_close(c->reader);image_mem_free(c->decoded);image_mem_free(c->cache);image_mem_free(c);
}
static int probe_image_io(int index,u64 l,u32 n,void*p,int w){(void)index;return !probing||w||l>~(u64)0/512?-1:image8_read(&probing->image,l*512,p,(size_t)n*512);}
static int supported_image_fs(int f){return f==FS_APFS||filesystem_hfs(f)||f==FS_FAT12||f==FS_FAT16||f==FS_FAT32||f==FS_EXFAT||f==FS_NTFS||f==FS_TMC2||f==FS_ISO9660;}
typedef struct {Image8*image;u64 base,sectors;} ImageProbe;
static int probe_read(void*x,u64 l,u32 n,void*out){ImageProbe*p=x;return !n||l>p->sectors||n>p->sectors-l?-1:image8_read(p->image,(p->base+l)*512,out,(size_t)n*512);}
static void*probe_alloc(void*x,size_t n){(void)x;return image_mem_alloc(n);}
static void probe_free(void*x,void*p){(void)x;image_mem_free(p);}
static int probe_bytes(void*x,u64 at,void*out,size_t n){ImageProbe*p=x;u64 bytes=p->sectors*512;return at>bytes||n>bytes-at?-1:image8_read(p->image,p->base*512+at,out,n);}
static int probe_page(void*x,u64 block,void*out){ImageProbe*p=x;return block>=p->sectors/32?-1:probe_read(p,block*32,32,out);}
static int probe_no_write(void*x,u64 block,const void*p){(void)x;(void)block;(void)p;return -1;}
static int probe_flush(void*x){(void)x;return 0;}
static int validate_filesystem(Image8*image,u64 base,u64 sectors,int fs){
 if(!supported_image_fs(fs))return 0;
 ImageProbe view={image,base,sectors};ForeignIo io={.context=&view,.sectors=sectors,.logical_bytes=512,.read=probe_read,.alloc=probe_alloc,.free=probe_free};ForeignEntry root;
 if(fs==FS_APFS)return apfs_volume_validate(&io)==0;
 if(filesystem_hfs(fs))return hfs_volume_stat(&io,"/",&root)==0&&root.dir;
 if(fs==FS_ISO9660)return iso_volume_stat(&io,"/",&root)==0&&root.dir;
 if(fs==FS_TMC2){
  Tmc2*t=image_mem_alloc(sizeof *t);if(!t)return 0;memset(t,0,sizeof *t);T2Entry entry;
  int mounted=tmc2_mount(t,(T2Io){&view,sectors/32,probe_page,probe_no_write,probe_flush,probe_alloc,probe_free})==0;
  int valid=mounted&&!tmc2_stat(t,1,&entry)&&entry.kind==T2_DIR;if(mounted)tmc2_close(t);image_mem_free(t);return valid;
 }
 ForeignFs*f=image_mem_alloc(sizeof *f);if(!f)return 0;FvNode entry;
 int valid=!fv_mount(f,&view,sectors*512,probe_bytes)&&f->type==fs&&!fv_root(f,&entry);image_mem_free(f);return valid;
}
static int message_image(ImageRequest*q,int result,const char*text){strset(q->message,text,sizeof q->message);return result;}
int fs_image_manage(Process*p,ImageRequest*q){
 if(!p||!q||q->operation<IMAGE_ATTACH||q->operation>IMAGE_NEXT||q->path[PATHLEN-1])return -1;
 u32 flags;rec_lock(&fs_gate,&flags);int result=-1;
 if(q->operation==IMAGE_NEXT){
  ImageCache*found=NULL;for(ImageCache*c=caches;c;c=c->next)if(c->active&&c->disk>=0&&c->disk>q->disk&&(!found||c->disk<found->disk))found=c;
  if(found){q->disk=found->disk;strset(q->path,found->source,PATHLEN);result=message_image(q,0,"Immagine file-backed, sola lettura.");}goto done;
 }
 if(q->operation!=IMAGE_ATTACH){
  ImageCache*c=caches;while(c&&(!c->active||c->disk!=q->disk))c=c->next;
  if(!c){message_image(q,-1,"Immagine non collegata.");goto done;}
  if(q->operation==IMAGE_INFO){strset(q->path,c->source,PATHLEN);result=message_image(q,0,"Immagine file-backed, sola lettura; cache limitata.");goto done;}
  if(vol_disk_busy(c->disk,proc,NPROC)){message_image(q,-1,"Immagine in uso: chiudi i documenti aperti.");goto done;}
  if(block_image_remove(c->disk)){message_image(q,-1,"Impossibile scollegare l'immagine.");goto done;}
  vol_refresh();release_image(c);result=message_image(q,0,"Immagine scollegata.");goto done;
 }
 Dirent file;if(q->path[0]!='/'||fs_stat(q->path,&file)||file.dir||file.size<512||file.size>IMAGE8_ADDRESS_LIMIT){message_image(q,-1,"File immagine non valido o oltre il limite di indirizzamento del sistema.");goto done;}
 const char*error="Memoria insufficiente per collegare l’immagine.";
 ImageCache*c=image_mem_alloc(sizeof *c);if(!c){message_image(q,-1,error);goto done;}memset(c,0,sizeof *c);c->bytes=file.size;c->disk=-1;strset(c->source,q->path,PATHLEN);
 c->next=caches;caches=c;c->cache=image_mem_alloc(IMAGE_SOURCE_CACHE);c->decoded=image_mem_alloc(IMAGE_SOURCE_CACHE);u8*scratch=image_mem_alloc(65536);
 if(!c->cache||!c->decoded||!scratch)goto load_failed;
 error="Sorgente non disponibile o in scrittura: chiudi i writer; il volume sorgente deve restare collegato per tutta la durata del mount.";
 c->reader=image_source_open(p,q->path,file.size);if(!c->reader)goto load_failed;c->active=1;
 error="Contenitore non supportato o corrotto: RAW/ISO o UDIF v4 a segmento singolo con XML, plist binario o resource fork.";
 if(image8_decode_alloc(&c->image,c,file.size,cached_read,scratch,65536,decoded_read,image_page_alloc,image_page_free))goto load_failed;
 error="Codec di compressione DMG non supportato.";
 for(u64 r=0;r<c->image.runs;r++)if(image8_run(&c->image,r)->type>=IMAGE8_ADC&&!image8_codec_supported(image8_run(&c->image,r)->type))goto load_failed;
 error="Nessun filesystem navigabile supportato, metadati corrotti, I/O o risorse decoder insufficienti. APFS cifrato/sealed e file speciali non supportati.";
 probing=c;
 Medium medium={.io=probe_image_io,.d=0,.z=c->image.bytes/512,.logical_bytes=512};
 int valid=validate_filesystem(&c->image,0,medium.z,filesystem_probe(&medium,0,medium.z));
 if(!valid){DiskMap*map=image_mem_alloc(sizeof *map);if(!map)goto load_failed;
  if(partition_scan_image(&medium,map)&&map->safe)for(int j=0;j<map->count;j++)if(validate_filesystem(&c->image,map->part[j].start,map->part[j].sectors,map->part[j].fs))valid=1;image_mem_free(map);
 }
 if(!valid)goto load_failed;probing=NULL;
 error="Registro dischi pieno: impossibile collegare un’altra immagine.";
 c->disk=block_image_add(&c->image,c->image.bytes,image8_read);if(c->disk<0)goto load_failed;
 q->disk=c->disk;strset(q->path,"/Volumes",PATHLEN);vol_refresh();image_mem_free(scratch);
 result=message_image(q,0,"Immagine collegata in sola lettura. Volumi disponibili nel Finder.");goto done;
load_failed:
 image_mem_free(scratch);release_image(c);
 message_image(q,-1,error);
done:rec_unlock(&fs_gate,flags);return result;
}
