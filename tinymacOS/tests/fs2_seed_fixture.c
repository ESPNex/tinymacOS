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
static Tmc2 fs;
static u64 add(u64 parent,const char*name,int kind){u64 id;assert(!tmc2_create(&fs,parent,name,kind,&id));return id;}
int main(int argc,char**argv){
 assert(argc==4&&!strncmp(argv[1],"/var/tmp/tmc-fs2-guest-",23));fd=open(argv[1],O_RDWR);assert(fd>=0);base=strtoull(argv[2],0,0)*512;u64 blocks=strtoull(argv[3],0,0)/32;
 assert(!tmc2_format(&fs,(T2Io){0,blocks,rd,wr,fl,al,fr}));
 u64 users=add(1,"Users",T2_DIR),home=add(users,"User",T2_DIR),docs=add(home,"Documents",T2_DIR);
 add(home,"Desktop",T2_DIR);add(home,"Downloads",T2_DIR);add(home,"Pictures",T2_DIR);u64 library=add(home,"Library",T2_DIR),prefs=add(library,"Preferences",T2_DIR);add(library,"Application Support",T2_DIR);add(prefs,"com.tinymacos.appearance.plist",T2_FILE);
 library=add(1,"Library",T2_DIR);add(library,"Preferences",T2_DIR);add(library,"Application Support",T2_DIR);add(1,"Volumes",T2_DIR);
 u64 proof=add(docs,"large-proof.txt",T2_FILE);char b[8192];memset(b,'X',sizeof b);for(int i=0;i<16;i++){if(!i)memcpy(b,"TMCBFS2_FILE_EXCEEDS_8K\n",23);if(i==15)memcpy(b+sizeof b-32,"TMCBFS2_END_AFTER_128K\n",22);assert(tmc2_write(&fs,proof,(u64)i*sizeof b,b,sizeof b)==sizeof b);}
 for(unsigned i=0;i<1100;i++){char name[48];snprintf(name,sizeof name,"item-%04u",i);add(docs,name,T2_FILE);}
 for(unsigned i=0;i<20;i++){char name[48];snprintf(name,sizeof name,"snapshot-%02u",i);assert(!tmc2_snapshot(&fs,name,0));}
 assert(!tmc2_collect(&fs));tmc2_close(&fs);close(fd);puts("PASS seeded real TMCBFS2: full partition geometry, 128KiB file, 1100 extra files, 20 snapshots; no account yet");return 0;
}
