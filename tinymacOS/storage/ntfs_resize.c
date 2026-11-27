/* SPDX-License-Identifier: GPL-2.0-or-later
 * Freestanding ntfsresize adapter. The real, security-patched upstream engine
 * is linked in a private symbol namespace; no Linux ELF/libc/syscalls execute.
 * Only the caller-selected partition view is addressable. fs_gate serializes
 * this workspace and the caller MUST reject every mounted disk beforehand.
 */
#include "disklayout.h"
#include "abi.h"
#include "date.h"
#include "../compat/ntfs/native_abi.h"
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#define NT_HEAP (32u*1024*1024)
#define NT_CHUNK 128u
static u8 arena[NT_HEAP] ALIGNED(16);
typedef struct Allocation {size_t size;struct Allocation*next;u64 free,magic;} Allocation;
#define ALLOC_MAGIC 0x4e544653414c4c4full
static Allocation*first;
static struct {Medium*m;u64 start,extent;u64 position,epoch;int dry,error,io_failed,wrote,write_attempts;size_t high;void*jmp[5];} ctx;
static u8 verify[NT_CHUNK*512] ALIGNED(512);
static char report[4096];static size_t reportused;
extern int ntp_tmc_ntfsresize_run(unsigned long long,int);
extern int ntp_vsnprintf(char*,size_t,const char*,va_list);
static void report_add(const char*s){
 size_t n=strlen(s);if(n>=sizeof report){s+=n-sizeof(report)+1;n=sizeof(report)-1;}
 if(reportused+n>=sizeof report){size_t keep=sizeof report/2;memmove(report,report+reportused-MIN(keep,reportused),MIN(keep,reportused));reportused=MIN(keep,reportused);}
 memcpy(report+reportused,s,n);reportused+=n;report[reportused]=0;
}
const char*ntfs_resize_log(void){return report;}
int*ntp___errno_location(void){return &ctx.error;}
void*ntp_memcpy(void*d,const void*s,size_t n){return memcpy(d,s,n);}
void*ntp_memmove(void*d,const void*s,size_t n){return memmove(d,s,n);}
void*ntp_memset(void*d,int c,size_t n){return memset(d,c,n);}
int ntp_memcmp(const void*a,const void*b,size_t n){return memcmp(a,b,n);}
size_t ntp_strlen(const char*s){return strlen(s);}
char*ntp_strchr(const char*s,int c){do{if(*s==(char)c)return (char*)s;}while(*s++);return 0;}
char*ntp_strrchr(const char*s,int c){const char*p=0;do{if(*s==(char)c)p=s;}while(*s++);return (char*)p;}
int ntp_strcmp(const char*a,const char*b){while(*a&&*a==*b)a++,b++;return (u8)*a-(u8)*b;}
int ntp_ffs(int n){return n?__builtin_ffs(n):0;}
void*ntp_malloc(size_t n){
 if(!n)n=1;if(n>NT_HEAP-sizeof(Allocation)-16){ctx.error=ENOMEM;return 0;}n=(n+15)&~15ull;
 for(Allocation*h=first;h;h=h->next){
  if(h->magic!=ALLOC_MAGIC){ctx.error=EFAULT;return 0;}
  if(!h->free||h->size<n)continue;
  if(h->size>=n+sizeof(Allocation)+16){Allocation*t=(void*)((u8*)(h+1)+n);*t=(Allocation){h->size-n-sizeof *t,h->next,1,ALLOC_MAGIC};h->next=t;h->size=n;}
  h->free=0;size_t end=(u8*)(h+1)+h->size-arena;if(end>ctx.high)ctx.high=end;return h+1;
 }
 ctx.error=ENOMEM;return 0;
}
void ntp_free(void*p){
 if(!p)return;
 if((uintptr_t)p<(uintptr_t)(arena+sizeof(Allocation))||(uintptr_t)p>=(uintptr_t)(arena+NT_HEAP)||((uintptr_t)p&15)){ctx.error=EFAULT;return;}
 Allocation*h=(Allocation*)p-1;if(h->magic!=ALLOC_MAGIC||h->free){ctx.error=EFAULT;return;}h->free=1;
 for(h=first;h&&h->next;){if(h->free&&h->next->free){h->size+=sizeof *h+h->next->size;h->next=h->next->next;}else h=h->next;}
}
void*ntp_calloc(size_t a,size_t b){if(b&&a>NT_HEAP/b){ctx.error=ENOMEM;return 0;}void*p=ntp_malloc(a*b);if(p)memset(p,0,a*b);return p;}
void*ntp_realloc(void*p,size_t n){
 if(!p)return ntp_malloc(n);if(!n){ntp_free(p);return 0;}Allocation*h=(Allocation*)p-1;
 if((u8*)h<arena||(u8*)h>arena+NT_HEAP-sizeof *h||h->magic!=ALLOC_MAGIC||h->free){ctx.error=EFAULT;return 0;}
 void*q=ntp_malloc(n);if(!q)return 0;memcpy(q,p,MIN(n,h->size));ntp_free(p);return q;
}
char*ntp_strdup(const char*s){size_t n=strlen(s)+1;char*p=ntp_malloc(n);if(p)memcpy(p,s,n);return p;}
void*ntp_stdout=(void*)1;void*ntp_stderr=(void*)2;
int ntp_fflush(void*f){(void)f;return 0;}
int ntp_vfprintf(void*f,const char*fmt,va_list a){(void)f;char text[768];int n=ntp_vsnprintf(text,sizeof text,fmt,a);report_add(text);return n;}
int ntp_fprintf(void*f,const char*fmt,...){va_list a;va_start(a,fmt);int n=ntp_vfprintf(f,fmt,a);va_end(a);return n;}
int ntp_printf(const char*fmt,...){va_list a;va_start(a,fmt);int n=ntp_vfprintf(0,fmt,a);va_end(a);return n;}
char*ntp_strerror(int e){switch(e){case ENOMEM:return "Insufficient resize memory";case EIO:return "Block I/O error";case EINVAL:return "Invalid NTFS metadata or argument";case EPERM:return "Unsafe/hibernated volume";case EOPNOTSUPP:return "Unsupported NTFS state";case ENOSPC:return "Insufficient space";default:return "NTFS operation failed";}}
__attribute__((noreturn)) void ntp_exit(int code){ctx.error=code?ctx.error?ctx.error:EIO:0;__builtin_longjmp(ctx.jmp,1);}
int ntp_clock_gettime(clockid_t id,struct timespec*t){if(id!=CLOCK_REALTIME||!t){ctx.error=EINVAL;return -1;}t->tv_sec=ctx.epoch;t->tv_nsec=0;return 0;}
static i64 transfer(i64 pos,i64 bytes,void*p,int write){
 u64 limit=(u64)ctx.extent*512;
 if(pos<0||bytes<0||(u64)pos>limit||(u64)bytes>limit-(u64)pos){ctx.error=EINVAL;ctx.io_failed=1;return -1;}
 if(write){ctx.write_attempts++;if(ctx.dry){ctx.error=EROFS;return -1;}}
 i64 done=0;
 while(done<bytes){
  u64 sector=(pos+done)/512;u32 off=(pos+done)%512,n;
  if(off||bytes-done<512){
   u8 b[512] ALIGNED(512);n=MIN((i64)(512-off),bytes-done);
   if(ctx.m->io(ctx.m->d,ctx.start+sector,1,b,0))goto bad;
   if(write){memcpy(b+off,(u8*)p+done,n);ctx.wrote=1;if(ctx.m->io(ctx.m->d,ctx.start+sector,1,b,1)||ctx.m->flush(ctx.m->d)||ctx.m->io(ctx.m->d,ctx.start+sector,1,verify,0)||memcmp(b,verify,512))goto bad;}
   else memcpy((u8*)p+done,b+off,n);
  }else{
   u32 blocks=MIN((bytes-done)/512,NT_CHUNK);n=blocks*512;
   if(write)ctx.wrote=1;
   if(ctx.m->io(ctx.m->d,ctx.start+sector,blocks,(u8*)p+done,write))goto bad;
   if(write&&(ctx.m->flush(ctx.m->d)||ctx.m->io(ctx.m->d,ctx.start+sector,blocks,verify,0)||memcmp((u8*)p+done,verify,n)))goto bad;
  }
  done+=n;
 }
 return done;
bad:ctx.error=EIO;ctx.io_failed=1;return -1;
}
static int devopen(NativeNtfsDevice*d,int flags){if(d->d_state&1){ctx.error=EBUSY;return -1;}d->d_state|=1|8;if((flags&O_ACCMODE)==O_RDONLY||ctx.dry)d->d_state|=2;return 0;}
static int devclose(NativeNtfsDevice*d){if(!ctx.dry&&ctx.m->flush(ctx.m->d)){ctx.error=EIO;ctx.io_failed=1;return -1;}d->d_state&=~1ul;return 0;}
static i64 devseek(NativeNtfsDevice*d,i64 pos,int whence){(void)d;i64 base=whence==0?0:whence==1?ctx.position:whence==2?(u64)ctx.extent*512:-1;
 if(base<0||(pos>0&&pos>0x7fffffffffffffffll-base)||(pos<0&&pos< -base)||base+pos>(u64)ctx.extent*512){ctx.error=EINVAL;return -1;}
 ctx.position=base+pos;return ctx.position;}
