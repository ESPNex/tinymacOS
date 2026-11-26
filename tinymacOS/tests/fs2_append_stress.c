/* Append UI stress data to an ALREADY GUI-installed disposable test volume.
 * No format, no account changes. This is not part of installer acceptance. */
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
static int wr(void*x,u64 b,const void*p){(void)x;return pwrite(fd,p,T2_BLOCK,base+b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int fl(void*x){(void)x;return fsync(fd);}
static void*al(void*x,size_t n){(void)x;return malloc(n);}
static void fr(void*x,void*p){(void)x;free(p);}
int main(int argc,char**argv){
 assert(argc==4&&!strncmp(argv[1],"/var/tmp/tmc-fs2-install-",25));fd=open(argv[1],O_RDWR);assert(fd>=0);base=strtoull(argv[2],0,0)*512;u64 sectors=strtoull(argv[3],0,0);Tmc2 fs;
 assert(!tmc2_mount(&fs,(T2Io){0,sectors/32,rd,wr,fl,al,fr}));T2Entry e;u64 parent=1;const char*parts[]={"Users","User","Documents"};for(unsigned j=0;j<3;j++){assert(!tmc2_lookup(&fs,parent,parts[j],&e));parent=e.key;}
 u64 folder;assert(!tmc2_create(&fs,parent,"Pagination",T2_DIR,&folder));
 for(unsigned j=0;j<1100;j++){char name[48];sprintf(name,"item-%04u",j);u64 id;assert(!tmc2_create(&fs,folder,name,T2_FILE,&id));}
 u64 large;assert(!tmc2_create(&fs,folder,"large.txt",T2_FILE,&large));char chunk[8192];memset(chunk,'X',sizeof chunk);memcpy(chunk,"LARGE_TEXTEDIT_128K\n",19);
 for(unsigned j=0;j<16;j++)assert(tmc2_write(&fs,large,j*sizeof chunk,chunk,sizeof chunk)==sizeof chunk);
 for(unsigned j=0;j<20;j++){char name[48];sprintf(name,"paged-%02u",j);assert(!tmc2_snapshot(&fs,name,0));}
 assert(!tmc2_collect(&fs)&&!tmc2_verify(&fs));tmc2_close(&fs);close(fd);puts("PASS appended 1100 files, 128KiB editor document and 20 snapshots to an already installed FS2; no format/account changes");
}
