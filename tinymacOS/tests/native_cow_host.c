/* Targeted CoW implementation test, not the final ISO/release matrix.
 * All devices are disposable RAM. No host device is opened. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nativecow.h"
#define SECTORS 4096u
#define FRAME 256u
#define LIMIT 3072u
static u8 *disk,*baseline;static int writes,cut=-1,torn;
static NativeCow a,b;
static int rd(void*x,u32 l,void*p){(void)x;assert(l<SECTORS);memcpy(p,disk+l*512,512);return 0;}
static int wr(void*x,u32 l,const void*p){(void)x;assert(l<SECTORS);if(writes++==cut){if(torn)memcpy(disk+l*512,p,231);return -1;}memcpy(disk+l*512,p,512);return 0;}
static CowIo io={0,SECTORS,rd,wr};
static CowEntry*ent(u8*p){return (void*)(p+32);}
static void fixture(void){
 memset(disk,0,SECTORS*512);memcpy(disk+NF_CAT*512,"SNAPCAT1",8);u32 rev=2;memcpy(disk+NF_CAT*512+8,&rev,4);memset(disk+200*512,'A',512);writes=0;cut=-1;torn=0;
 assert(!native_cow_enable(&a,io,FRAME,LIMIT));assert(a.enabled);
}
static void create(NativeCow*c,int k){
 u32 sum;assert(!native_cow_prepare(c,k,&sum));u8 cat[512];assert(!rd(0,NF_CAT,cat));CowEntry*e=ent(cat);snprintf(e[k].name,28,"point%d",k);e[k].id=k+1;e[k].base=(k+1)*FRAME;e[k].crc=sum;u32 count=8;memcpy(cat+12,&count,4);assert(!native_cow_write(c,NF_CAT,cat));
}
static void remove_snap(NativeCow*c,int k){u8 cat[512];assert(!rd(0,NF_CAT,cat));memset(ent(cat)+k,0,sizeof(CowEntry));assert(!native_cow_write(c,NF_CAT,cat));}
static void expect(NativeCow*c,int k,int value){u8 data[512];assert(!native_cow_read(c,(k+1)*FRAME+200,data));for(int i=0;i<512;i++)assert(data[i]==value);assert(!native_cow_verify(c,k));}
static u32 c32c(const u8*p,u32 n){u32 c=~0u;while(n--){c^=*p++;for(int j=0;j<8;j++)c=(c>>1)^(0x82f63b78u&-(c&1));}return ~c;}
int main(void){
 disk=malloc(SECTORS*512);baseline=malloc(SECTORS*512);assert(disk&&baseline);
 fixture();int emptywrites=writes;create(&a,0);create(&a,1);assert(!a.allocated);assert(writes-emptywrites<FRAME/4);
 u8 value[512];memset(value,'B',512);assert(!native_cow_write(&a,200,value));assert(a.allocated==1&&a.refs[0][200].location==a.refs[1][200].location);expect(&a,0,'A');expect(&a,1,'A');
 memset(value,'C',512);assert(!native_cow_write(&a,200,value)&&a.allocated==1);expect(&a,0,'A');
 create(&a,2);memset(value,'D',512);assert(!native_cow_write(&a,200,value)&&a.allocated==2);expect(&a,2,'C');
 assert(native_cow_mount(&b,io,FRAME,LIMIT)==1);expect(&b,0,'A');expect(&b,2,'C');
 remove_snap(&b,0);assert(b.allocated==2);remove_snap(&b,1);assert(b.allocated==1);remove_snap(&b,2);assert(!b.allocated);
 puts("PASS snapshots initially share every data block, two snapshots retain one common old block, repeated overwrite doesn't duplicate it, reboot preserves views, deletion frees only last references");
 fixture();create(&a,0);create(&a,1);memcpy(baseline,disk,SECTORS*512);
 for(int partial=0;partial<2;partial++)for(int fail=0;fail<12;fail++){
  memcpy(disk,baseline,SECTORS*512);cut=-1;assert(native_cow_mount(&a,io,FRAME,LIMIT)==1);writes=0;cut=fail;torn=partial;memset(value,'Z',512);(void)native_cow_write(&a,200,value);cut=-1;
  int r=native_cow_mount(&b,io,FRAME,LIMIT);assert(r==1);expect(&b,0,'A');expect(&b,1,'A');
  assert(disk[200*512]=='A'||disk[200*512]=='Z');for(int j=0;j<512;j++)assert(disk[200*512+j]==disk[200*512]);
 }
 puts("PASS interrupted/torn CoW map/catalog/live writes replay before mount, retaining the complete old snapshot block");
 /* Full-image legacy snapshots occupy overlapping future maps/pool. Import
  * them without a full RAM/disk clone; also exercise a gap in the old slots. */
 memset(disk,0,SECTORS*512);memcpy(disk+NF_CAT*512,"SNAPCAT1",8);u32 rev=2,count=8;memcpy(disk+NF_CAT*512+8,&rev,4);memcpy(disk+NF_CAT*512+12,&count,4);memset(disk+200*512,'N',512);
 for(int k=0;k<8;k+=3){memcpy(disk+(k+1)*FRAME*512,disk,FRAME*512);memset(disk+((k+1)*FRAME+200)*512,'0'+k,512);CowEntry*e=ent(disk+NF_CAT*512)+k;snprintf(e->name,28,"old%d",k);e->id=k+1;e->base=(k+1)*FRAME;e->crc=c32c(disk+e->base*512,FRAME*512);}
 cut=-1;assert(!native_cow_enable(&a,io,FRAME,LIMIT));for(int k=0;k<8;k+=3)expect(&a,k,'0'+k);
 puts("PASS legacy full snapshots migrate via overlap-safe in-place sector permutation, including gaps, with independent original checksums");
 /* A full pool must fail BEFORE modifying the live block, and deleting a
  * no-longer-needed snapshot must make writes possible again. */
 fixture();assert(native_cow_mount(&a,io,FRAME,LIMIT)==1);
 memset(disk+NC_SUPER*512,0,512);assert(!native_cow_enable(&a,io,FRAME,2048));int full=0;u32 failed_l=0;
 for(int k=0;k<8&&!full;k++){
  create(&a,k);memset(value,'a'+k,512);
  for(u32 l=1;l<FRAME;l++){
   if(l>=128&&l<144)continue;
   u8 previous[512];memcpy(previous,disk+l*512,512);int oldwrites=writes;int r=native_cow_write(&a,l,value);
   if(r==NC_SPACE){assert(writes==oldwrites&&!memcmp(previous,disk+l*512,512));failed_l=l;full=1;break;}assert(!r);
  }
 }
 assert(full);remove_snap(&a,0);assert(!native_cow_write(&a,failed_l,value));
 puts("PASS full shared-block pool refuses writes without changing live data; deleting the last relevant references permits writes again");
 free(disk);free(baseline);return 0;
}
