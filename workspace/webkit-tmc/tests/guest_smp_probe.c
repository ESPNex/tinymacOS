/* QA native ABI SMP stress: real threads, no fake pthread/errno implementation. */
#include "lib.h"
#include "vmabi.h"
int errno; /* Linked adapters require it; stress workers use native calls only. */
static volatile u32 ready,go,done,failed,mask,reads;
static volatile u64 shared;
static u32 ncpu;
static void say(const char *s){sc(S_STDOUT,s,strlen(s),0,0);}
static void number_out(unsigned n){char b[24];number(b,n);say(b);}
static long vm(u64 op,u64 a,u64 n,u64 p){return sc(S_VM,op,a,n,p);}
static void fail(void){__atomic_store_n(&failed,1,__ATOMIC_RELEASE);}
static void sample(void){SysInfo info;if(sc(S_INFO,&info,0,0,0)||info.current_cpu>=8)fail();else __atomic_fetch_or(&mask,1u<<info.current_cpu,__ATOMIC_RELAXED);}
static void worker(u64 id)
{
 __atomic_add_fetch(&ready,1,__ATOMIC_RELEASE);
 while(!__atomic_load_n(&go,__ATOMIC_ACQUIRE))slp(1);
 for(unsigned k=0;k<100;k++){
  sample();u64 a=vm(TMC_VM_RESERVE,8192,4096,1);
  if((i64)a<0){fail();break;}
  volatile u8 *p=(void *)a;
  if(vm(TMC_VM_COMMIT,a,8192,3)){fail();break;}
  for(unsigned i=0;i<8192;i++)p[i]=(u8)(i+k+id*29);
  if(vm(TMC_VM_PROTECT,a,8192,1)){fail();break;}
  for(unsigned i=0;i<8192;i++)if(p[i]!=(u8)(i+k+id*29)){fail();break;}
  if(vm(TMC_VM_PROTECT,a,8192,0)||sc(S_STDOUT,p,1,0,0)!=-1||vm(TMC_VM_PROTECT,a,8192,3)){fail();break;}
  if(vm(TMC_VM_DECOMMIT,a,8192,0)||vm(TMC_VM_COMMIT,a,8192,3)){fail();break;}
  for(unsigned i=0;i<8192;i++)if(p[i]){fail();break;}
  if(vm(TMC_VM_RELEASE,a,8192,0)){fail();break;}
  long old=sc(S_ALLOC,4096,0,0,0);
  if(old<0){fail();break;}*(volatile u64 *)old=id^k;
  if(sc(S_FREE,old,0,0,0)){fail();break;}
  /* Kernel OUT buffer races mappings changed by main, never accessed directly here. */
  for(int j=0;j<4;j++){
   long r=sc(S_INFO,__atomic_load_n(&shared,__ATOMIC_ACQUIRE),0,0,0);
   if(r!=0&&r!=-1)fail();
   __atomic_add_fetch(&reads,1,__ATOMIC_RELAXED);
  }
 }
 __atomic_add_fetch(&done,1,__ATOMIC_RELEASE);
 sc(S_EXIT,1,0,0,0);for(;;){}
}
void app_main(void)
{
 SysInfo info;sc(S_INFO,&info,0,0,0);ncpu=info.ncpu;
 shared=vm(TMC_VM_RESERVE,4096,4096,1);
 if((i64)shared<0||vm(TMC_VM_COMMIT,shared,4096,3)){say("SMP FAIL initial\n");sc(S_RETURN,42,0,0,0);return;}
 for(unsigned i=0;i<4;i++)if(sc(S_THREAD,worker,i+1,0,0)<0){say("SMP FAIL thread creation\n");sc(S_RETURN,42,0,0,0);return;}
 while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=4)slp(1);
 __atomic_store_n(&go,1,__ATOMIC_RELEASE);
 unsigned cycles=0;u64 until=info.ticks+15000;
 while(__atomic_load_n(&done,__ATOMIC_ACQUIRE)!=4){
  if(vm(TMC_VM_PROTECT,shared,4096,0)||vm(TMC_VM_PROTECT,shared,4096,3)||vm(TMC_VM_DECOMMIT,shared,4096,0)||vm(TMC_VM_COMMIT,shared,4096,3)){fail();break;}
  cycles++;sample();sc(S_INFO,&info,0,0,0);if(info.ticks>until){fail();break;}slp(1);
 }
 if(vm(TMC_VM_RELEASE,shared,4096,0))fail();
 if(ncpu>1 && !(mask&(mask-1)))fail();
 if(__atomic_load_n(&done,__ATOMIC_ACQUIRE)!=4||reads!=1600)fail();
 say(failed?"SMP FAIL stress\n":"VM SMP STRESS PASS\n");
 say("cpus=");number_out(ncpu);say(" workers=4 cpu-mask=");number_out(mask);say("\n");
 say("worker-rounds=400 uaccess-races=");number_out(reads);say("\nmap-cycles=");number_out(cycles);say("\n");
 sc(S_RETURN,failed?42:0,0,0,0);
}
