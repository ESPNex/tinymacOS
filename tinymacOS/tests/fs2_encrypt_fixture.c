/* Host-only QA fixture transformation, NEVER an installer/conversion API.
 * Read a bounded already-installed test home into RAM, wipe that disposable
 * partition, create a FRESH authenticated envelope and import the QA contents.
 * A crash here destroys only this fixture; this is NOT in-place migration.
 */
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "tmcbfs2.h"
#include "vault2.h"
static int fd;static u64 base,units;static Vault2 vault;
static int rd(void*x,u64 b,void*p){(void)x;return b>=units?-1:pread(fd,p,FV2_UNIT,base+b*FV2_UNIT)==FV2_UNIT?0:-1;}
static int wr(void*x,u64 b,const void*p){(void)x;return b>=units?-1:pwrite(fd,p,FV2_UNIT,base+b*FV2_UNIT)==FV2_UNIT?0:-1;}
static int fl(void*x){(void)x;return fsync(fd);}
static void*al(void*x,size_t n){(void)x;return malloc(n);}
static void fr(void*x,void*p){(void)x;free(p);}
static int oldrd(void*x,u64 b,void*p){(void)x;return pread(fd,p,T2_BLOCK,base+b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int oldwr(void*x,u64 b,const void*p){(void)x;(void)b;(void)p;assert(!"source mount is READ ONLY");return -1;}
static int newrd(void*x,u64 b,void*p){(void)x;return vault2_read(&vault,b,p);}
static int newwr(void*x,u64 b,const void*p){(void)x;return vault2_write(&vault,b,p);}
static struct {T2Entry entry;u8*data;u64 new_id;} nodes[128];static unsigned count;static u64 loaded;
static void collect(Tmc2*c,u64 parent,unsigned depth){
 assert(depth<32);T2Entry e;u64 cookie=0;int r;
 while((r=tmc2_readdir(c,parent,&cookie,&e))==1){
  assert(count<128&&e.size<=262144&&loaded+e.size<=1048576);unsigned at=count++;nodes[at].entry=e;
  if(e.kind==T2_FILE&&e.size){nodes[at].data=malloc(e.size);assert(nodes[at].data&&tmc2_read(c,e.key,0,nodes[at].data,e.size)==(i64)e.size);loaded+=e.size;}
  if(e.kind==T2_DIR)collect(c,e.key,depth+1);
 }assert(!r);
}
int main(int argc,char**argv){
 assert(argc==5&&!strncmp(argv[1],"/var/tmp/tmc-fs2-install-",25)&&!strncmp(argv[4],"/var/tmp/tmc-fs2-install-",25));fd=open(argv[1],O_RDWR);assert(fd>=0);base=strtoull(argv[2],0,0)*512;u64 bytes=strtoull(argv[3],0,0)*512;assert(!(base%4096));units=bytes/FV2_UNIT;
 Tmc2 old;assert(!tmc2_mount(&old,(T2Io){0,bytes/T2_BLOCK,oldrd,oldwr,fl,al,fr}));u8 accounts[2][512];for(unsigned i=0;i<2;i++)assert(!tmc2_account(&old,i,accounts[i],0));assert(!tmc2_verify(&old));collect(&old,1,0);tmc2_close(&old);
 static u8 zeros[65536];for(u64 at=0;at<bytes;){size_t n=MIN(sizeof zeros,bytes-at);assert(pwrite(fd,zeros,n,base+at)==(ssize_t)n);at+=n;}assert(!fsync(fd));
 u8 random[128];int rng=open("/dev/urandom",O_RDONLY);assert(rng>=0&&read(rng,random,sizeof random)==sizeof random);close(rng);char recovery[65];
 assert(!vault2_probe(&vault,(Fv2Io){0,units,rd,wr,fl,al,fr})&&!vault2_create(&vault,"local-test-285",random,recovery));
 int keyfd=open(argv[4],O_CREAT|O_EXCL|O_WRONLY,0600);assert(keyfd>=0&&write(keyfd,recovery,64)==64&&!fsync(keyfd));close(keyfd);
 Tmc2 fs;assert(!tmc2_format(&fs,(T2Io){0,vault.h.blocks,newrd,newwr,fl,al,fr}));
 for(unsigned i=0;i<count;i++){
  T2Entry*e=&nodes[i].entry;u64 parent=1;if(e->parent!=1){parent=0;for(unsigned j=0;j<i;j++)if(nodes[j].entry.key==e->parent)parent=nodes[j].new_id;assert(parent);}
  assert(!tmc2_create(&fs,parent,e->name,e->kind,&nodes[i].new_id));if(e->kind==T2_FILE&&e->size)assert(tmc2_write(&fs,nodes[i].new_id,0,nodes[i].data,e->size)==(i64)e->size);if(e->reserved&1)assert(!tmc2_readonly(&fs,nodes[i].new_id,1));free(nodes[i].data);
 }
 for(unsigned i=0;i<2;i++)assert(!tmc2_account(&fs,i,accounts[i],1));
 T2Entry docs;u64 p=1;const char*parts[]={"Users","User","Documents"};for(unsigned i=0;i<3;i++){assert(!tmc2_lookup(&fs,p,parts[i],&docs));p=docs.key;}
 u64 id;assert(!tmc2_create(&fs,p,"encrypted-proof.txt",T2_FILE,&id));const char proof[]="FV3_GUEST_ONLY_SECRET_30B16\n";assert(tmc2_write(&fs,id,0,proof,sizeof proof-1)==sizeof proof-1);
 assert(!tmc2_create(&fs,p,"encrypted-large.txt",T2_FILE,&id));u8 chunk[8192];memset(chunk,'X',sizeof chunk);memcpy(chunk,"VAULT2_SCALABLE_128K\n",21);for(unsigned i=0;i<16;i++)assert(tmc2_write(&fs,id,i*sizeof chunk,chunk,sizeof chunk)==sizeof chunk);
 assert(!tmc2_snapshot(&fs,"encrypted-baseline",0)&&!tmc2_collect(&fs)&&!tmc2_verify(&fs));tmc2_close(&fs);vault2_lock(&vault);assert(!fsync(fd));close(fd);memset(recovery,0,sizeof recovery);memset(accounts,0,sizeof accounts);memset(random,0,sizeof random);
 puts("PASS fresh host-prepared encrypted QA fixture: original GUI-installed public partitions retained, bounded home/account import, encrypted 128KiB file and snapshot; NOT encrypted-installer or crash-safe conversion acceptance");
}
