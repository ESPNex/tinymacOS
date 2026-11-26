#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include "tmc_threads.h"
#include "stdlib_cases.h"
void stdlib_pause(void){struct timespec t={0,1000000};nanosleep(&t,0);}
static void trap(void(*entry)(void))
{
 pid_t child=fork();assert(child>=0);if(!child){struct rlimit zero={0,0};setrlimit(RLIMIT_CORE,&zero);entry();_exit(77);}
 int status;assert(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGILL);
}
int main(void)
{
 struct tmc_thread initial={0},workers[4]={0},dispatch={0};struct stdlib_job jobs[4];assert(!tmc_runtime_enter(&initial));
 unsigned failure=stdlib_setup(jobs);if(failure)fprintf(stderr,"STDLIB setup line=%u\n",failure);
 assert(!failure);
 assert(!tmc_thread_create(&dispatch,stdlib_dispatcher,0));
 for(unsigned w=0;w<4;w++)assert(!tmc_thread_create(workers+w,stdlib_worker,jobs+w));
 stdlib_start();
 for(unsigned w=0;w<4;w++){void*r;assert(!tmc_thread_join(workers+w,&r)&&r==jobs+w&&!jobs[w].handoff);}
 failure=stdlib_finish();assert(!tmc_thread_join(&dispatch,0));if(!failure)failure=stdlib_teardown();
 if(failure)fprintf(stderr,"STDLIB first-failure-line=%u\n",failure);
 assert(!failure);
 trap(stdlib_length_trap);trap(stdlib_support_trap);assert(!tmc_runtime_leave());
 puts("PASS real C++ templates/WTF dtoa component: vector/string/unique_ptr/function/algorithm/span/atomic, 1040 independent parse vectors, 8000 binary64 roundtrips, 400 function tasks/cleanup, 128 x87/SSE control-word context checks, native heap/cross-thread deletion and zero final residues. Expected SIGILL on length/security errors. NOT complete libstdc++/WTF/JSC.");
}
