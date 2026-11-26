/* SPDX-License-Identifier: MIT
 * x86-64 Itanium local-static guard ABI, opt-in -fno-exceptions runtime.
 * Compiler checks the FIRST BYTE of its 64-bit guard. Only release publishes
 * that byte; abort leaves it zero. Each active initializer has a heap record,
 * so nesting has no fixed-capacity table or pointer-hash aliasing limit.
 * Bootstrap requires attached native TCB; no compiler thread_local, unwind,
 * process teardown, C++ global constructor runner or __cxa_atexit is provided. */
#include <stdint.h>
#include "tmc_heap.h"
#include "tmc_sync.h"
#include "tmc_threads.h"
namespace {
struct Active {uint64_t *guard,owner;Active *next;};
Active *active;
tmc_mutex mutex=TMC_MUTEX_INITIALIZER;
tmc_condition changed=TMC_CONDITION_INITIALIZER;
void lock(){if(tmc_mutex_lock(&mutex))__builtin_trap();}
void unlock(){if(tmc_mutex_unlock(&mutex))__builtin_trap();}
uint64_t owner(){uint64_t id=tmc_thread_id();if(!id)__builtin_trap();return id;}
bool ready(uint64_t*g){return __atomic_load_n(reinterpret_cast<unsigned char*>(g),__ATOMIC_ACQUIRE)!=0;}
void finish(uint64_t *g,bool initialized)
{
 uint64_t id=owner();lock();Active **entry=&active;
 while(*entry&&(*entry)->guard!=g)entry=&(*entry)->next;
 if(!*entry||(*entry)->owner!=id)__builtin_trap();
 Active *record=*entry;*entry=record->next;
 if(initialized)__atomic_store_n(reinterpret_cast<unsigned char*>(g),1,__ATOMIC_RELEASE);
 if(tmc_condition_broadcast(&changed))__builtin_trap();
 unlock();tmc_heap_free(record);
}
}
extern "C" int __cxa_guard_acquire(uint64_t *g)
{
 if(!g||(reinterpret_cast<uintptr_t>(g)&7))__builtin_trap();
 if(ready(g))return 0;
 uint64_t id=owner();lock();
 for(;;){
  if(ready(g)){unlock();return 0;}
  Active *record=active;while(record&&record->guard!=g)record=record->next;
  if(!record){
   record=static_cast<Active*>(tmc_heap_alloc(sizeof(Active)));
   if(!record)__builtin_trap();
   *record={g,id,active};active=record;unlock();return 1;
  }
  if(record->owner==id)__builtin_trap(); // recursive initialization is not a deadlock
  if(tmc_condition_wait(&changed,&mutex,0))__builtin_trap();
 }
}
extern "C" void __cxa_guard_release(uint64_t*g){finish(g,true);}
extern "C" void __cxa_guard_abort(uint64_t*g){finish(g,false);}
