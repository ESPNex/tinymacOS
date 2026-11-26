/* Real header-template C++ and a bounded WTF numeric-component port. */
#include "lib.h"
#include "tmc_threads.h"
#include "tmc_heap.h"
#include "stdlib_cases.h"
void *__dso_handle=&__dso_handle;
static unsigned cpu_mask;
void stdlib_pause(void){SysInfo s;sc(S_INFO,&s,0,0,0);if(s.current_cpu<8)__atomic_fetch_or(&cpu_mask,1u<<s.current_cpu,__ATOMIC_RELAXED);sc(S_SLEEP,1,0,0,0);}
static void say(const char*s){sc(S_STDOUT,s,strlen(s),0,0);}
static void num(u64 n){char b[24];number(b,n);say(b);}
void app_main(void)
{
 struct tmc_thread initial={0},workers[4]={0},dispatcher={0};struct stdlib_job jobs[4];unsigned failure=0;
 if(tmc_runtime_enter(&initial)){say("STDLIB NATIVE FAIL startup\n");sc(S_RETURN,42,0,0,0);return;}
 failure=stdlib_setup(jobs);
 if(!failure){
  if(tmc_thread_create(&dispatcher,stdlib_dispatcher,0))__builtin_trap();
  for(unsigned w=0;w<4;w++)if(tmc_thread_create(workers+w,stdlib_worker,jobs+w))__builtin_trap();
  stdlib_start();
  for(unsigned w=0;w<4;w++){void*r=0;if(tmc_thread_join(workers+w,&r)||r!=jobs+w||jobs[w].handoff)failure=__LINE__;}
  unsigned shared=stdlib_finish();if(shared)failure=shared;
  if(tmc_thread_join(&dispatcher,0))failure=__LINE__;
  shared=stdlib_teardown();if(shared)failure=shared;
 }
 SysInfo s;sc(S_INFO,&s,0,0,0);if(s.ncpu>1&&!(cpu_mask&(cpu_mask-1)))failure=__LINE__;
 struct tmc_heap_stats st=tmc_heap_statistics();if(tmc_runtime_leave())failure=__LINE__;
 say(failure?"STDLIB NATIVE FAIL\n":"STDLIB NATIVE SMP PASS\n");
 say("cpus=");num(s.ncpu);say(" workers=4 C++-tasks=400\n");
 say("std=vector/string/unique_ptr/function/span/atomic\n");
 say("WTF-dtoa=parse-vectors=1040 roundtrips=8000\n");
 say("FP-context=x87/SSE2 checks=128 AVX=no\n");
 say("VM-spans=");num(st.spans);say(" live=");num(st.live);say(" reserved=");num(st.reserved);say(" cpu-mask=");num(cpu_mask);
 say("\nfirst-failure-line=");num(failure);say("\n");sc(S_RETURN,failure?42:0,0,0,0);
}
