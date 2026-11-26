#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include "tmc_pthread.h"
#include "pthread_cases.h"
void lifecycle_vm_fail_reserve(void);
void pthread_test_pause(void){struct timespec t={0,1000000};nanosleep(&t,0);}
uint64_t pthread_test_ticks(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*100+t.tv_nsec/10000000;}
static void *never(void*p){(void)p;assert(0);return 0;}
int main(void)
{
 struct tmc_thread initial={0};assert(!tmc_runtime_enter(&initial));
 tmc_pthread_t out=UINT64_MAX;errno=81;lifecycle_vm_fail_reserve();
 assert(tmc_pthread_create(&out,0,never,0)==EAGAIN&&out==UINT64_MAX&&errno==81);
 unsigned failure=pthread_cases_run();if(failure)fprintf(stderr,"PTHREAD first-failure-line=%u\n",failure);assert(!failure);
 assert(!tmc_runtime_leave());
 puts("PASS pthread subset: four workers/2000 protected updates + C++ RAII, once recursion/publication, owner/errorcheck/recursive mutex, condition predicate/reacquired timeout, errno, TSD reinsertion/destructors, child exit, stale IDs, concurrent join race, 16 thread reuses, actual task exhaustion/recovery, no final heap/DSO residue. No detach/cancel/realtime timedwait or full POSIX ABI claim.");
}