static i64 devpread(NativeNtfsDevice*d,void*p,i64 n,i64 pos){(void)d;return transfer(pos,n,p,0);}
static i64 devpwrite(NativeNtfsDevice*d,const void*p,i64 n,i64 pos){if(d->d_state&2){ctx.error=EROFS;return -1;}return transfer(pos,n,(void*)p,1);}
static i64 devread(NativeNtfsDevice*d,void*p,i64 n){i64 done=devpread(d,p,n,ctx.position);if(done>0)ctx.position+=done;return done;}
static i64 devwrite(NativeNtfsDevice*d,const void*p,i64 n){i64 done=devpwrite(d,p,n,ctx.position);if(done>0)ctx.position+=done;return done;}
static int devsync(NativeNtfsDevice*d){(void)d;if(ctx.dry)return 0;if(ctx.m->flush(ctx.m->d)){ctx.io_failed=1;ctx.error=EIO;return -1;}return 0;}
static int devstat(NativeNtfsDevice*d,struct stat*s){(void)d;memset(s,0,sizeof *s);s->st_mode=S_IFBLK|0600;s->st_size=(u64)ctx.extent*512;s->st_blksize=512;s->st_blocks=ctx.extent;return 0;}
static int devioctl(NativeNtfsDevice*d,unsigned long req,void*p){(void)d;if(!p){ctx.error=EINVAL;return -1;}
 switch(req){case 0x80081272:*(u64*)p=(u64)ctx.extent*512;return 0;case 0x1260:*(unsigned long*)p=ctx.extent;return 0;case 0x1268:*(int*)p=512;return 0;
 case 0x40081271:{size_t n=*(size_t*)p;if(n>=512&&n<=2097152&&!(n&(n-1)))return 0;break;}
 case 0x301:{struct {u8 heads,sectors;u16 cylinders;unsigned long start;}*g=p;memset(g,0,sizeof *g);g->heads=255;g->sectors=63;g->cylinders=ctx.extent/(255*63);g->start=ctx.start;return 0;}}
 ctx.error=ENOTTY;return -1;
}
NativeNtfsOps ntp_ntfs_device_unix_io_ops={devopen,devclose,devseek,devread,devwrite,devpread,devpwrite,devsync,devstat,devioctl};
static u64 utc_now(void){
#ifdef TMC_NTFS_HOST
 return 1795000000;
#else
 extern void clock_info(SysInfo*);SysInfo s;clock_info(&s);
 if(s.year<2020||s.year>2200||s.month<1||s.month>12||!s.day||s.day>31)return 0;
 u64 days=0;for(int y=1970;y<(int)s.year;y++)days+=365+(monthdays(y,2)==29);
 for(int m=1;m<(int)s.month;m++)days+=monthdays(s.year,m);
 return (days+s.day-1)*86400+s.hour*3600+s.minute*60+s.second;
#endif
}
int ntfs_resize_native(Medium*m,DiskEdit*q,int apply){
 if(!q)return DE_INVALID;
 u64 extent=MAX(q->sectors,q->size);
 if(!m||!m->flush||!q->sectors||!q->size||q->start>=m->z||extent>m->z-q->start)return DE_INVALID;
 reportused=0;report[0]=0;
 u8 bp[512];
 if(m->io(m->d,q->start,1,bp,0))return DE_IO;
 if(memcmp(bp+3,"NTFS    ",8)||bp[510]!=0x55||bp[511]!=0xaa||bp[11]!=0||bp[12]!=2){report_add("Invalid primary NTFS boot sector or unsupported sector size. No writes.\n");return DE_PROTECTED;}
 memset(&ctx,0,sizeof ctx);ctx.m=m;ctx.start=q->start;ctx.extent=extent;ctx.dry=!apply;ctx.epoch=utc_now();
 reportused=0;report[0]=0;first=(void*)arena;*first=(Allocation){NT_HEAP-sizeof *first,0,1,ALLOC_MAGIC};
 int result;
 if(!__builtin_setjmp(ctx.jmp)){result=ntp_tmc_ntfsresize_run((u64)q->size*512,!apply);if(result&&!ctx.error)ctx.error=EIO;else if(!result)ctx.error=0;}
 int error=ctx.error,failed=ctx.io_failed,wrote=ctx.wrote;
 /* Best effort flush after errors; never clear the dirty flag or claim rollback. */
 if(apply&&m->flush(m->d))failed=1;
 if(ctx.high)erase(arena,ctx.high);first=0;
 if(failed||(apply&&wrote&&error))return DE_IO;
 if(error)return error==ENOSPC?DE_OCCUPIED:error==ENOMEM?DE_UNSUPPORTED:DE_PROTECTED;
 return DE_OK;
}
