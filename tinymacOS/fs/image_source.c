/* SPDX-License-Identifier: MIT
 * Stable read-only image backing independent of external FD generations.
 * ForeignFs is the reentrant, read-only metadata reader, NOT singleton FatFs
 * or NTFS native state. Apple/ISO readers use caller-owned local state.
 * Source partition geometry is pinned by vol_source_open across inventory
 * refresh, writes, logical unmount and eject. Nested images form an immutable
 * DAG because an attachment can only reference already-published parents;
 * parent detach is blocked by the retained source view.
 */
#include "../kernel/k.h"
#include "image_memory.h"
#include "fs2_runtime.h"
#include "foreign_volume.h"
#include "foreignfs.h"
#include "tmcbfs2.h"
#include "image_source.h"
struct ImageSource {
 struct ImageSource*next;Process*reader;ForeignIo io;ForeignFs*foreign;Tmc2*native;
 FvNode node;u64 bytes,inode;int type,disk,reading;void*stack;char path[PATHLEN];
};
#define SOURCE_STACK_BYTES 65536u
extern i64 image_source_stack_call(void*,i64(*)(void*),void*);
static ImageSource*sources;
int image_source_native_busy(u64 id){for(ImageSource*s=sources;s;s=s->next)if(s->reader&&s->reader->fd[0].backend==2&&s->reader->fd[0].inode==id)return 1;return 0;}
int image_source_snapshot_busy(u64 id){for(ImageSource*s=sources;s;s=s->next)if(s->reader&&s->reader->fd[0].backend==3&&s->reader->fd[0].snapshot==id)return 1;return 0;}
int image_source_attached(void){for(ImageSource*s=sources;s;s=s->next)if(s->reader&&s->reader->fd[0].backend==2)return 1;return 0;}
static int source_bytes(void*context,u64 off,void*out,size_t n){
 ImageSource*s=context;if(off>s->io.sectors*512||n>s->io.sectors*512-off)return -1;
 while(n){u8 b[512];unsigned part=off%512,take=MIN((u64)n,512-part);
  if(!part&&n>=512){unsigned blocks=MIN((u64)n/512,128);take=blocks*512;if(s->io.read(s->io.context,off/512,blocks,out))return -1;}
  else{if(s->io.read(s->io.context,off/512,1,b))return -1;memcpy(out,b+part,take);}
  out=(u8*)out+take;off+=take;n-=take;
 }return 0;
}
static int source_page(void*x,u64 block,void*p){ImageSource*s=x;return block>=s->io.sectors/32?-1:s->io.read(s->io.context,block*32,32,p);}
static int no_write(void*x,u64 block,const void*p){(void)x;(void)block;(void)p;return -1;}
static int source_flush(void*x){ImageSource*s=x;return s->io.flush?s->io.flush(s->io.context):0;}
static void*source_alloc(void*x,size_t n){ImageSource*s=x;return s->io.alloc(s->io.context,n);}
static void source_free(void*x,void*p){ImageSource*s=x;s->io.free(s->io.context,p);}
static int resolve_native(ImageSource*s){
 T2Entry e;if(tmc2_stat(s->native,1,&e))return -1;const char*p=s->path;
 while(*p){while(*p=='/')p++;if(!*p)break;char name[T2_NAME];unsigned n=0;while(*p&&*p!='/'){if(n>=sizeof name-1)return -1;name[n++]=*p++;}name[n]=0;
  if(e.kind!=T2_DIR||tmc2_lookup(s->native,e.key,name,&e))return -1;
 }if(e.kind!=T2_FILE||e.size!=s->bytes)return -1;s->inode=e.key;return 0;
}
static int resolve_foreign(ImageSource*s){
 if(fv_mount(s->foreign,s,s->io.sectors*512,source_bytes)||fv_root(s->foreign,&s->node))return -1;const char*p=s->path;
 while(*p){while(*p=='/')p++;if(!*p)break;char name[48];unsigned n=0;while(*p&&*p!='/'){if(n>=sizeof name-1)return -1;name[n++]=*p++;}name[n]=0;FvNode next;
  if(!s->node.dir||fv_lookup(s->foreign,&s->node,name,&next))return -1;s->node=next;
 }return s->node.dir||s->node.size!=s->bytes?-1:0;
}
static void destroy_source(ImageSource*s){
 if(!s)return;if(s->native){tmc2_close(s->native);image_mem_free(s->native);}image_mem_free(s->foreign);
 if(s->io.context)vol_source_close(&s->io);image_mem_free(s->reader);image_mem_free(s->stack);image_mem_free(s);
}
ImageSource*image_source_open(Process*p,const char*path,u64 bytes){
 if(!p||!path)return NULL;ImageSource*s=image_mem_alloc(sizeof *s);if(!s)return NULL;memset(s,0,sizeof *s);s->bytes=bytes;s->disk=-1;s->stack=image_mem_alloc(SOURCE_STACK_BYTES);if(!s->stack)goto bad;
 int fd=fs_open(p,path,0);if(fd<0)goto bad;FD handle=p->fd[fd];fs_close(p,fd);
 if(handle.backend==0||handle.backend==2||handle.backend==3){
  if(handle.backend==2)for(int pi=0;pi<NPROC;pi++)for(int f=0;f<12;f++){FD*other=&proc[pi].fd[f];if(other->id&&other->write&&other->backend==2&&other->inode==handle.inode)goto bad;}
  s->reader=image_mem_alloc(sizeof *s->reader);if(!s->reader)goto bad;memset(s->reader,0,sizeof *s->reader);handle.at=0;handle.write=0;s->reader->fd[0]=handle;
 }else{
  if(vol_source_open(path,proc,NPROC,&s->io,s->path,&s->type,&s->disk))goto bad;
  if(s->type==FS_TMC2){
   s->native=image_mem_alloc(sizeof *s->native);if(!s->native)goto bad;memset(s->native,0,sizeof *s->native);
   if(tmc2_mount(s->native,(T2Io){s,s->io.sectors/32,source_page,no_write,source_flush,source_alloc,source_free})||resolve_native(s))goto bad;
  }else if(s->type==FS_FAT12||s->type==FS_FAT16||s->type==FS_FAT32||s->type==FS_EXFAT||s->type==FS_NTFS){
   s->foreign=image_mem_alloc(sizeof *s->foreign);if(!s->foreign||resolve_foreign(s))goto bad;
  }else if(s->type!=FS_ISO9660&&s->type!=FS_APFS&&!filesystem_hfs(s->type))goto bad;
 }
 s->next=sources;sources=s;return s;
bad:destroy_source(s);return NULL;
}
typedef struct {ImageSource*s;u64 at;void*out;size_t n;} SourceRead;
static i64 source_read_stack(void*arg){
 SourceRead*r=arg;ImageSource*s=r->s;u64 at=r->at;void*out=r->out;size_t n=r->n;i64 got=-1;
 if(s->reader){if(n<=0x7fffffff&&!fs2_seek(s->reader,0,at))got=fs2_read(s->reader,0,out,n);}
 else if(s->native)got=tmc2_read(s->native,s->inode,at,out,n);
 else if(s->foreign)got=fv_read(s->foreign,&s->node,at,out,n);
 else if(s->type==FS_ISO9660)got=iso_volume_read(&s->io,s->path,at,out,n);
 else if(s->type==FS_APFS)got=apfs_volume_read(&s->io,s->path,at,out,n);
 else if(filesystem_hfs(s->type))got=hfs_volume_read(&s->io,s->path,at,out,n);
 return got;
}
int image_source_read(ImageSource*s,u64 at,void*out,size_t n){
 /* Each retained source owns its stack, so dependency depth consumes real
  * memory, not recursive space on the 16 KiB syscall stack. A per-source
  * active guard rejects actual cycles; there is no fixed nesting count. */
 if(!s||!out||at>s->bytes||n>s->bytes-at||s->reading)return -1;
 SourceRead r={s,at,out,n};s->reading=1;
 i64 got=image_source_stack_call((u8*)s->stack+SOURCE_STACK_BYTES,source_read_stack,&r);
 s->reading=0;return got==(i64)n?0:-1;
}
void image_source_close(ImageSource*s){ImageSource**link=&sources;while(*link){if(*link==s){*link=s->next;destroy_source(s);return;}link=&(*link)->next;}}
