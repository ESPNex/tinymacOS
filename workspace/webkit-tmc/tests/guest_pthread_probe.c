/* Actual native source-level pthread consumer, not hosted libpthread. */
#include "lib.h"
#include "tmc_pthread.h"
#include "tmc_heap.h"
#include "pthread_cases.h"
void *__dso_handle=&__dso_handle;
static unsigned cpu_mask;
void pthread_test_pause(void)
{
 SysInfo s;sc(S_INFO,&s,0,0,0);if(s.current_cpu<8)__atomic_fetch_or(&cpu_mask,1u<<s.current_cpu,__ATOMIC_RELAXED);sc(S_SLEEP,1,0,0,0);
}
uint64_t pthread_test_ticks(void){return sc(S_TICKS,0,0,0,0);}
static void say(const char*s){sc(S_STDOUT,s,strlen(s),0,0);}
static void num(u64 n){char b[24];number(b,n);say(b);}
void app_main(void)
{
 struct tmc_thread initial={0};
 if(tmc_runtime_enter(&initial)){say("PTHREAD NATIVE FAIL startup\n");sc(S_RETURN,42,0,0,0);return;}
 unsigned failure=pthread_cases_run();SysInfo s;sc(S_INFO,&s,0,0,0);
 if(s.ncpu>1&&!(cpu_mask&(cpu_mask-1)))failure=__LINE__;
 struct tmc_heap_stats st=tmc_heap_statistics();if(tmc_runtime_leave())failure=__LINE__;
 say(failure?"PTHREAD NATIVE FAIL\n":"PTHREAD NATIVE SMP PASS\n");
 say("cpus=");num(s.ncpu);say(" workers=4 updates=2000\n");
 say("C++=RAII/local-static/new/DSO once=1 TSD-dtors=8\n");
 say("join=race/stale/reuse capacity=exhaust/recover\n");
 say("mutex=owner/recursive cond=predicate/timeout\n");
 say("VM-spans=");num(st.spans);say(" live=");num(st.live);say(" reserved=");num(st.reserved);say(" cpu-mask=");num(cpu_mask);
 say("\nfirst-failure-line=");num(failure);say("\n");sc(S_RETURN,failure?42:0,0,0,0);
}
