/* Real native managed-thread/TSD/finish tests + freestanding C++ consumer. */
#define TMC_NATIVE_PROBE 1
#include "lib.h"
#include "tmc_vm.h"
#include "threadfinishabi.h"
#include "thread_cases.h"
static volatile u32 cpu_mask;
static void test_sample_cpu(void)
{
 SysInfo s;CHECK(!sc(S_INFO,&s,0,0,0));
 if(s.current_cpu<8)__atomic_fetch_or(&cpu_mask,1u<<s.current_cpu,__ATOMIC_RELAXED);else CHECK(0);
}
static int test_vm_lifetime(void)
{
 if(sizeof(struct tmc_thread)>4096)return -1;
 for(unsigned i=0;i<16;i++){
  struct tmc_thread *t=tmc_vm_reserve(4096,4096,1);if(!t)return -1;
  if(tmc_vm_commit(t,4096,TMC_VM_READ|TMC_VM_WRITE))return -1;
  memset(t,0,sizeof *t);reuse_data.reentrant=0;
  if(tmc_thread_create(t,fast_worker,&reuse_data))return -1;
  void *result=0;
  if(tmc_thread_join_until(t,&result,tick()+2000) || result!=&reuse_data || reuse_data.reentrant!=1)return -1;
  /* This mapping contains the actual child FS/TCB. No user code may still
   * access it after completion, including a trailing cleanup/uninstall. */
  if(tmc_vm_release(t,4096))return -1;
 }
 return 16;
}
static void say(const char *s){sc(S_STDOUT,s,strlen(s),0,0);}
static void num(u64 n){char b[24];number(b,n);say(b);}
void app_main(void)
{
 CHECK(sc(S_THREADFINISH,TMC_THREAD_FINISH_QUERY,0,0,0)==TMC_THREAD_FINISH_VERSION);
 CHECK(sc(S_THREADFINISH,TMC_THREAD_FINISH,1,0,0)==-1);
 CHECK(sc(S_THREADFINISH,TMC_THREAD_FINISH,(u64)app_main,0,0)==-1);
 CHECK(sc(S_THREADFINISH,99,0,0,0)==-1);
 u32 untouched=1;
 CHECK(sc(S_THREADFINISH,TMC_THREAD_FINISH,(u64)&untouched,1,0)==-1 && untouched==1);
 run_thread_cases();SysInfo s;sc(S_INFO,&s,0,0,0);
 CHECK(created==90+resource_capacity);
 if(s.ncpu>1)CHECK(cpu_mask&(cpu_mask-1));
 int result=(int)first_failure;
 say(result?"THREAD RUNTIME FAIL\n":"THREAD RUNTIME SMP PASS\n");
 say("base-created=");num(created-resource_capacity);say(" cpus=");num(s.ncpu);say(" updates=");num(counter);
 say("\nstartup=TLS join=kernel-final-handoff\nTSD=generations dtors=4-bounded\nCXX=RAII/C-ABI VM-unmap=16\nfirst-failure-line=");num(first_failure);say(" cpu-mask=");num(cpu_mask);say("\nresource-slots=");num(resource_capacity);say(" failure=EAGAIN recovered\n");
 sc(S_RETURN,result?42:0,0,0,0);
}
