/* Native monotonic waits and event queue, not WTF::RunLoop/WebKit. */
#include "lib.h"
#include "tmc_threads.h"
#include "tmc_heap.h"
#include "events_cases.h"
void *__dso_handle=&__dso_handle;
static unsigned cpu_mask;
void events_pause(void)
{
 SysInfo s;sc(S_INFO,&s,0,0,0);if(s.current_cpu<8)__atomic_fetch_or(&cpu_mask,1u<<s.current_cpu,__ATOMIC_RELAXED);sc(S_SLEEP,1,0,0,0);
}
static void say(const char*s){sc(S_STDOUT,s,strlen(s),0,0);}
static void num(u64 n){char b[24];number(b,n);say(b);}
void app_main(void)
{
 struct tmc_thread initial={0};
 if(tmc_runtime_enter(&initial)){say("EVENTS NATIVE FAIL startup\n");sc(S_RETURN,42,0,0,0);return;}
 unsigned failure=events_cases_run();SysInfo s;sc(S_INFO,&s,0,0,0);
 if(s.ncpu>1&&!(cpu_mask&(cpu_mask-1)))failure=__LINE__;
 struct tmc_heap_stats st=tmc_heap_statistics();if(tmc_runtime_leave())failure=__LINE__;
 say(failure?"EVENTS NATIVE FAIL\n":"EVENTS NATIVE SMP PASS\n");
 say("cpus=");num(s.ncpu);say(" producers=4 C++-tasks=400\n");
 say("clock=MONOTONIC/100Hz realtime=UNSUPPORTED\n");
 say("cond=absolute/timeout/relock/notify\n");
 say("queue=FIFO/nested/timers cancel-races=400\n");
 say("stop=active-finish/pending-cleanup/late-reject\n");
 say("VM-spans=");num(st.spans);say(" live=");num(st.live);say(" reserved=");num(st.reserved);say(" cpu-mask=");num(cpu_mask);
 say("\nfirst-failure-line=");num(failure);say("\n");sc(S_RETURN,failure?42:0,0,0,0);
}
