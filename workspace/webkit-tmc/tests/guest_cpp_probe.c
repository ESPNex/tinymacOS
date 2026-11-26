/* Actual native C++ allocation and local-static ABI. Diagnostic only. */
#include "lib.h"
#include "tmc_threads.h"
#include "tmc_heap.h"
#include "cpp_cases.h"
#include <errno.h>
static unsigned cpu_mask;
void cpp_test_pause(void)
{
 SysInfo s;sc(S_INFO,&s,0,0,0);if(s.current_cpu<8)__atomic_fetch_or(&cpu_mask,1u<<s.current_cpu,__ATOMIC_RELAXED);sc(S_SLEEP,1,0,0,0);
}
static void say(const char*s){sc(S_STDOUT,s,strlen(s),0,0);}
static void num(u64 n){char b[24];number(b,n);say(b);}
void cpp_test_progress(const char*s)
{
 static int opened;
 if(!opened){sc(S_WOPEN,570,190,"C++ runtime QA",0);opened=1;}
 Draw draws[2]={{.type=C_RECT,.w=568,.h=190,.ink=INK(I_BG)}, {.type=C_TEXT,.x=16,.y=32,.style=F_MONO,.ink=INK(I_TXT)}};
 strset(draws[1].text,s,100);sc(S_WDRAW,draws,2,"C++ runtime QA",0);
 say(s);say("\n");
}
void app_main(void)
{
 struct tmc_thread initial={0};
 if(tmc_runtime_enter(&initial)){say("CPP NATIVE FAIL startup\n");sc(S_RETURN,42,0,0,0);return;}
 unsigned failure=cpp_test_basics(),created=0;
 cpp_test_progress("CPP phase=worker-create");
 struct tmc_thread threads[4]={0};struct cpp_job jobs[4];
 for(unsigned i=0;i<4;i++){
  jobs[i]=(struct cpp_job){78+i,1000,cpp_test_handoff(78+i)};
  if(tmc_thread_create(threads+i,cpp_test_worker,jobs+i)){failure=__LINE__;break;}created++;
 }
 cpp_test_start();
 cpp_test_progress("CPP phase=worker-join");
 for(unsigned i=0;i<created;i++){
  void*result=0;int error;
  do {
   error=tmc_thread_join_until(threads+i,&result,sc(S_TICKS,0,0,0,0)+100);
   if(error==ETIMEDOUT){char progress[100]="CPP stages=";char n[24];number(n,cpp_test_snapshot());append(progress,n,100);cpp_test_progress(progress);}
  }while(error==ETIMEDOUT);
  if(error||result!=jobs+i||jobs[i].handoff)failure=__LINE__;
 }
 unsigned cpp_failure=cpp_test_finish();if(cpp_failure)failure=cpp_failure;
 if(created!=4)failure=__LINE__;
 SysInfo s;sc(S_INFO,&s,0,0,0);if(s.ncpu>1&&!(cpu_mask&(cpu_mask-1)))failure=__LINE__;
 struct tmc_heap_stats st=tmc_heap_statistics();if(tmc_runtime_leave())failure=__LINE__;
 say(failure?"CPP NATIVE FAIL\n":"CPP NATIVE SMP PASS\n");
 say("cpus=");num(s.ncpu);say(" workers=4 destructors=12000\n");
 say("new=scalar/array/aligned/nothrow handler=OOM-recovery\n");
 say("guards=once/nested/contended abort=retry\n");
 say("VM-spans=");num(st.spans);say(" live=");num(st.live);say(" reserved=");num(st.reserved);say(" cpu-mask=");num(cpu_mask);
 say("\nfirst-failure-line=");num(failure);say("\n");sc(S_RETURN,failure?42:0,0,0,0);
}
