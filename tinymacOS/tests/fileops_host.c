/* Real userspace transfer engine driving the actual mounted FS2 backend. */
#define main previous_runtime_main
#include "fs2_runtime_host.c"
#undef main
#include "../userspace/ui.h"
#undef sc
static long hostcall(long,u64,u64,u64,u64);
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/ui.c"
#include "../userspace/fileops.c"
static Process client;
static int copy_writes,fail_copy=-1;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){
 switch(n){
 case S_STAT:return fs2_stat((void*)a,(void*)b);
 case S_LIST:return fs2_list((void*)a,(void*)b,c,d);
 case S_CREATE:return fs2_create((void*)a,b);
 case S_REMOVE:return fs2_remove((void*)a,b);
 case S_MOVE:return fs2_move((void*)a,(void*)b);
 case S_OPEN:return fs2_open(&client,(void*)a,b);
 case S_CLOSE:return fs2_close(&client,a);
 case S_READ:assert(c<=8192);return fs2_read(&client,a,(void*)b,c);
 case S_WRITE:assert(c<=8192);if(fail_copy>=0&&copy_writes++==fail_copy)return -1;return fs2_write(&client,a,(void*)b,c);
 default:assert(!"unexpected file operation syscall");return -1;
 }
}
static void same(const char*path,const u8*data,size_t size){Dirent e;assert(!fs2_stat(path,&e)&&e.size==size);int fd=fs2_open(&client,path,0);assert(fd>=0);u8 b[8192];for(size_t at=0;at<size;){size_t n=MIN(sizeof b,size-at);assert(fs2_read(&client,fd,b,n)==(int)n&&!memcmp(data+at,b,n));at+=n;}assert(!fs2_close(&client,fd));}
int main(void){
 disk=calloc(4096,T2_BLOCK);assert(disk);assert(!tmc2_format(&fs,(T2Io){0,4096,read_page,write_page,sync_page,allocate,deallocate}));fs2_active=1;u64 users=node(1,"Users",T2_DIR),home=node(users,"User",T2_DIR);node(home,"Documents",T2_DIR);node(home,"Desktop",T2_DIR);node(home,"Downloads",T2_DIR);
 assert(!fs2_create(DOCS "/tree",1)&&!fs2_create(DOCS "/tree/nested",1));static u8 data[262144];for(unsigned j=0;j<sizeof data;j++)data[j]=(j*19+53)%251;
 assert(!fs2_create(DOCS "/tree/nested/large.bin",0));int fd=fs2_open(&client,DOCS "/tree/nested/large.bin",1);assert(fd>=0);for(unsigned at=0;at<sizeof data;at+=8192)assert(fs2_write(&client,fd,data+at,8192)==8192);assert(!fs2_close(&client,fd));assert(!fs2_create(DOCS "/tree/.dotfile",0));
 for(unsigned j=0;j<1100;j++){char p[PATHLEN];snprintf(p,sizeof p,DOCS "/tree/file-%04u",j);assert(!fs2_create(p,0));}
 char placed[PATHLEN];assert(!fileop_place(DOCS "/tree",HOME "/Downloads",0,placed)&&eq(placed,HOME "/Downloads/tree"));same(HOME "/Downloads/tree/nested/large.bin",data,sizeof data);Dirent e;assert(!fs2_stat(HOME "/Downloads/tree/file-1099",&e)&&!fs2_stat(HOME "/Downloads/tree/.dotfile",&e));
 assert(fileop_copy(DOCS "/tree",HOME "/Downloads/tree")==FILEOP_EXISTS);same(HOME "/Downloads/tree/nested/large.bin",data,sizeof data);
 assert(fileop_place(DOCS "/tree",DOCS "/tree/nested",0,placed)==FILEOP_CYCLE);assert(fileop_copy(DOCS "/tree","/Volumes/Macintosh HD" DOCS "/tree/recursive")==FILEOP_CYCLE);assert(fileop_place(DOCS "/tree",DOCS,1,placed)==FILEOP_CYCLE);
 assert(!fileop_place(DOCS "/tree/nested/large.bin",DOCS "/tree/nested",0,placed)&&eq(placed,DOCS "/tree/nested/Copia large.bin"));same(placed,data,sizeof data);assert(!fileop_place(DOCS "/tree/nested/large.bin",DOCS "/tree/nested",0,placed)&&eq(placed,DOCS "/tree/nested/Copia 2 large.bin"));same(placed,data,sizeof data);
 assert(!fileop_place(DOCS "/tree",HOME "/Desktop",1,placed)&&!fs2_stat(HOME "/Desktop/tree/file-1099",&e)&&fs2_stat(DOCS "/tree",&e)<0);same(HOME "/Desktop/tree/nested/large.bin",data,sizeof data);
 assert(fileop_place(HOME "/Desktop/tree","/System",0,placed)<0);
 puts("PASS actual transfer engine + FS2: recursive 1100-file tree, dotfiles, streamed byte-exact 256KiB, exclusive copies, numbered collisions, atomic folder moves, same-path/descendant/volume-alias cycle refusal and read-only destination refusal");
 for(int cut=0;cut<32;cut++){copy_writes=0;fail_copy=cut;assert(fileop_copy(HOME "/Downloads/tree/nested/large.bin",DOCS "/partial.bin")<0);assert(fs2_stat(DOCS "/partial.bin",&e)<0);same(HOME "/Downloads/tree/nested/large.bin",data,sizeof data);for(int j=0;j<12;j++)assert(!client.fd[j].id);}
 fail_copy=-1;assert(!fileop_copy(HOME "/Downloads/tree/nested/large.bin",DOCS "/after-errors.bin"));same(DOCS "/after-errors.bin",data,sizeof data);
 puts("PASS every one of 32 streamed-write failures closes both FDs, cleans partial destination and retains byte-identical source; retry succeeds");
 tmc2_close(&fs);assert(!tmc2_mount(&fs,(T2Io){0,4096,read_page,write_page,sync_page,allocate,deallocate}));same(DOCS "/after-errors.bin",data,sizeof data);assert(!tmc2_verify(&fs));tmc2_close(&fs);free(disk);puts("PASS transferred data survives cold filesystem remount and full checksum verification");return 0;
}
