#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tmcbfs2_seed.h"
#include "nativefs.h"
typedef struct PACK{u8 kind,ro;u16 parent;u32 size,gen;char name[48];u32 reserved;} Node;
static u8 *disk,*source;static int writes,fault=-1;
static int rd(void*x,u64 b,void*p){(void)x;assert(b<2048);memcpy(p,disk+b*T2_BLOCK,T2_BLOCK);return 0;}
static int wr(void*x,u64 b,const void*p){(void)x;assert(b<2048);if(writes++==fault)return -1;memcpy(disk+b*T2_BLOCK,p,T2_BLOCK);return 0;}
static int fl(void*x){(void)x;return 0;}
static void*al(void*x,size_t n){(void)x;return malloc(n);}
static void fr(void*x,void*p){(void)x;free(p);}
static void put32(u8*p,u32 n){memcpy(p,&n,4);}
int main(void){
 source=calloc(NF_MAX_IMAGE,512);disk=calloc(2048,T2_BLOCK);assert(source&&disk);T2Io io={0,2048,rd,wr,fl,al,fr};
 memcpy(source,"TMCBFS1\0",8);put32(source+8,2);put32(source+12,384);put32(source+16,144);put32(source+20,1);
 Node*n=(void*)(source+NF_NODE*512);n[0]=(Node){.kind=1};
 n[1]=(Node){.kind=2,.parent=20,.size=8192,.ro=1};strcpy(n[1].name,"readonly.txt");
 n[20]=(Node){.kind=1,.parent=0};strcpy(n[20].name,"Documents");
 n[350]=(Node){.kind=2,.parent=20};strcpy(n[350].name,"empty.txt");
 for(int j=0;j<8192;j++)source[(NF_DATA+16)*512+j]=j%251;memset(source+13*512,0x19,512);memset(source+14*512,0x29,512);
 u8*sb=source+NF_SUPER*512;memcpy(sb+64,"_BHRfS_M",8);put32(sb,native_crc32c(sb+32,4064));
 assert(!tmc2_seed_validate(source,NF_MAX_IMAGE*512));Tmc2 fs;assert(!tmc2_format(&fs,io));assert(!tmc2_seed_import(&fs,source,NF_MAX_IMAGE*512));assert(!tmc2_seed_verify(&fs,source,NF_MAX_IMAGE*512));tmc2_close(&fs);
 assert(!tmc2_mount(&fs,io));assert(!tmc2_seed_verify(&fs,source,NF_MAX_IMAGE*512));assert(!tmc2_snapshot(&fs,"Installazione",0));assert(!tmc2_snapshot_verify(&fs,"Installazione"));
 T2Entry dir,e;assert(!tmc2_lookup(&fs,1,"Documents",&dir));assert(!tmc2_lookup(&fs,dir.key,"readonly.txt",&e)&&e.reserved==1);u8 value=255;assert(tmc2_write(&fs,e.key,0,&value,1)==1);assert(tmc2_seed_verify(&fs,source,NF_MAX_IMAGE*512)<0);assert(!tmc2_snapshot_restore(&fs,"Installazione"));assert(!tmc2_seed_verify(&fs,source,NF_MAX_IMAGE*512));tmc2_close(&fs);
 int before=writes;n[350].parent=350;assert(tmc2_seed_validate(source,NF_MAX_IMAGE*512)<0&&writes==before);n[350].parent=20;
 strcpy(n[350].name,"readonly.txt");assert(tmc2_seed_validate(source,NF_MAX_IMAGE*512)<0);strcpy(n[350].name,"empty.txt");
 n[350].name[0]='/';assert(tmc2_seed_validate(source,NF_MAX_IMAGE*512)<0);n[350].name[0]='e';put32(source+24,1);assert(tmc2_seed_validate(source,NF_MAX_IMAGE*512)<0);put32(source+24,0);
 assert(tmc2_seed_validate(source,511)<0&&tmc2_seed_validate(source,NF_MAX_IMAGE*512-1)<0);
 for(int cut=0;cut<18;cut++){
  memset(disk,0,2048*T2_BLOCK);writes=0;fault=-1;assert(!tmc2_format(&fs,io));fault=writes+cut;assert(tmc2_seed_import(&fs,source,NF_MAX_IMAGE*512)<0);tmc2_close(&fs);
 }
 free(source);free(disk);puts("PASS scalable fresh import: sparse/non-topological source IDs, full 8KiB file readback, readonly, both account copies, remount and shared-root backup restore; corrupt/cyclic/duplicate/encrypted/truncated source rejection; target write failures never report success");
}
