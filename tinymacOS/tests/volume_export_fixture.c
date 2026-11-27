/* Disposable host fixture and READ-ONLY guest output extraction.
 * Not evidence of guest source creation; the installed destination is GUI-made.
 */
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "tmcbfs2.h"
static int fd;static u64 base;
static int rd(void*x,u64 b,void*p){(void)x;return pread(fd,p,T2_BLOCK,base+b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int wr(void*x,u64 b,const void*p){(void)x;return pwrite(fd,p,T2_BLOCK,base+b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int fl(void*x){(void)x;return fsync(fd);}
static void*al(void*x,size_t n){(void)x;return malloc(n);}static void fr(void*x,void*p){(void)x;free(p);}
static u64 node(Tmc2*t,u64 parent,const char*name,int kind){u64 id;assert(!tmc2_create(t,parent,name,kind,&id));return id;}
int main(int argc,char**argv){
 assert(argc>=5&&!strncmp(argv[2],"/var/tmp/",9));int seed=!strcmp(argv[1],"seed");assert(seed||!strcmp(argv[1],"extract"));
 fd=open(argv[2],seed?O_RDWR:O_RDONLY);assert(fd>=0);base=strtoull(argv[3],0,0)*512;u64 blocks=strtoull(argv[4],0,0)/32;
 Tmc2 fs;T2Io io={0,blocks,rd,seed?wr:0,fl,al,fr};
 if(seed){
  assert(!tmc2_format(&fs,io));u64 d=node(&fs,1,"Documenti",T2_DIR),f=node(&fs,d,"esterno.txt",T2_FILE);assert(tmc2_write(&fs,f,0,"EXTERNAL_NATIVE_GUEST",21)==21);
  f=node(&fs,d,"caffè.txt",T2_FILE);u8 p[65537];for(unsigned i=0;i<sizeof p;i++)p[i]=(i*17+23)&255;assert(tmc2_write(&fs,f,0,p,sizeof p)==sizeof p);node(&fs,d,"vuoto.txt",T2_FILE);
  assert(!tmc2_collect(&fs)&&!tmc2_verify(&fs));
 }else{
  assert(argc==7&&!strncmp(argv[6],"/var/tmp/",9));assert(!tmc2_mount(&fs,io));T2Entry e={.key=1};char*path=strdup(argv[5]);char*part=strtok(path,"/");
  while(part){assert(!tmc2_lookup(&fs,e.key,part,&e));part=strtok(0,"/");}free(path);assert(e.kind==T2_FILE);
  int out=open(argv[6],O_CREAT|O_EXCL|O_WRONLY,0600);assert(out>=0);u8 p[65536];u64 at=0;
  while(at<e.size){unsigned want=e.size-at<sizeof p?e.size-at:sizeof p;i64 n=tmc2_read(&fs,e.key,at,p,want);assert(n==want&&write(out,p,n)==n);at+=n;}assert(!close(out));
  printf("PASS read-only extraction of actual guest export: %s, %llu bytes\n",argv[5],(unsigned long long)e.size);
 }
 tmc2_close(&fs);close(fd);return 0;
}
