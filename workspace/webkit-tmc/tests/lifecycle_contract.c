#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include "tmc_threads.h"
#include "tmc_heap.h"
#include "lifecycle_cases.h"
extern tmc_cpp_initializer __start_tmc_test_init[],__stop_tmc_test_init[];
void lifecycle_vm_fail_reserve(void);
void lifecycle_pause(void){struct timespec t={0,1000000};nanosleep(&t,0);}
static void never(void *p){(void)p;assert(0);}
int main(void)
{
 struct tmc_thread initial={0},workers[4]={0};assert(!tmc_runtime_enter(&initial));
 lifecycle_vm_fail_reserve();errno=0;
 assert(__cxa_atexit(never,0,0)==-1&&errno==ENOMEM&&!tmc_cpp_pending_destructors());
 assert(!lifecycle_setup(__start_tmc_test_init,__stop_tmc_test_init));
 for(unsigned phase=0;phase<2;phase++){
  for(unsigned i=0;i<4;i++)assert(!tmc_thread_create(workers+i,phase?lifecycle_finalizer:lifecycle_worker,(void*)(uintptr_t)i));
  for(unsigned i=0;i<4;i++){void *r=0;assert(!tmc_thread_join(workers+i,&r)&&r==(void*)(uintptr_t)i);}
 }
 unsigned failure=lifecycle_finish();if(failure)fprintf(stderr,"LIFECYCLE first-failure-line=%u\n",failure);assert(!failure);
 assert(!tmc_runtime_leave());
 puts("PASS lifecycle: real compiler global and local-static constructors/destructors; once-only concurrent explicit init_array, recursive startup rejection, 400 concurrent registrations, 401 worker callbacks, DSO-selective LIFO, repeated/nested finalization, registration during destructor, allocation failure with unchanged registry, zero final heap spans. Host symbols isolated from hosted CRT; not full libc++/WebKit.");
}
