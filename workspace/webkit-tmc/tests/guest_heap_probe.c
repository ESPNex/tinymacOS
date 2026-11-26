/* Uses the actual native VM, parking mutex, threads and per-thread errno. */
#include "lib.h"
#include "tmc_threads.h"
#include "tmc_vm.h"
#include <stdlib.h>
static void heap_cpu_sample(void);
#define HEAP_SAMPLE() heap_cpu_sample()
#include "heap_cases.h"
static unsigned cpu_mask;
static void heap_cpu_sample(void){SysInfo s;sc(S_INFO,&s,0,0,0);if(s.current_cpu<8)__atomic_fetch_or(&cpu_mask,1u<<s.current_cpu,__ATOMIC_RELAXED);sc(S_SLEEP,0,0,0,0);}
extern int heap_cpp_smoke(void);
static void say(const char*s){sc(S_STDOUT,s,strlen(s),0,0);}
static void num(u64 n){char b[24];number(b,n);say(b);}
void app_main(void)
{
 struct tmc_thread initial=TMC_THREAD_INITIALIZER;
 if(tmc_runtime_enter(&initial)){say("HEAP NATIVE FAIL startup\n");sc(S_RETURN,42,0,0,0);return;}
 heap_sequential();
 /* Exercise actual libc symbol exports separately from prefixed API tests. */
 unsigned char*p=calloc(128,32);HC(p);if(p){for(unsigned i=0;i<4096;i++)HC(!p[i]);fill(p,4096,45);void*q=realloc(p,16384);HC(q);if(q){HC(intact(q,4096,45));free(q);}else free(p);}
 HC(!heap_cpp_smoke());
 struct tmc_thread workers[4]={0};struct HeapJob jobs[4];unsigned created=0;
 for(unsigned i=0;i<4;i++){
  jobs[i]=(struct HeapJob){78+i,1000,tmc_heap_alloc(1024)};HC(jobs[i].transfer);if(!jobs[i].transfer)break;
  fill(jobs[i].transfer,1024,jobs[i].seed);
  int r=tmc_thread_create(workers+i,heap_worker,jobs+i);HC(!r);if(r){tmc_heap_free(jobs[i].transfer);break;}created++;
 }
 for(unsigned i=0;i<created;i++){void*result=0;HC(!tmc_thread_join(workers+i,&result));HC(result==jobs+i&&!jobs[i].transfer);}
 HC(created==4);struct tmc_heap_stats st=tmc_heap_statistics();HC(!st.spans&&!st.reserved&&!st.live&&!st.requested);
 SysInfo s;sc(S_INFO,&s,0,0,0);
 if(s.ncpu>1)HC(cpu_mask&(cpu_mask-1));
 HC(!tmc_runtime_leave());
 say(heap_failure?"HEAP NATIVE FAIL\n":"HEAP NATIVE SMP PASS\n");
 say("cpus=");num(s.ncpu);say(" workers=4 random-ops=9000\n");
 say("libc=malloc/calloc/realloc/free/aligned_alloc\nCXX=RAII/C-allocation ABI\nVM-spans=");num(st.spans);
 say(" live=");num(st.live);say(" reserved=");num(st.reserved);say(" cpu-mask=");num(cpu_mask);say("\nfirst-failure-line=");num(heap_failure);say("\n");
 sc(S_RETURN,heap_failure?42:0,0,0,0);
}
