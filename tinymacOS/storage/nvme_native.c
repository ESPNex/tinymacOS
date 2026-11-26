/* SPDX-License-Identifier: MIT */
#include "nvme_native.h"
#define CAP 0x00u
#define INTMS 0x0cu
#define CC 0x14u
#define CSTS 0x1cu
#define AQA 0x24u
#define ASQ 0x28u
#define ACQ 0x30u
static u32 rd(NvController*c,u32 r){return c->os.read32(c->os.context,r);}
static void wr(NvController*c,u32 r,u32 v){c->os.write32(c->os.context,r,v);}
static void w64(NvController*c,u32 r,u64 v){wr(c,r,(u32)v);wr(c,r+4,v>>32);}
static void fence(void){__atomic_thread_fence(__ATOMIC_SEQ_CST);}
static int stop(NvController*c){c->dead=1;wr(c,CC,rd(c,CC)&~1u);return -1;}
static int ready(NvController*c,int want){
 u64 at=c->os.clock_ms(c->os.context);
 for(;;){u32 v=rd(c,CSTS);if(v&2)return -1;if((v&1)==(u32)want)return 0;
  if(c->os.clock_ms(c->os.context)-at>=c->ready_timeout_ms)return -1;
  c->os.poll(c->os.context);
 }
}
static int submit(NvController*c,NvQueue*q,unsigned qid,NvCommand*cmd){
 if(c->dead)return -1;
 u16 cid=++q->cid;cmd->op=(cmd->op&0xffffu)|(u32)cid<<16;
 memcpy(q->sq+q->tail,cmd,sizeof *cmd);fence();
 q->tail=(q->tail+1)%NV_DEPTH;wr(c,0x1000+2*qid*c->stride,q->tail);
 u64 at=c->os.clock_ms(c->os.context);
 for(;;){
  volatile NvCompletion*v=q->cq+q->head;
  u16 status=__atomic_load_n(&v->status,__ATOMIC_ACQUIRE);
  if((status&1)==q->phase){
   fence();u16 got=v->cid,sqid=v->sqid,sqhead=v->sqhead;
   if(got!=cid||sqid!=qid||sqhead>=NV_DEPTH)return stop(c);
   q->head=(q->head+1)%NV_DEPTH;if(!q->head)q->phase^=1;
   fence();wr(c,0x1000+(2*qid+1)*c->stride,q->head);
   return status>>1?-1:0;
  }
  if(rd(c,CSTS)&2 || c->os.clock_ms(c->os.context)-at>=30000)return stop(c);
  c->os.poll(c->os.context);
 }
}
static void text(char*out,const u8*in,unsigned n){
 memcpy(out,in,n);out[n]=0;while(n&&out[n-1]==' ')out[--n]=0;
 for(unsigned i=0;i<n;i++)if((u8)out[i]<32||(u8)out[i]>126)out[i]='?';
}
static int identify(NvController*c,u32 ns,u32 type){
 memset(c->identify,0,sizeof c->identify);
 NvCommand cmd={.op=6,.nsid=ns,.prp1=(u64)c->identify,.d={type}};
 return submit(c,&c->admin,0,&cmd);
}
static int namespace_add(NvController*c,u32 id){
 if(identify(c,id,0))return -1;
 const u8*p=c->identify;u64 blocks,capacity;memcpy(&blocks,p,8);memcpy(&capacity,p+8,8);
 unsigned fmt=(p[26]&15)|((p[26]>>1)&0x30),formats=p[25]+1;
 if(fmt>=formats||fmt>=64||!capacity||capacity>blocks||!blocks||(p[29]&7))return 0;
 u16 metadata;memcpy(&metadata,p+128+4*fmt,2);unsigned bits=p[130+4*fmt];
 if(metadata||(bits!=9&&bits!=12)||blocks>(~0ull>>(bits-9)))return 0;
 if(c->count==NV_NS_MAX){c->namespace_limit_reached=1;return 0;}
 c->ns[c->count++]=(NvNamespace){id,1u<<bits,blocks,blocks<<(bits-9)};
 return 0;
}
int nvme_controller_start(NvController*c,NvPlatform platform){
 if(!c||!platform.read32||!platform.write32||!platform.clock_ms||!platform.poll)return -1;
 c->os=platform;c->dead=1;
 u64 cap=(u64)rd(c,CAP)|(u64)rd(c,CAP+4)<<32;
 /* NVM command set; 4KiB host pages supported; both queue depths fit MQES. */
 if(cap==~0ull||!(cap&(1ull<<37))||(cap&0xffff)<NV_DEPTH-1||((cap>>48)&15))return -1;
 c->stride=4u<<((cap>>32)&15);c->ready_timeout_ms=MAX(500u,(u32)((cap>>24)&255)*500u);
 wr(c,INTMS,~0u);wr(c,CC,rd(c,CC)&~1u);
 if(ready(c,0))return -1;
 /* A restart must quiesce the old controller BEFORE zeroing DMA queues. */
 u32 stride=c->stride,timeout=c->ready_timeout_ms;
 memset(c,0,sizeof *c);c->os=platform;c->dead=1;c->stride=stride;c->ready_timeout_ms=timeout;
 c->admin.phase=c->io.phase=1;
 wr(c,AQA,(NV_DEPTH-1)|((NV_DEPTH-1)<<16));
 w64(c,ASQ,(u64)c->admin.sq);w64(c,ACQ,(u64)c->admin.cq);fence();
 wr(c,CC,1u|(6u<<16)|(4u<<20));if(ready(c,1))return stop(c);c->dead=0;
 if(identify(c,0,1))return stop(c);
 text(c->serial,c->identify+4,20);text(c->model,c->identify+24,40);
 unsigned mdts=c->identify[77];c->max_bytes=!mdts||mdts>=5?NV_BYTES:4096u<<mdts;
 /* One polled I/O CQ/SQ, contiguous queue pages, interrupts disabled. */
 NvCommand cmd={.op=5,.prp1=(u64)c->io.cq,.d={1u|((NV_DEPTH-1)<<16),1u}};
 if(submit(c,&c->admin,0,&cmd))return stop(c);
 cmd=(NvCommand){.op=1,.prp1=(u64)c->io.sq,.d={1u|((NV_DEPTH-1)<<16),1u|(1u<<16)}};
 if(submit(c,&c->admin,0,&cmd))return stop(c);
 /* Active NSID list (not the allocated namespace count), paged by last NSID.
  * Never assume namespace IDs are dense or that namespace 1 is present. */
 u32 after=0;
 for(;;){
  if(identify(c,after,2))return stop(c);
  u32 ids[1024];memcpy(ids,c->identify,sizeof ids);int end=0;
  for(unsigned i=0;i<1024;i++){
   u32 id=ids[i];if(!id){end=1;break;}
   if(id<=after||id==~0u)return stop(c);after=id;
   if(c->count==NV_NS_MAX){c->namespace_limit_reached=1;end=1;break;}
   if(namespace_add(c,id))return stop(c);
  }
  if(end)break;
 }
 return 0;
}
static int transfer(NvController*c,NvNamespace*n,u64 lba,u32 blocks,int write){
 u32 bytes=blocks*n->block_bytes;
 if(!blocks||bytes>c->max_bytes||blocks>65536||lba>=n->blocks||blocks>n->blocks-lba)return -1;
 NvCommand cmd={.op=write?1:2,.nsid=n->id,.prp1=(u64)c->bounce};
 if(bytes>4096){
  if(bytes<=8192)cmd.prp2=(u64)c->bounce+4096;
  else{for(unsigned j=1;j<(bytes+4095)/4096;j++)c->prp[j-1]=(u64)c->bounce+4096*j;cmd.prp2=(u64)c->prp;}
 }
 cmd.d[0]=(u32)lba;cmd.d[1]=lba>>32;cmd.d[2]=blocks-1;
 return submit(c,&c->io,1,&cmd);
}
int nvme_namespace_io(NvController*c,unsigned which,u64 sector,u32 count,void*buffer,int write){
 if(!c||c->dead||which>=c->count||!count||!buffer||(write!=0&&write!=1))return -1;
 NvNamespace*n=c->ns+which;u32 scale=n->block_bytes/512;
 if(sector>=n->sectors512||count>n->sectors512-sector)return -1;
 u8*p=buffer;
 while(count){
  u32 at=sector%scale,take;
  if(at||count<scale){
   take=MIN(count,scale-at);
   if(transfer(c,n,sector/scale,1,0))return -1;
   if(write){memcpy(c->bounce+at*512,p,take*512);if(transfer(c,n,sector/scale,1,1))return -1;}
   else memcpy(p,c->bounce+at*512,take*512);
  }else{
   take=MIN(count/scale,c->max_bytes/n->block_bytes)*scale;
   if(write)memcpy(c->bounce,p,take*512);
   if(transfer(c,n,sector/scale,take/scale,write))return -1;
   if(!write)memcpy(p,c->bounce,take*512);
  }
  p+=(size_t)take*512;sector+=take;count-=take;
 }
 return 0;
}
int nvme_namespace_flush(NvController*c,unsigned which){
 if(!c||c->dead||which>=c->count)return -1;
 NvCommand cmd={.op=0,.nsid=c->ns[which].id};return submit(c,&c->io,1,&cmd);
}
