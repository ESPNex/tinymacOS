/* Disposable genuine TMCFS2 source; no old filesystem image is synthesized. */
#include "tmcbfs2_seed.h"
static int seed_rd(void*x,u64 b,void*p){assert(b<512);memcpy(p,(u8*)x+b*T2_BLOCK,T2_BLOCK);return 0;}
static int seed_wr(void*x,u64 b,const void*p){assert(b<512);memcpy((u8*)x+b*T2_BLOCK,p,T2_BLOCK);return 0;}
static int seed_fl(void*x){(void)x;return 0;}
static void*seed_al(void*x,size_t n){(void)x;return malloc(n);}
static void seed_fr(void*x,void*p){(void)x;free(p);}
static int seed_fixture(void*p,size_t bytes,int invalid){
 u8*memory=calloc(512,T2_BLOCK);assert(memory);Tmc2 source;
 assert(!tmc2_format(&source,(T2Io){memory,512,seed_rd,seed_wr,seed_fl,seed_al,seed_fr}));u64 dir,file;
 assert(!tmc2_create(&source,1,"Documents",T2_DIR,&dir));assert(!tmc2_create(&source,dir,"proof.txt",T2_FILE,&file));
 u8 proof[8192];for(unsigned i=0;i<sizeof proof;i++)proof[i]=(i*17+13)%251;
 assert(tmc2_write(&source,file,0,proof,sizeof proof)==sizeof proof);
 u8 account[512];memset(account,19,512);assert(!tmc2_account(&source,0,account,1));memset(account,29,512);assert(!tmc2_account(&source,1,account,1));
 int r=tmc2_seed_export(&source,p,bytes);tmc2_close(&source);free(memory);if(!r&&invalid)((u8*)p)[24]^=1;return r;
}
