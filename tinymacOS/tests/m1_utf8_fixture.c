/* Offline input fixture on an ALREADY GUI-installed disposable volume.
 * No format/account changes. Not proof of guest bulk creation. Guest tests
 * independently open/edit/save/reopen these files and stream native hashes. */
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
 assert(argc==4&&!strncmp(argv[1],"/var/tmp/tmc-m1-acceptance-",27));fd=open(argv[1],O_RDWR);assert(fd>=0);base=strtoull(argv[2],0,0)*512;u64 sectors=strtoull(argv[3],0,0);Tmc2 fs;
 assert(!tmc2_mount(&fs,(T2Io){0,sectors/32,rd,wr,fl,al,fr}));T2Entry e;u64 parent=1;const char*parts[]={"Users","User","Documents"};for(unsigned j=0;j<3;j++){assert(!tmc2_lookup(&fs,parent,parts[j],&e));parent=e.key;}
 static char text[144001];
 for(int newline=0;newline<2;newline++){
  unsigned len=0;for(int j=0;j<24000;j++){const char*unit=newline&&j%53==0?"\n":"à€Z";unsigned n=strlen(unit);memcpy(text+len,unit,n);len+=n;}
  u64 id;assert(!tmc2_create(&fs,parent,newline?"m1-lines.txt":"m1-raw.txt",T2_FILE,&id));
  for(unsigned off=0;off<len;){unsigned n=len-off;if(n>8192)n=8192;assert(tmc2_write(&fs,id,off,text+off,n)==n);off+=n;}
  printf("Prepared %u UTF8 bytes, %s newlines\n",len,newline?"with":"without");
 }
 assert(!tmc2_collect(&fs)&&!tmc2_verify(&fs));tmc2_close(&fs);close(fd);
}
