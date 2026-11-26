#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include "tmc_pthread.h"
#include "tmc_runloop.h"
#include "events_cases.h"
void lifecycle_vm_fail_reserve(void);
void events_pause(void){struct timespec t={0,1000000};nanosleep(&t,0);}
static unsigned cleanup_count;
static void never(void*p){(void)p;assert(0);}
static void cleanup(void*p){(void)p;cleanup_count++;}
int main(void)
{
 struct tmc_thread initial={0};assert(!tmc_runtime_enter(&initial));
 struct tmc_runloop loop=TMC_RUNLOOP_INITIALIZER;uint64_t out=UINT64_MAX;
 lifecycle_vm_fail_reserve();errno=81;
 assert(tmc_runloop_post(&loop,0,never,0,cleanup,&out)==ENOMEM&&out==UINT64_MAX&&!cleanup_count&&errno==81);
 assert(!tmc_runloop_stop(&loop)&&!tmc_runloop_destroy(&loop));
 unsigned failure=events_cases_run();if(failure)fprintf(stderr,"EVENTS first-failure-line=%u\n",failure);assert(!failure);
 assert(!tmc_runtime_leave());
 puts("PASS events: monotonic clock/nanosleep, pthread absolute timeout/reacquire/notification, FIFO/nested dispatch and equal-deadline timers, early-wake, cancel races/cleanup, 4 producers/400 C++ RAII tasks, stop-during-callback and zero final heap/DSO residue. Host transport, NOT WTF RunLoop or browser.");
}
