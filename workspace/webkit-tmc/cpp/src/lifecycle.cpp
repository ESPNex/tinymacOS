/* SPDX-License-Identifier: MIT
 * Opt-in explicit .init_array runner and process/DSO Itanium destruction ABI.
 * No hosted C++ library, exceptions or hidden fixed registration capacity. */
#include "tmc_cpp_runtime.h"
#include "tmc_heap.h"
#include "tmc_sync.h"
#include "tmc_threads.h"
#include <errno.h>
#include <stdint.h>
namespace {
struct Record {void (*callback)(void *);void *argument,*dso;Record *next;};
Record *head;
unsigned long pending;
tmc_mutex mutex=TMC_MUTEX_INITIALIZER;
tmc_condition changed=TMC_CONDITION_INITIALIZER;
uint64_t finalizer_owner,initializer_owner;
unsigned finalizer_depth,initializer_state;
tmc_cpp_initializer const *initial_first,*initial_last;
void lock(){if(tmc_mutex_lock(&mutex))__builtin_trap();}
void unlock(){if(tmc_mutex_unlock(&mutex))__builtin_trap();}
uint64_t identity(){uint64_t id=tmc_thread_id();if(!id)__builtin_trap();return id;}
void wait(){if(tmc_condition_wait(&changed,&mutex,0))__builtin_trap();}
void notify(){if(tmc_condition_broadcast(&changed))__builtin_trap();}
}
extern "C" int __cxa_atexit(void (*callback)(void *),void *argument,void *dso)
{
 (void)identity();if(!callback){errno=EINVAL;return -1;}
 Record *record=static_cast<Record*>(tmc_heap_alloc(sizeof(Record)));
 if(!record)return -1; // allocation sets ENOMEM; registry stays unchanged
 lock();*record={callback,argument,dso,head};head=record;pending++;unlock();return 0;
}
extern "C" void __cxa_finalize(void *dso)
{
 uint64_t id=identity();int saved_errno=errno;lock();
 while(finalizer_owner&&finalizer_owner!=id)wait();
 if(finalizer_depth==~0u)__builtin_trap();
 finalizer_owner=id;finalizer_depth++;
 for(;;){
  Record **link=&head;while(*link&&dso&&(*link)->dso!=dso)link=&(*link)->next;
  if(!*link)break;
  Record *record=*link;*link=record->next;pending--;
  auto callback=record->callback;void *argument=record->argument;
  unlock();tmc_heap_free(record);callback(argument);lock();
 }
 if(!--finalizer_depth){finalizer_owner=0;notify();}
 unlock();errno=saved_errno;
}
extern "C" unsigned long tmc_cpp_pending_destructors(void)
{
 (void)identity();lock();unsigned long result=pending;unlock();return result;
}
extern "C" int tmc_cpp_run_initializers(tmc_cpp_initializer const *first,tmc_cpp_initializer const *last)
{
 uint64_t id=identity();
 if((!first)!=(!last)||reinterpret_cast<uintptr_t>(last)<reinterpret_cast<uintptr_t>(first)||
    reinterpret_cast<uintptr_t>(first)%alignof(tmc_cpp_initializer)||
    (reinterpret_cast<uintptr_t>(last)-reinterpret_cast<uintptr_t>(first))%sizeof(tmc_cpp_initializer))return EINVAL;
 lock();
 for(;;){
  if(initializer_state&&(initial_first!=first||initial_last!=last)){unlock();return EINVAL;}
  if(initializer_state==2){unlock();return 0;}
  if(!initializer_state)break;
  if(initializer_owner==id){unlock();return EDEADLK;}
  wait();
 }
 initializer_state=1;initializer_owner=id;initial_first=first;initial_last=last;unlock();
 // Linker-defined ranges contain function pointers, not ELF loader relocations.
 // These are a fixed-address static executable's already linked addresses.
 for(auto p=first;p!=last;++p)if(*p)(*p)();
 lock();initializer_state=2;initializer_owner=0;notify();unlock();return 0;
}
