/* Execute the shipping mounted backend and actual COW inode rename, including
 * real FV003 block transforms. Never opens any host block device. */
#define main old_runtime_main
#include "fs2_runtime_host.c"
#undef main
#include "fs2_volume.h"
#define HOME_BASE 2048u
#define HOME_EXTENT 32768u
#define HOME_SECTORS 36864u
int sata_count(void){return 1;}
int volume_current(void){return -1;}
u64 sata_size(int d){assert(!d);return HOME_SECTORS;}
u32 sata_logical_bytes(int d){assert(!d);return 512;}
int sata_flush(int d){assert(!d);return 0;}
int sata_io(int d,u64 l,u32 n,void*p,int wr){assert(!d&&l<HOME_SECTORS&&n<=HOME_SECTORS-l);if(wr)memcpy(disk+l*512,p,n*512);else memcpy(p,disk+l*512,n*512);return 0;}
void*kmalloc(size_t n){return malloc(n);}void kfree(void*p){free(p);}int kmalloc_owns(const void*p){(void)p;return 1;}
u64 page_get(void){return (u64)malloc(4096);}void page_put(u64 p){free((void*)p);}
int vault_random(u8*p,size_t n){for(size_t j=0;j<n;j++)p[j]=(j*31+57)%251+1;return 1;}
int layout_read(Medium*m,Layout*l){(void)m;(void)l;return 0;}
static int read_home(void*x,u64 b,void*p){(void)x;return fs2_volume_read(0,HOME_BASE,HOME_EXTENT,b,p);}
static int write_home(void*x,u64 b,const void*p){(void)x;return fs2_volume_write(0,HOME_BASE,HOME_EXTENT,b,p);}
static void account(const char*name,u8 raw[512]){memset(raw,0,512);memcpy(raw,"TMCUSER2",8);u32 v=1,r=120000;memcpy(raw+8,&v,4);memcpy(raw+12,&r,4);strset((char*)raw+16,name,32);sha_bytes(raw,480,raw+480);}
static void run(int encrypted){
 disk=calloc(HOME_SECTORS,512);assert(disk);fs2_volume_forget(0);char recovery[65];
 if(encrypted){u8 seed[128];vault_random(seed,sizeof seed);assert(!fs2_volume_create_fresh(0,HOME_BASE,HOME_EXTENT,"home-test-only-password",seed,recovery));}
 T2Io io={0,fs2_volume_blocks(0,HOME_BASE,HOME_EXTENT),read_home,write_home,sync_page,allocate,deallocate};
 assert(!tmc2_format(&fs,io));fs2_active=1;strset(home_name,"User",48);
 u64 users=node(1,"Users",T2_DIR),home=node(users,"User",T2_DIR),docs=node(home,"Documents",T2_DIR),file=node(docs,"proof",T2_FILE);
 const char *name="Elena Ròssi";u8 raw[512],check[512],payload[8192],readback[8192];account(name,raw);
 for(unsigned j=0;j<sizeof payload;j++)payload[j]=(j*17+9)%251;
 assert(tmc2_write(&fs,file,0,payload,sizeof payload)==sizeof payload);
 assert(!fs2_account(1,raw,1)&&!fs2_account(0,raw,1)&&!fs2_snap_create("prima"));
 Process p={0};int fd=fs2_open(&p,DOCS "/proof",0);assert(fd>=0);
 assert(!fs2_home_bind(name));T2Entry e;assert(tmc2_lookup(&fs,users,"User",&e)<0);
 assert(!tmc2_lookup(&fs,users,name,&e)&&e.key==home);Dirent d,list[8];
 assert(fs2_list("/Users",list,8,0)==1&&eq(list[0].name,name));
 assert(!fs2_stat(HOME,&d)&&eq(d.name,name));assert(!fs2_stat("/Users/Elena Ròssi/Documents/proof",&d)&&d.id==file);
 assert(fs2_read(&p,fd,readback,sizeof readback)==sizeof readback&&!memcmp(payload,readback,sizeof payload));assert(!fs2_close(&p,fd));
 assert(fs2_remove("/Users/Elena Ròssi",REMOVE_RECURSIVE)==-REMOVE_READ_ONLY);
 assert(fs2_move("/Users/Elena Ròssi","/Users/stolen")<0);
 assert(!fs2_account(0,check,0)&&!memcmp(raw,check,512));
 assert(!fs2_stat("/.snapshots/prima/Users/User/Documents/proof",&d)&&d.ro);
 assert(fs2_stat("/.snapshots/prima/Users/Elena Ròssi/Documents/proof",&d)<0);
 u64 generation=fs.super.generation;assert(!fs2_home_bind(name)&&fs.super.generation==generation);
 assert(fs2_home_bind("../bad")<0&&fs2_home_bind(".")<0&&fs2_home_bind("")<0);
 assert(!fs2_snap_create("dopo"));assert(!fs2_stat("/.snapshots/dopo/Users/User/Documents/proof",&d)&&d.id==file);tmc2_close(&fs);
 if(encrypted){fs2_volume_forget(0);assert(fs2_volume_encrypted(0,HOME_BASE,HOME_EXTENT)==1&&fs2_volume_locked(0));VaultRequest q={.disk=0};VaultInfo info;int result;strset(q.password,recovery,65);assert(fs2_volume_control(4,0,&q,&info,&result)&&!result);}
 assert(!tmc2_mount(&fs,io));home_refresh();assert(eq(home_name,name)&&!fs2_stat(DOCS "/proof",&d));
 assert(!fs2_snap_restore("prima")&&eq(home_name,"User")&&!fs2_stat(DOCS "/proof",&d));
 assert(!fs2_home_bind(name)&&!fs2_snap_restore("dopo")&&eq(home_name,name));
 fd=fs2_open(&p,DOCS "/proof",0);assert(fd>=0&&fs2_read(&p,fd,readback,sizeof readback)==sizeof readback&&!memcmp(payload,readback,sizeof payload));assert(!fs2_close(&p,fd));
 /* Separate fixture: do not overwrite or bind an unrelated existing name. */
 assert(!tmc2_move(&fs,home,users,"User"));strset(home_name,"User",48);u64 collision=node(users,name,T2_DIR);generation=fs.super.generation;
 assert(fs2_home_bind(name)==-2&&fs.super.generation==generation);assert(!tmc2_lookup(&fs,users,name,&e)&&e.key==collision);assert(!tmc2_lookup(&fs,users,"User",&e)&&e.key==home);
 tmc2_close(&fs);fs2_volume_forget(0);free(disk);
 printf("PASS actual home %s: real UTF8 inode name, unchanged IDs/content/account, old path alias, no duplicate, protected named home, open FD, idempotence, invalid/colliding refusal, before/after snapshots, cold remount/unlock\n",encrypted?"FV003":"plain");
}
int main(void){run(0);run(1);return 0;}
