/* Production 64-bit/4KiB-I/O envelope on sparse files and a volatile disk model.
 * No physical disks, no fast KDF substitutions, no fake crypto primitives. */
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "vault2.h"
#include "tmcbfs2.h"
#include "sha256.h"
static struct {int fd,cut,event,torn;u64 units,reads,writes,flushes;u8*stable,*cache;} m;
static int rd(void*x,u64 unit,void*p){(void)x;assert(unit<m.units);m.reads++;if(m.cache){memcpy(p,m.cache+unit*FV2_UNIT,FV2_UNIT);return 0;}return pread(m.fd,p,FV2_UNIT,unit*FV2_UNIT)==FV2_UNIT?0:-1;}
static int wr(void*x,u64 unit,const void*p){(void)x;assert(unit<m.units);m.writes++;
 if(m.cut>=0&&m.event++==m.cut){if(m.torn){assert(m.stable);unsigned n=m.torn==1?37:FV2_UNIT-1;memcpy(m.stable+unit*FV2_UNIT,p,n);memcpy(m.cache+unit*FV2_UNIT,p,n);}return -1;}
 if(m.cache){memcpy(m.cache+unit*FV2_UNIT,p,FV2_UNIT);return 0;}return pwrite(m.fd,p,FV2_UNIT,unit*FV2_UNIT)==FV2_UNIT?0:-1;
}
static int fl(void*x){(void)x;m.flushes++;
 if(m.cut>=0&&m.event++==m.cut){if(m.torn){assert(m.stable);unsigned n=m.torn==1?37:FV2_UNIT-1;for(u64 j=0;j<m.units;j++)if(memcmp(m.stable+j*FV2_UNIT,m.cache+j*FV2_UNIT,FV2_UNIT))memcpy(m.stable+j*FV2_UNIT,m.cache+j*FV2_UNIT,n);}return -1;}
 if(m.cache){memcpy(m.stable,m.cache,m.units*FV2_UNIT);return 0;}return fsync(m.fd);
}
static void*al(void*x,size_t n){(void)x;assert(n<=FV2_RECORD);return malloc(n);}
static void fr(void*x,void*p){(void)x;free(p);}
static Fv2Io io(void){return (Fv2Io){0,m.units,rd,wr,fl,al,fr};}
static u8 randoms[128],plain[FV2_BLOCK],out[FV2_BLOCK];static char recovery[65];
static const char*password="scalable-block-password";
static void seed(void){for(unsigned j=0;j<128;j++)randoms[j]=(j*71+19)%251+1;for(unsigned j=0;j<FV2_BLOCK;j++)plain[j]=(j*17+13)%251;}
static void ram(unsigned blocks){memset(&m,0,sizeof m);m.fd=-1;m.cut=-1;m.units=FV2_START+5ull*blocks;m.stable=calloc(m.units,FV2_UNIT);m.cache=calloc(m.units,FV2_UNIT);assert(m.stable&&m.cache);}
static void close_ram(void){free(m.stable);free(m.cache);m.stable=m.cache=0;}
static void boot(Vault2*v){m.cut=-1;memcpy(m.cache,m.stable,m.units*FV2_UNIT);assert(vault2_probe(v,io())==1);assert(!vault2_unlock(v,recovery));}
static void sparse(const char*vector){
 memset(&m,0,sizeof m);m.cut=-1;m.units=FV2_START+5*FV2_MAX_BLOCKS;char path[]="/var/tmp/tmc-vault2-sparse-XXXXXXXX";m.fd=mkstemp(path);assert(m.fd>=0&&!unlink(path));assert(!ftruncate(m.fd,m.units*FV2_UNIT));
 assert(vault2_capacity(m.units)==FV2_MAX_BLOCKS&&!vault2_capacity(m.units+5));assert(!vault2_capacity(13));
 Vault2 v;assert(!vault2_probe(&v,io()));u64 start=m.writes;assert(!vault2_create(&v,password,randoms,recovery));assert(m.writes-start==4); /* no size-proportional initialization */
 assert(!vault2_write(&v,0,plain));assert(!vault2_write(&v,FV2_MAX_BLOCKS-1,plain));assert(!vault2_read(&v,FV2_MAX_BLOCKS-1,out)&&!memcmp(out,plain,sizeof out));
 assert(m.writes<40&&m.reads<30);vault2_lock(&v);assert(vault2_read(&v,0,out)==FV_AUTH);for(unsigned i=0;i<sizeof out;i++)assert(!out[i]);
 assert(vault2_probe(&v,io())==1);assert(vault2_unlock(&v,"incorrect-password")==FV_AUTH&&!v.unlocked);assert(!vault2_unlock(&v,password));assert(!vault2_read(&v,0,out)&&!memcmp(out,plain,sizeof out));
 FILE*f=fopen(vector,"wb");assert(f);assert(fwrite(&v.h,1,512,f)==512);u8 meta[FV2_UNIT],cipher[FV2_BLOCK];assert(!rd(0,FV2_START+4,meta));assert(pread(m.fd,cipher,FV2_BLOCK,FV2_START*FV2_UNIT)==FV2_BLOCK);
 assert(fwrite(meta,1,sizeof meta,f)==sizeof meta&&fwrite(cipher,1,sizeof cipher,f)==sizeof cipher&&fwrite(plain,1,sizeof plain,f)==sizeof plain&&fwrite(recovery,1,64,f)==64);assert(!fclose(f));
 u8 rotate[72];for(unsigned j=0;j<sizeof rotate;j++)rotate[j]=j+77;char changed[65];assert(!vault2_recovery(&v,recovery,rotate,changed));vault2_lock(&v);assert(vault2_probe(&v,io())==1&&vault2_unlock(&v,recovery)==FV_AUTH&&!vault2_unlock(&v,changed));
 assert(!vault2_password(&v,changed,"new-scalable-password",rotate));vault2_lock(&v);assert(vault2_probe(&v,io())==1&&vault2_unlock(&v,password)==FV_AUTH&&!vault2_unlock(&v,"new-scalable-password"));assert(!vault2_read(&v,FV2_MAX_BLOCKS-1,out)&&!memcmp(out,plain,sizeof out));
 vault2_lock(&v);struct stat s;assert(!fstat(m.fd,&s));close(m.fd);printf("PASS actual scalable authenticated envelope: 1TiB logical payload on 1.25TiB sparse backing, last 16KiB block roundtrips at 64-bit byte offset, only 4 create writes/no volume scan; %llu allocated bytes; password/recovery rotation and independent vector\n",(unsigned long long)s.st_blocks*512);
}
static void faults(void){
 ram(16);Vault2 v;assert(!vault2_probe(&v,io())&&!vault2_create(&v,password,randoms,recovery));memset(plain,'A',sizeof plain);assert(!vault2_write(&v,0,plain));vault2_lock(&v);
 size_t bytes=m.units*FV2_UNIT;u8*original=malloc(bytes);assert(original);memcpy(original,m.stable,bytes);
 for(int torn=0;torn<3;torn++)for(int cut=0;cut<40;cut++){
  memcpy(m.stable,original,bytes);boot(&v);m.cut=cut;m.torn=torn;m.event=0;memset(plain,'B',sizeof plain);int r=vault2_write(&v,0,plain);
  if(r){assert(v.poison);u64 writes=m.writes;assert(vault2_write(&v,0,plain)<0&&m.writes==writes);}u64 attempted=v.next?v.next-1:0;vault2_lock(&v);boot(&v);
  assert(!vault2_read(&v,0,out));u8 which=out[0];assert(which=='A'||which=='B');for(unsigned j=0;j<sizeof out;j++)assert(out[j]==which);
  u64 used;memcpy(&used,m.stable+(FV2_START+4)*FV2_UNIT+16,8);memset(plain,'C',sizeof plain);assert(!vault2_write(&v,0,plain));u64 after;memcpy(&after,m.stable+(FV2_START+4)*FV2_UNIT+16,8);assert(after>used&&after>attempted);vault2_lock(&v);boot(&v);assert(!vault2_read(&v,0,out)&&out[0]=='C');vault2_lock(&v);
 }
 puts("PASS 120 block-write/flush/torn-4KiB/volatile-cache interruptions: authenticated replay yields whole old/new 16KiB data, poisoned contexts refuse retries, new nonce is above pre-reboot range");
 for(int torn=0;torn<3;torn++)for(int cut=0;cut<6;cut++){
  memcpy(m.stable,original,bytes);boot(&v);u8 rnd[72];for(unsigned i=0;i<72;i++)rnd[i]=i+91;char rotated[65];m.cut=cut;m.torn=torn;m.event=0;vault2_recovery(&v,recovery,rnd,rotated);vault2_lock(&v);m.cut=-1;memcpy(m.cache,m.stable,bytes);assert(vault2_probe(&v,io())==1);
  int a=vault2_unlock(&v,recovery),b=vault2_unlock(&v,rotated);/* failed rotation erases the output; compute candidate independently */
  if(a&&b){for(unsigned i=0;i<32;i++){rotated[2*i]="0123456789ABCDEF"[rnd[i]>>4];rotated[2*i+1]="0123456789ABCDEF"[rnd[i]&15];}rotated[64]=0;b=vault2_unlock(&v,rotated);}assert(!a||!b);assert(!vault2_read(&v,0,out)&&out[0]=='A');vault2_lock(&v);
 }
 puts("PASS 18 interrupted recovery-key header rotations retain at least one authenticated old/new credential and preserve ciphertext data");
 const unsigned flips[]={0,FV2_UNIT-1,FV2_BLOCK-1,FV2_BLOCK,FV2_BLOCK+8,FV2_BLOCK+16,FV2_BLOCK+24,FV2_BLOCK+40,FV2_BLOCK+56};
 for(unsigned i=0;i<sizeof flips/sizeof flips[0];i++){
  memcpy(m.stable,original,bytes);m.stable[FV2_START*FV2_UNIT+flips[i]]^=0x40;boot(&v);memset(out,0x77,sizeof out);u64 writes=m.writes;assert(vault2_read(&v,0,out)==FV_CORRUPT&&v.poison&&writes==m.writes);for(unsigned j=0;j<sizeof out;j++)assert(!out[j]);vault2_lock(&v);
 }
 memcpy(m.stable,original,bytes);boot(&v);memset(plain,'Z',sizeof plain);assert(!vault2_write(&v,1,plain));memcpy(m.stable+FV2_START*FV2_UNIT,m.stable+(FV2_START+5)*FV2_UNIT,FV2_RECORD);vault2_lock(&v);boot(&v);assert(vault2_read(&v,0,out)==FV_CORRUPT);vault2_lock(&v);
 memcpy(m.stable,original,bytes);m.stable[0]^=0x80;boot(&v);assert(!vault2_read(&v,0,out)&&out[0]=='A');vault2_lock(&v);m.stable[FV2_UNIT]^=0x80;memcpy(m.cache,m.stable,bytes);u64 writes=m.writes;assert(vault2_probe(&v,io())==FV_CORRUPT&&m.writes==writes);assert(vault2_create(&v,password,randoms,recovery)<0&&m.writes==writes);
 memcpy(m.stable,original,bytes);for(unsigned i=0;i<2;i++){Fv2Header*h=(void*)(m.stable+i*FV2_UNIT);h->ceiling+=100;sha_bytes(h,480,h->sum);}memcpy(m.cache,m.stable,bytes);assert(vault2_probe(&v,io())==1&&vault2_unlock(&v,recovery)==FV_AUTH);
 puts("PASS ciphertext/tag/index/nonce/UUID/padding tampering and cross-block substitution fail closed with zero plaintext output; one-header recovery, two-header refusal without format, recomputed public SHA cannot forge header MAC");
 /* A committed intent with a damaged ciphertext journal may NOT be replayed
  * or silently dropped once target writes could have begun. */
 memcpy(m.stable,original,bytes);boot(&v);m.cut=20;m.torn=0;m.event=0;memset(plain,'J',sizeof plain);assert(vault2_write(&v,0,plain)<0);vault2_lock(&v);m.cut=-1;m.stable[4*FV2_UNIT+99]^=0x40;memcpy(m.cache,m.stable,bytes);writes=m.writes;assert(vault2_probe(&v,io())==1&&vault2_unlock(&v,recovery)==FV_CORRUPT&&v.poison&&!v.unlocked&&writes==m.writes);
 /* create() itself checks persisted blank headers before any write. */
 memcpy(m.stable,original,bytes);memcpy(m.cache,m.stable,bytes);Vault2 fake={.io=io()};writes=m.writes;assert(vault2_create(&fake,password,randoms,recovery)==FV_STATE&&writes==m.writes);
 puts("PASS damaged committed ciphertext journal refuses replay without writes; fresh-create boundary rechecks both persisted headers and refuses an existing volume even when caller skipped probe");
 free(original);close_ram();
}
static int fsrd(void*x,u64 b,void*p){return vault2_read(x,b,p);}
static int fswr(void*x,u64 b,const void*p){return vault2_write(x,b,p);}
static int fsfl(void*x){(void)x;return fl(0);}
static void filesystem(void){
 ram(256);Vault2 v;assert(!vault2_probe(&v,io())&&!vault2_create(&v,password,randoms,recovery));T2Io t={&v,v.h.blocks,fsrd,fswr,fsfl,al,fr};Tmc2 fs;assert(!tmc2_format(&fs,t));
 u64 file;assert(!tmc2_create(&fs,1,"encrypted-document",T2_FILE,&file));memset(plain,'D',sizeof plain);assert(tmc2_write(&fs,file,0,plain,sizeof plain)==sizeof plain);u8 account[512];memset(account,'P',512);assert(!tmc2_account(&fs,0,account,1));assert(!tmc2_snapshot(&fs,"encrypted-history",0));memset(plain,'E',sizeof plain);assert(tmc2_write(&fs,file,0,plain,sizeof plain)==sizeof plain);assert(!tmc2_checkpoint(&fs,"Copia locale"));assert(!tmc2_verify(&fs));tmc2_close(&fs);vault2_lock(&v);
 for(size_t i=0;i+16<m.units*FV2_UNIT;i++){assert(memcmp(m.stable+i,"TMCBFS2\0",8));assert(memcmp(m.stable+i,"DDDDDDDDDDDDDDDD",16));assert(memcmp(m.stable+i,"PPPPPPPPPPPPPPPP",16));}
 boot(&v);assert(!tmc2_mount(&fs,t));assert(tmc2_read(&fs,file,0,out,sizeof out)==sizeof out&&out[0]=='E');assert(!tmc2_snapshot_restore(&fs,"encrypted-history"));assert(tmc2_read(&fs,file,0,out,sizeof out)==sizeof out&&out[0]=='D');assert(!tmc2_account(&fs,0,account,0)&&account[0]=='P');assert(!tmc2_snapshot_verify(&fs,"Copia locale"));assert(!tmc2_collect(&fs)&&!tmc2_verify(&fs));tmc2_close(&fs);vault2_lock(&v);close_ram();
 puts("PASS REAL TMCBFS2 through production encryption: encrypted filesystem superblocks/trees/data/private account/CoW snapshots/local checkpoint, lock/reopen/mount/restore/GC and ciphertext-only backing (host adapter, NOT kernel/installer acceptance)");
}
int main(int argc,char**argv){assert(argc==2);seed();sparse(argv[1]);faults();filesystem();return 0;}
