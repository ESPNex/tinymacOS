/* Actual production NVMe transport with an independent register/queue model.
 * Not hardware certification: QEMU guest tests exercise a separate controller.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvme_native.h"
static NvController c;
static struct {
 u32 regs[0x2000/4];NvCommand*sq[2];NvCompletion*cq[2];u16 head[2],tail[2],phase[2];
 u64 ms;int drop,badcid,error,writes,reads,flushes;u32 mdts;
 u8 small[131072],high[131072],kn[131072];
} hw;
static u32 r32(void*x,u32 r){(void)x;return hw.regs[r/4];}
static void scatter(NvCommand*q,void*memory,u32 n,int write){
 for(u32 off=0;off<n;){
  u32 amount=n-off;if(amount>4096)amount=4096;u64 p;
  if(!off)p=q->prp1;
  else if(n<=8192)p=q->prp2;
  else p=((u64*)q->prp2)[off/4096-1];
  assert(p&&!(p&4095));if(write)memcpy((u8*)memory+off,(void*)p,amount);else memcpy((void*)p,(u8*)memory+off,amount);off+=amount;
 }
}
static void execute(unsigned queue,unsigned newtail){
 unsigned sqid=queue;NvCommand*q=hw.sq[queue]+hw.head[queue];u16 cid=q->op>>16;unsigned op=q->op&255;u16 status=0;
 assert(newtail==(hw.head[queue]+1)%NV_DEPTH);
 if(hw.drop)return;
 if(hw.error){status=2;hw.error=0;}
 else if(!queue){
  if(op==6){
   u8 p[4096]={0};unsigned type=q->d[0]&255;
   if(type==1){memset(p+4,' ',60);memcpy(p+4,"MODEL-SERIAL",12);memcpy(p+24,"Independent NVMe model",22);p[77]=hw.mdts;u32 nn=7;memcpy(p+516,&nn,4);}
   else if(type==2){u32 ids[]={1,7,0};if(!q->nsid)memcpy(p,ids,sizeof ids);}
   else if(type==0){assert(q->nsid==1||q->nsid==7);u64 blocks=q->nsid==1?2147483648ull:8192;memcpy(p,&blocks,8);memcpy(p+8,&blocks,8);p[25]=0;p[26]=0;p[130]=q->nsid==1?9:12;}
   else assert(0);
   scatter(q,p,4096,0);
  }else if(op==5){assert((q->d[0]&0xffff)==1&&(q->d[1]&3)==1);hw.cq[1]=(void*)q->prp1;hw.phase[1]=1;}
  else if(op==1){assert((q->d[0]&0xffff)==1&&q->d[1]==65537);hw.sq[1]=(void*)q->prp1;}
  else assert(0);
 }else{
  assert(q->nsid==1||q->nsid==7);
  if(!op)hw.flushes++;
  else{
   assert(op==1||op==2);u64 lba=(u64)q->d[0]|(u64)q->d[1]<<32;
   u32 size=q->nsid==1?512:4096,n=((q->d[2]&65535)+1)*size;u8*p;
   assert(n<=NV_BYTES&&lba<(q->nsid==1?2147483648ull:8192));
   if(q->nsid==7){assert(lba*size+n<=sizeof hw.kn);p=hw.kn+lba*size;}
   else if(lba>=2147483392ull){assert((lba-2147483392ull)*size+n<=sizeof hw.high);p=hw.high+(lba-2147483392ull)*size;}
   else{assert(lba*size+n<=sizeof hw.small);p=hw.small+lba*size;}
   scatter(q,p,n,op==1);if(op==1)hw.writes++;else hw.reads++;
  }
 }
 hw.head[queue]=newtail;
 NvCompletion*done=hw.cq[queue]+hw.tail[queue];*done=(NvCompletion){.sqhead=newtail,.sqid=sqid,.cid=cid+hw.badcid,.status=(u16)(status<<1|hw.phase[queue])};hw.badcid=0;
 hw.tail[queue]=(hw.tail[queue]+1)%NV_DEPTH;if(!hw.tail[queue])hw.phase[queue]^=1;
}
static void w32(void*x,u32 r,u32 v){
 (void)x;hw.regs[r/4]=v;
 if(r==0x14){hw.regs[0x1c/4]=v&1;if(v&1){hw.sq[0]=(void*)((u64)hw.regs[0x28/4]|(u64)hw.regs[0x2c/4]<<32);hw.cq[0]=(void*)((u64)hw.regs[0x30/4]|(u64)hw.regs[0x34/4]<<32);hw.phase[0]=1;}}
 if(r==0x1000)execute(0,v);
 if(r==0x1008)execute(1,v);
}
static u64 clock_ms(void*x){(void)x;return hw.ms++;}
static void poll(void*x){(void)x;}
static void init(unsigned mdts){
 memset(&hw,0,sizeof hw);hw.regs[0]=(NV_DEPTH-1)|(1<<24);hw.regs[1]=1u<<5;hw.mdts=mdts;
 assert(!nvme_controller_start(&c,(NvPlatform){0,r32,w32,clock_ms,poll}));
 assert(c.count==2&&c.ns[0].id==1&&c.ns[1].id==7&&c.ns[0].sectors512==2147483648ull);
 assert(c.ns[1].block_bytes==4096&&c.ns[1].sectors512==65536);assert(!strcmp(c.model,"Independent NVMe model"));
}
int main(void){
 static u8 a[NV_BYTES],b[NV_BYTES];for(unsigned j=0;j<sizeof a;j++)a[j]=(j*31+7)%251;
 init(0);assert(!nvme_namespace_io(&c,0,2147483392ull,256,a,1));assert(!nvme_namespace_flush(&c,0));assert(!nvme_namespace_io(&c,0,2147483392ull,256,b,0));assert(!memcmp(a,b,sizeof a));assert(hw.writes==1&&hw.reads==1&&hw.flushes==1);
 puts("PASS actual NVMe transport: 1TiB high-LBA 128KiB PRP-list write/flush/read, sparse NSIDs 1/7, namespace/model discovery");
 for(unsigned i=0;i<100;i++){assert(!nvme_namespace_io(&c,0,0,16,a,1));assert(!nvme_namespace_io(&c,0,0,16,b,0));assert(!memcmp(a,b,8192));}
 puts("PASS repeated queue wrap/phase/CID handling and direct two-page PRPs");
 memset(hw.kn,0x5a,sizeof hw.kn);memset(a,0x83,512);assert(!nvme_namespace_io(&c,1,3,1,a,1));
 for(unsigned j=0;j<4096;j++)assert(hw.kn[j]==(j>=1536&&j<2048?0x83:0x5a));
 assert(!nvme_namespace_io(&c,1,2,3,b,0));for(unsigned j=0;j<1536;j++)assert(b[j]==(j>=512&&j<1024?0x83:0x5a));
 puts("PASS real 4Kn partial-sector RMW preserves every neighboring byte (not a 4Kn boot/partition claim)");
 int before=hw.writes;assert(nvme_namespace_io(&c,0,2147483648ull,1,a,1)<0);assert(nvme_namespace_io(&c,1,65535,2,a,1)<0);assert(nvme_namespace_io(&c,7,0,1,a,1)<0);assert(nvme_namespace_io(&c,0,0,0,a,1)<0);assert(hw.writes==before);
 hw.error=1;assert(nvme_namespace_io(&c,0,0,1,b,0)<0&&!c.dead);assert(!nvme_namespace_io(&c,0,0,1,b,0));
 puts("PASS out-of-range/namespace/zero-count refusal without writes; NVMe error completion consumed without false success");
 init(1);assert(c.max_bytes==8192);assert(!nvme_namespace_io(&c,0,0,256,a,1));assert(hw.writes==16);assert(!memcmp(hw.small,a,sizeof a));
 puts("PASS MDTS-limited 128KiB request splits into sixteen independently completed 8KiB commands");
 hw.badcid=1;assert(nvme_namespace_io(&c,0,0,1,b,0)<0&&c.dead);before=hw.writes;assert(nvme_namespace_io(&c,0,0,1,b,1)<0&&hw.writes==before);
 init(0);hw.drop=1;assert(nvme_namespace_flush(&c,0)<0&&c.dead&&!(hw.regs[0x14/4]&1));
 puts("PASS CID mismatch and command timeout disable/fail the controller; no retry or success claim after unknown write outcome");
 return 0;
}
