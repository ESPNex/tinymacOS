/* Offline append-only UI stress to a disposable, already GUI-installed disk.
 * This is NOT evidence of guest creation of 43 snapshots. No format/accounts.
 */
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
 assert((argc==4||argc==5)&&!strncmp(argv[1],"/var/tmp/tmc-fs2-install-",25));
 int verify=argc==5;assert(!verify||!strcmp(argv[4],"--verify"));
 fd=open(argv[1],verify?O_RDONLY:O_RDWR);assert(fd>=0);base=strtoull(argv[2],0,0)*512;
 u64 sectors=strtoull(argv[3],0,0);Tmc2 fs;
 assert(!tmc2_mount(&fs,(T2Io){0,sectors/32,rd,wr,fl,al,fr}));
 T2Entry e;u64 cookie=0;int initial=0,installer=0,existing_local=0,rc;
 while((rc=tmc2_snapshots(&fs,&cookie,&e))==1){printf("existing snapshot: %s\n",e.name);initial++;if(!strcmp(e.name,"installer-guest"))installer++;if(!strcmp(e.name,"Copia locale"))existing_local++;if(!verify)assert(strncmp(e.name,"catalog-",8));}
 assert(!rc&&installer==1);
 if(verify){assert(initial==43&&existing_local==1&&!tmc2_verify(&fs));tmc2_close(&fs);close(fd);puts("PASS read-only verification of existing 43-snapshot disposable fixture; no reappend or format");return 0;}
 assert(initial==2&&!existing_local);
 assert(!tmc2_lookup(&fs,1,"Users",&e));u64 home=qa_home(&fs,e.key);assert(home);
 assert(!tmc2_lookup(&fs,home,"Documents",&e));u64 docs=e.key;
 assert(!tmc2_lookup(&fs,docs,"persistent.txt",&e));u64 file=e.key;
 char before[128]={0},after[128]={0};i64 count=tmc2_read(&fs,file,0,before,sizeof before);
 assert(count>0&&count<sizeof before);
 assert(!tmc2_checkpoint(&fs,"Copia locale"));
 for(int i=0;i<42-initial;i++){char name[28];sprintf(name,"catalog-%02d",i);assert(!tmc2_snapshot(&fs,name,0));}
 cookie=0;int snapshots=0,local=0,r;while((r=tmc2_snapshots(&fs,&cookie,&e))==1){snapshots++;if(!strcmp(e.name,"Copia locale"))local++;}
 assert(!r&&snapshots==43&&local==1);
 assert(tmc2_read(&fs,file,0,after,sizeof after)==count&&!memcmp(before,after,count));
 assert(!tmc2_collect(&fs)&&!tmc2_verify(&fs));tmc2_close(&fs);close(fd);
 puts("PASS appended UI stress snapshots and one Copia locale to the GUI-installed disk: 43 total, one local checkpoint, live marker unchanged, full verification; no format/account modification");
}
