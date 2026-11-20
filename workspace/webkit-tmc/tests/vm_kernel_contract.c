/* Fixture prepended to the verbatim kernel/uservm.c by test-vm.py.
 * Simulated page tables/locks, NOT hardware protection or SMP testing. */
#include "base.h"
#include "vmabi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PAGE TMC_VM_PAGE
#define NX (1ull<<63)
#define ADDR 0x000ffffffffff000ull
int ncpu_online=1;
typedef struct {int held;} Spin;
static void spin_lock(Spin *s,u32 *f){assert(!s->held);s->held=1;*f=0;}
static void spin_unlock(Spin *s,u32 f){(void)f;assert(s->held);s->held=0;}
typedef struct {u64 *root;struct {u64 address,bytes,guard;}vm[TMC_VM_SLOTS];}Process;
static u64 pages[(TMC_VM_ARENA_END-TMC_VM_ARENA_START)/PAGE];
static unsigned count,adds;
static int fail_after=-1;
static u64 *vm_pte(u64 *r,u64 a,int create){
 (void)r;(void)create;
 if(a<TMC_VM_ARENA_START || a>=TMC_VM_ARENA_END)return 0;
 return &pages[(a-TMC_VM_ARENA_START)/PAGE];
}
static int vm_add(u64 *r,u64 a,u64 flags){
 u64 *e=vm_pte(r,a,0);assert(e&&!*e);
 if(fail_after>=0 && adds++>=(unsigned)fail_after)return 0;
 *e=(a&ADDR)|5|flags;count++;return 1;
}
static void vm_unmap(u64 *r,u64 a){u64 *e=vm_pte(r,a,0);if(e&&*e){*e=0;count--;}}
static int vm_access(u64 *r,u64 a,u64 protection){
 u64 *e=vm_pte(r,a,0);if(!e||!(*e&1))return 0;
 *e=(*e&ADDR)|1|NX|(protection&TMC_VM_READ?4:0)|(protection&TMC_VM_WRITE?2:0);return 1;
}
/* KERNEL_SOURCE */
#define OP(o,a,n,p) uservm_operation(&process,o,a,n,p)
#define R(n,a,g) OP(TMC_VM_RESERVE,n,a,g)
#define C(a,n,p) OP(TMC_VM_COMMIT,a,n,p)
#define P(a,n,p) OP(TMC_VM_PROTECT,a,n,p)
#define D(a,n) OP(TMC_VM_DECOMMIT,a,n,0)
#define F(a,n) OP(TMC_VM_RELEASE,a,n,0)
int main(void){
 Process process={0};struct TmcVMInfo caps;uservm_info(&caps);
 assert(caps.version==1&&(caps.flags&TMC_VM_AVAILABLE)&&caps.page_size==4096);
 assert(R(0,PAGE,0)==TMC_VM_INVALID);
 assert(R(PAGE-1,PAGE,0)==TMC_VM_INVALID);
 assert(R(PAGE,0,0)==TMC_VM_INVALID);
 assert(R(PAGE,3*PAGE,0)==TMC_VM_INVALID);
 assert(R(PAGE,PAGE,17)==TMC_VM_INVALID);
 assert(R(TMC_VM_MAX_REGION+PAGE,PAGE,0)==TMC_VM_INVALID);
 u64 a=(u64)R(3*PAGE,2*1024*1024,1);assert(a>=TMC_VM_ARENA_START&&!(a%(2*1024*1024))&&!count);
 assert(C(a-PAGE,PAGE,3)==TMC_VM_INVALID);
 assert(C(a+3*PAGE,PAGE,3)==TMC_VM_INVALID);
 assert(C(a,2*PAGE,3)==0&&count==2);
 assert(C(a,3*PAGE,3)==TMC_VM_BUSY&&count==2);
 assert(P(a,3*PAGE,1)==TMC_VM_NOT_COMMITTED&&(*vm_pte(0,a,0)&7)==7);
 assert(P(a,2*PAGE,1)==0&&(*vm_pte(0,a,0)&7)==5);
 assert(P(a,2*PAGE,0)==0&&(*vm_pte(0,a,0)&7)==1);
 assert(P(a,2*PAGE,7)==TMC_VM_UNSUPPORTED);
 assert(P(a,2*PAGE,2)==TMC_VM_UNSUPPORTED);
 assert(P(a,2*PAGE,3)==0&&(*vm_pte(0,a,0)&NX));
 assert(D(a,3*PAGE)==0&&!count);assert(D(a,3*PAGE)==0);
 fail_after=1;adds=0;assert(C(a,3*PAGE,3)==TMC_VM_NOMEM&&!count);fail_after=-1;
 for(unsigned i=0;i<3;i++)assert(!*vm_pte(0,a+i*PAGE,0));
 assert(C(a,3*PAGE,3)==0&&count==3);
 assert(F(a,PAGE)==TMC_VM_INVALID&&count==3);
 assert(F(a,3*PAGE)==0&&!count);assert(F(a,3*PAGE)==TMC_VM_INVALID);
 assert(R(PAGE,PAGE,0)==(i64)TMC_VM_ARENA_START);assert(F(TMC_VM_ARENA_START,PAGE)==0);
 for(unsigned i=0;i<32;i++)assert(R(PAGE,PAGE,1)>0);
 assert(R(PAGE,PAGE,0)==TMC_VM_NOMEM);
 for(unsigned i=0;i<32;i++)assert(F(process.vm[i].address,process.vm[i].bytes)==0);
 assert(OP(999,a,PAGE,0)==TMC_VM_INVALID);
 assert(C(~0ull-4095,PAGE,3)==TMC_VM_INVALID);
 assert(C(a+1,PAGE,3)==TMC_VM_INVALID);
 assert(D(a,0)==TMC_VM_INVALID);
 ncpu_online=8;uservm_info(&caps);assert(caps.flags&TMC_VM_AVAILABLE);
 i64 smp=R(PAGE,PAGE,0);assert(smp>0);assert(C(smp,PAGE,3)==0);assert(P(smp,PAGE,1)==0);assert(F(smp,PAGE)==0&&!count);ncpu_online=1;
 /* Deterministic first-fit/fragmentation/alignment and reservation lifetime. */
 unsigned rng=6841;
 for(unsigned k=0;k<50000;k++){
  rng=rng*1664525+1013904223;unsigned i=(rng>>8)%32;
  if(process.vm[i].bytes)assert(F(process.vm[i].address,process.vm[i].bytes)==0);
  else {
   u64 alignment=PAGE<<(rng%10),bytes=PAGE*(1+(rng%1024));
   i64 x=R(bytes,alignment,(rng>>12)%4);
   if(x>0)assert(!((u64)x&(alignment-1)));
   else assert(x==TMC_VM_NOMEM);
  }
  for(unsigned j=0;j<32;j++)if(process.vm[j].bytes){
   u64 lo=process.vm[j].address-process.vm[j].guard;
   u64 hi=process.vm[j].address+process.vm[j].bytes+process.vm[j].guard;
   assert(lo>=TMC_VM_ARENA_START&&hi<=TMC_VM_ARENA_END);
   for(unsigned z=j+1;z<32;z++)if(process.vm[z].bytes){
    u64 l=process.vm[z].address-process.vm[z].guard;
    u64 h=process.vm[z].address+process.vm[z].bytes+process.vm[z].guard;
    assert(hi<=l||h<=lo);
   }
  }
 }
 assert(!count);
 puts("PASS: kernel VM model, guards/ranges, permission prevalidation, OOM rollback, 50000 reservation operations, multi-CPU reservation path (host model, not MMU proof).");
}
