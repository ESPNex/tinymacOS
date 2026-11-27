#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tmcbfs2_seed.h"
#include "fs2_seed_fixture.h"
static u8 *disk;static int writes,fault=-1;
static int rd(void*x,u64 b,void*p){(void)x;assert(b<2048);memcpy(p,disk+b*T2_BLOCK,T2_BLOCK);return 0;}
static int wr(void*x,u64 b,const void*p){(void)x;assert(b<2048);if(writes++==fault)return -1;memcpy(disk+b*T2_BLOCK,p,T2_BLOCK);return 0;}
static int fl(void*x){(void)x;return 0;}
static void reseal(u8*p){u64 bytes;memcpy(&bytes,p+8,8);u32 c=~0u;for(u64 j=0;j<bytes;j++){c^=j>=24&&j<28?0:p[j];for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(c&1));}c=~c;memcpy(p+24,&c,4);}
int main(void){
 size_t cap=3*1024*1024;u8*source=malloc(cap);disk=calloc(2048,T2_BLOCK);assert(source&&disk);assert(!seed_fixture(source,cap,0));
 T2Io io={0,2048,rd,wr,fl,seed_al,seed_fr};Tmc2 fs;assert(!tmc2_seed_validate(source,cap));assert(!tmc2_format(&fs,io));assert(!tmc2_seed_import(&fs,source,cap));assert(!tmc2_seed_verify(&fs,source,cap));tmc2_close(&fs);
 assert(!tmc2_mount(&fs,io));assert(!tmc2_seed_verify(&fs,source,cap));assert(!tmc2_snapshot(&fs,"Installazione",0));assert(!tmc2_snapshot_verify(&fs,"Installazione"));
 T2Entry dir,e;assert(!tmc2_lookup(&fs,1,"Documents",&dir)&&!tmc2_lookup(&fs,dir.key,"proof.txt",&e));u8 value=255;assert(tmc2_write(&fs,e.key,0,&value,1)==1);assert(tmc2_seed_verify(&fs,source,cap)<0);assert(!tmc2_snapshot_restore(&fs,"Installazione"));assert(!tmc2_seed_verify(&fs,source,cap));
 /* Large UTF8 file and moved parent with a newer inode ID: export must order by ancestry, not IDs. */
 u64 later;assert(!tmc2_create(&fs,1,"new-parent",T2_DIR,&later));assert(!tmc2_move(&fs,dir.key,later,"caffè"));u8*payload=malloc(65537);assert(payload);for(unsigned j=0;j<65537;j++)payload[j]=(j*17+23)&255;
 assert(tmc2_write(&fs,e.key,0,payload,65537)==65537);assert(!tmc2_readonly(&fs,e.key,1));assert(!tmc2_seed_export(&fs,source,cap));assert(tmc2_seed_export(&fs,source,32)<0);tmc2_close(&fs);
 assert(!tmc2_format(&fs,io));assert(!tmc2_seed_import(&fs,source,cap)&&!tmc2_seed_verify(&fs,source,cap));tmc2_close(&fs);
 /* Valid-checksum structural corruption must also reject before any writes. */
 T2Entry first,second,third;memcpy(&first,source+1056,sizeof first);memcpy(&second,source+1152,sizeof second);memcpy(&third,source+1248,sizeof third);
 T2Entry bad=first;bad.parent=bad.key;memcpy(source+1056,&bad,sizeof bad);reseal(source);assert(tmc2_seed_validate(source,cap)<0);memcpy(source+1056,&first,sizeof first);
 bad=second;bad.key=first.key;memcpy(source+1152,&bad,sizeof bad);reseal(source);assert(tmc2_seed_validate(source,cap)<0);
 bad=second;bad.parent=1;strcpy(bad.name,first.name);memcpy(source+1152,&bad,sizeof bad);reseal(source);assert(tmc2_seed_validate(source,cap)<0);memcpy(source+1152,&second,sizeof second);
 bad=third;bad.size=~0ull;memcpy(source+1248,&bad,sizeof bad);reseal(source);assert(tmc2_seed_validate(source,cap)<0);
 bad=third;bad.name[0]='/';memcpy(source+1248,&bad,sizeof bad);reseal(source);assert(tmc2_seed_validate(source,cap)<0);memcpy(source+1248,&third,sizeof third);reseal(source);assert(!tmc2_seed_validate(source,cap));
 int before=writes;source[24]^=1;assert(tmc2_seed_validate(source,cap)<0&&writes==before);source[24]^=1;
 assert(tmc2_seed_validate(source,511)<0&&tmc2_seed_validate(source,1056)<0);
 u8*old=calloc(1,cap);assert(old);memcpy(old,"TMCBFS1",8);assert(tmc2_seed_validate(old,cap)<0);memcpy(old,"TINYFS1",8);assert(tmc2_seed_validate(old,cap)<0);free(old);
 for(int cut=0;cut<18;cut++){memset(disk,0,2048*T2_BLOCK);writes=0;fault=-1;assert(!tmc2_format(&fs,io));fault=writes+cut;assert(tmc2_seed_import(&fs,source,cap)<0);tmc2_close(&fs);}
 free(source);free(disk);free(payload);puts("PASS TMCFS2-only installer transport: real core export/import, 65537-byte UTF8 tree, ancestry order, readonly/accounts/remount/snapshot, checksum/truncation/old-format refusal and injected target write failures");
}
