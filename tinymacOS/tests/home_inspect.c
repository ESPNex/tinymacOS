/* Independent READ-ONLY evidence on the guest-created disposable image.
 * This tool cannot format, rename or fix the data it is meant to inspect. */
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "tmcbfs2.h"
#include "home_name.h"
static int fd;static u64 base;
static int rd(void*x,u64 b,void*p){(void)x;return pread(fd,p,T2_BLOCK,base+b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int wr(void*x,u64 b,const void*p){(void)x;(void)b;(void)p;assert(!"read-only inspector attempted a write");return -1;}
static int fl(void*x){(void)x;return 0;}
static void*al(void*x,size_t n){(void)x;return malloc(n);}static void fr(void*x,void*p){(void)x;free(p);}
int main(int argc,char**argv){
 assert(argc==5&&!strncmp(argv[1],"/var/tmp/tmc-fs2-install-",25));fd=open(argv[1],O_RDONLY);assert(fd>=0);base=strtoull(argv[2],0,0)*512;u64 sectors=strtoull(argv[3],0,0);Tmc2 fs;
 assert(!tmc2_mount(&fs,(T2Io){0,sectors/32,rd,wr,fl,al,fr}));T2Entry users,home,absent,doc,file;
 u8 account[512];char name[48];assert(!tmc2_account(&fs,0,account,0)&&home_account_name(account,name)&&eq(name,argv[4]));
 assert(!tmc2_lookup(&fs,1,"Users",&users)&&!tmc2_lookup(&fs,users.key,name,&home)&&home.kind==T2_DIR);
 assert(tmc2_lookup(&fs,users.key,"User",&absent)<0);u64 cookie=0;assert(tmc2_readdir(&fs,users.key,&cookie,&absent)==1&&absent.key==home.key&&tmc2_readdir(&fs,users.key,&cookie,&absent)==0);
 assert(!tmc2_lookup(&fs,home.key,"Documents",&doc)&&!tmc2_lookup(&fs,doc.key,"imported.txt",&file));
 char content[128]={0};i64 n=tmc2_read(&fs,file.key,0,content,sizeof content-1);assert(n>0&&strstr(content,"IMPORTED_FROM_LIVE_ACCOUNT"));
 assert(!tmc2_lookup(&fs,doc.key,"persistent.txt",&file));memset(content,0,sizeof content);n=tmc2_read(&fs,file.key,0,content,sizeof content-1);assert(n>0&&strstr(content,"FS2_INSTALL_PERSISTENT"));
 assert(!tmc2_verify(&fs));erase(account,sizeof account);tmc2_close(&fs);close(fd);
 puts("PASS independent read-only mounted image: exactly one account-named home, no physical User directory, live-imported and snapshot-restored documents intact; metadata/account verified");return 0;
}
