/* Macroblock 5 document-only offline input fixture on an ALREADY GUI-installed disposable volume.
 * No format/account changes. Not proof of guest bulk creation. Guest tests
 * independently opens/edits/saves this file; host extraction is read-only. */
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "tmcbfs2.h"
#include "fs2_qa_home.h"
static int fd;static u64 base;
static int rd(void*x,u64 b,void*p){(void)x;return pread(fd,p,T2_BLOCK,base+b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int wr(void*x,u64 b,const void*p){(void)x;return pwrite(fd,p,T2_BLOCK,base+b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int fl(void*x){(void)x;return fsync(fd);}
static void*al(void*x,size_t n){(void)x;return malloc(n);}
static void fr(void*x,void*p){(void)x;free(p);}
int main(int argc,char**argv){
 assert(argc==5&&!strncmp(argv[1],"/var/tmp/tmc-m3-guest-",22));int prepare=!strcmp(argv[4],"prepare");assert(prepare||!strcmp(argv[4],"read"));fd=open(argv[1],prepare?O_RDWR:O_RDONLY);assert(fd>=0);base=strtoull(argv[2],0,0)*512;u64 sectors=strtoull(argv[3],0,0);Tmc2 fs;
 assert(!tmc2_mount(&fs,(T2Io){0,sectors/32,rd,wr,fl,al,fr}));T2Entry e;u64 parent=1;const char*parts[]={"Users","User","Documents"};for(unsigned j=0;j<3;j++){if(j==1){parent=qa_home(&fs,parent);assert(parent);}else{assert(!tmc2_lookup(&fs,parent,parts[j],&e));parent=e.key;}}
 static char text[144001];unsigned len=0;
 if(prepare){
  for(int j=0;j<48;j++)len+=sprintf(text+len,"Riga %03d: abcdefghijklmnopqrstuvwxyz fgijpq àèéòù € parole intere senza perdere lettere. LUNGHISSIMAPAROLASENZASPAZIABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 fine riga.\n",j);
  u64 id;assert(tmc2_lookup(&fs,parent,"apps5-input.txt",&e));assert(!tmc2_create(&fs,parent,"apps5-input.txt",T2_FILE,&id));
  assert(tmc2_write(&fs,id,0,text,len)==len);printf("Prepared %u plain text bytes only; installed accounts/system untouched\n",len);
 }else{
  assert(!tmc2_lookup(&fs,parent,"apps5-input.txt",&e)&&e.size<sizeof text);
  assert(tmc2_read(&fs,e.key,0,text,e.size)==(i64)e.size);assert(fwrite(text,1,e.size,stdout)==e.size);
 }
 if(prepare)assert(!tmc2_collect(&fs));assert(!tmc2_verify(&fs));tmc2_close(&fs);close(fd);
}
