#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include "tmc_threads.h"
#include "tmc_heap.h"
#include "cpp_cases.h"
void cpp_test_progress(const char*s){(void)s;}
void cpp_test_pause(void){struct timespec t={0,1000000};nanosleep(&t,0);}
static void traps(void (*entry)(void))
{
 pid_t p=fork();assert(p>=0);
 if(!p){struct rlimit no_core={0,0};setrlimit(RLIMIT_CORE,&no_core);entry();_exit(77);}
 int status=0;assert(waitpid(p,&status,0)==p);assert(WIFSIGNALED(status)&&WTERMSIG(status)==SIGILL);
}
int main(void)
{
 struct tmc_thread initial={0},threads[4]={0};struct cpp_job jobs[4];assert(!tmc_runtime_enter(&initial));
 assert(!cpp_test_basics());
 for(unsigned i=0;i<4;i++){jobs[i]=(struct cpp_job){78+i,1000,cpp_test_handoff(78+i)};assert(!tmc_thread_create(threads+i,cpp_test_worker,jobs+i));}
 cpp_test_start();
 for(unsigned i=0;i<4;i++){void*result=0;assert(!tmc_thread_join(threads+i,&result));assert(result==jobs+i&&!jobs[i].handoff);}
 assert(!cpp_test_finish());traps(cpp_test_oom_trap);traps(cpp_test_recursion_trap);
 assert(!tmc_runtime_leave());assert(!tmc_heap_statistics().spans);
 puts("PASS C++ runtime: new/delete scalar/array/sized/aligned/nothrow/placement, real OOM new_handler recovery, cross-thread deletion, 12000 destructors, contended once-only nested guards, abort/retry publication; host-only expected SIGILL on unhandled OOM and recursive initialization. No STL/exceptions/JSC claim.");
}
