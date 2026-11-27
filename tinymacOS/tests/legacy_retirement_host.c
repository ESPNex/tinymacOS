/* Actual mount/live-session backend: unknown disks must never become RAM copies
 * or writable legacy mounts. All media here is disposable host memory. */
#include <setjmp.h>
#define main baseline_runtime_main
#include "fs2_runtime_host.c"
#undef main
static int disk_writes,encrypted,locked,stop_expected;static jmp_buf stopped;
void retail_stop(void){assert(stop_expected);longjmp(stopped,1);}
void*kmalloc(size_t n){return malloc(n);}void kfree(void*p){free(p);}int kmalloc_owns(const void*p){(void)p;return 1;}
u64 page_get(void){return (u64)calloc(1,4096);}void page_put(u64 p){free((void*)p);}
void boot_logstr(const char*p){(void)p;}void boot_lognum(u64 n){(void)n;}
void device_add(const char*a,const char*b,void*c){(void)a;(void)b;(void)c;}
int volume_mount(Block*b){*b=(Block){0};return 1;}int volume_current(void){return 0;}
int volume_extent(u64*b,u64*n){*b=0;*n=4096*T2_BLOCK/512;return 0;}
int sata_flush(int d){assert(!d);return 0;}
int sata_io(int d,u64 l,u32 n,void*p,int wr){assert(!d&&l+n<=4096*T2_BLOCK/512);if(wr){disk_writes++;memcpy(disk+l*512,p,n*512);}else memcpy(p,disk+l*512,n*512);return 0;}
int fs2_volume_encrypted(int d,u64 b,u64 n){(void)b;(void)n;assert(!d);return encrypted;}
int fs2_volume_locked(int d){assert(!d);return locked;}
u64 fs2_volume_blocks(int d,u64 b,u64 n){(void)d;(void)b;(void)n;return 4096;}
int fs2_volume_read(int d,u64 b,u64 n,u64 at,void*p){(void)d;(void)b;(void)n;return locked?-1:read_page(0,at,p);}
int fs2_volume_write(int d,u64 b,u64 n,u64 at,const void*p){(void)d;(void)b;(void)n;if(locked)return -1;disk_writes++;return write_page(0,at,p);}
static void close_runtime(void){if(fs2_active)tmc2_close(&fs);fs2_active=0;for(unsigned b=0;b<LIVE_BLOCKS;b++)for(unsigned i=0;i<4;i++)if(live_pages[b][i]){page_put(live_pages[b][i]);live_pages[b][i]=0;}}
int main(void){
 disk=calloc(4096,T2_BLOCK);assert(disk);u8 before[32],after[32];
 const char*magic[]={"TMCBFS1","TINYFS1","UNKNOWN!","TMCBFS2"};
 for(unsigned j=0;j<COUNT(magic);j++){
  memset(disk,0,4096*T2_BLOCK);memcpy(disk,magic[j],8);sha_bytes(disk,4096*T2_BLOCK,before);int writes_before=disk_writes;
  stop_expected=1;if(!setjmp(stopped)){fs2_mount();assert(!"unsupported/corrupt installed filesystem mounted");}stop_expected=0;
  sha_bytes(disk,4096*T2_BLOCK,after);assert(!memcmp(before,after,32)&&disk_writes==writes_before&&!persistent&&!fs2_active);
 }
 for(unsigned recovery=0;recovery<2;recovery++){
  boot.recovery=recovery;boot.install_len=!recovery;int w=disk_writes;sha_bytes(disk,4096*T2_BLOCK,before);assert(fs2_mount()&&fs2_active&&!persistent);
  Dirent e;assert(!fs2_stat(DOCS,&e)&&e.dir);assert(!fs2_create(DOCS "/live.txt",0));Process p={0};int fd=fs2_open(&p,DOCS "/live.txt",1);assert(fd>=0&&fs2_write(&p,fd,"RAM_TMCFS2",10)==10&&!fs2_close(&p,fd));
  u8*archive=malloc(3<<20);assert(archive&&!fs2_snapshot(archive,3<<20)&&!memcmp(archive,"TMCSEED2",8)&&!tmc2_seed_validate(archive,3<<20));free(archive);
  assert(!fs2_snap_create("live-snapshot")&&!fs2_snap_verify("live-snapshot"));sha_bytes(disk,4096*T2_BLOCK,after);assert(!memcmp(before,after,32)&&disk_writes==w);close_runtime();
 }
 /* Valid installed TMCFS2 remains mountable; lock UI gets a separate fresh
  * volatile TMCFS2, not the real account/content or a generation-1 fallback. */
 boot=(struct Boot){0};memset(disk,0,4096*T2_BLOCK);assert(!tmc2_format(&fs,(T2Io){0,4096,read_page,write_page,sync_page,allocate,deallocate}));tmc2_close(&fs);
 assert(fs2_mount()&&persistent);close_runtime();encrypted=locked=1;int w=disk_writes;assert(fs2_mount()&&!persistent&&medium.disk==0&&disk_writes==w);locked=0;assert(fs2_mount()&&persistent&&disk_writes==w);close_runtime();
 free(disk);puts("PASS TMCFS2-only actual runtime: TMCBFS1/TINYFS1/unknown/corrupt native boot refuse with identical disk SHA and zero writes; live/Recovery use real volatile TMCFS2 + new transport; installed v2 and locked/unlock remount work without legacy fallback");
}
