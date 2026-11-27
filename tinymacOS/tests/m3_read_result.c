/* Read-only extraction of guest-produced results after QEMU has shut down.
 * Only disposable m3 QA paths; never preformats or writes guest content. */
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "tmcbfs2.h"
static int fd;static u64 base;
static int rd(void*x,u64 b,void*p){(void)x;return pread(fd,p,T2_BLOCK,base+b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int wr(void*x,u64 b,const void*p){(void)x;(void)b;(void)p;return -1;}
static int fl(void*x){(void)x;return 0;}
static void*al(void*x,size_t n){(void)x;return malloc(n);}
static void fr(void*x,void*p){(void)x;free(p);}
int main(int argc,char**argv){
 assert(argc==4&&!strncmp(argv[1],"/var/tmp/tmc-m3-guest-",22));fd=open(argv[1],O_RDONLY);assert(fd>=0);base=strtoull(argv[2],0,0)*512;u64 sectors=strtoull(argv[3],0,0);Tmc2 fs;
 assert(!tmc2_mount(&fs,(T2Io){0,sectors/32,rd,wr,fl,al,fr})&&!tmc2_verify(&fs));T2Entry e;u64 parent=1;
 const char*parts[]={"Users","User","Documents","m3-result.jsonl"};for(unsigned j=0;j<4;j++){assert(!tmc2_lookup(&fs,parent,parts[j],&e));parent=e.key;}
 assert(e.kind==T2_FILE&&e.size&&e.size<=8192);char buf[8192];assert(tmc2_read(&fs,e.key,0,buf,e.size)==(i64)e.size);assert(fwrite(buf,1,e.size,stdout)==e.size);tmc2_close(&fs);close(fd);
}
