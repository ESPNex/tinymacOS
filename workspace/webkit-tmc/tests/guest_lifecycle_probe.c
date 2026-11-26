/* Explicit native C++ process lifecycle, no hosted CRT. QA ONLY. */
#include "lib.h"
#include "tmc_threads.h"
#include "tmc_heap.h"
#include "lifecycle_cases.h"
void *__dso_handle=&__dso_handle;
extern tmc_cpp_initializer __tmc_init_array_start[],__tmc_init_array_end[];
static unsigned cpu_mask;
void lifecycle_pause(void)
{
 SysInfo s;sc(S_INFO,&s,0,0,0);if(s.current_cpu<8)__atomic_fetch_or(&cpu_mask,1u<<s.current_cpu,__ATOMIC_RELAXED);sc(S_SLEEP,1,0,0,0);
}
static void say(const char*s){sc(S_STDOUT,s,strlen(s),0,0);}
static void num(u64 n){char b[24];number(b,n);say(b);}
void app_main(void)
{
 struct tmc_thread initial={0},threads[4]={0};unsigned failure=0;
 if(tmc_runtime_enter(&initial)){say("LIFECYCLE NATIVE FAIL startup\n");sc(S_RETURN,42,0,0,0);return;}
 failure=lifecycle_setup(__tmc_init_array_start,__tmc_init_array_end);
 for(unsigned phase=0;phase<2&&!failure;phase++){
  unsigned created=0;
  for(unsigned i=0;i<4;i++){
   if(tmc_thread_create(threads+i,phase?lifecycle_finalizer:lifecycle_worker,(void*)(u64)i)){failure=__LINE__;break;}created++;
  }
  for(unsigned i=0;i<created;i++){void *result=0;if(tmc_thread_join(threads+i,&result)||result!=(void*)(u64)i)failure=__LINE__;}
  if(created!=4)failure=__LINE__;
 }
 unsigned cases_failure=lifecycle_finish();if(cases_failure)failure=cases_failure;
 SysInfo s;sc(S_INFO,&s,0,0,0);if(s.ncpu>1&&!(cpu_mask&(cpu_mask-1)))failure=__LINE__;
 struct tmc_heap_stats st=tmc_heap_statistics();unsigned long pending=tmc_cpp_pending_destructors();if(tmc_runtime_leave())failure=__LINE__;
 say(failure?"LIFECYCLE NATIVE FAIL\n":"LIFECYCLE NATIVE SMP PASS\n");
 say("cpus=");num(s.ncpu);say(" workers=4 registrations=400 callbacks=401\n");
 say("global-ctors=2 local-static=1 dtors=3\n");
 say("DSO=LIFO recursive=yes callback-registration=yes\n");
 say("pending=");num(pending);
 say(" VM-spans=");num(st.spans);say(" live=");num(st.live);say(" reserved=");num(st.reserved);
 say("\nfirst-failure-line=");num(failure);say(" cpu-mask=");num(cpu_mask);say("\n");sc(S_RETURN,failure?42:0,0,0,0);
}
