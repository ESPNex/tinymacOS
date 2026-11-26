/* Same C++ consumer compiled freestanding for host and actual native guest. */
#include <new>
#include <stdint.h>
#include "tmc_heap.h"
#include "cpp_cases.h"
extern "C" int __cxa_guard_acquire(uint64_t*);
extern "C" void __cxa_guard_release(uint64_t*);
extern "C" void __cxa_guard_abort(uint64_t*);
static unsigned failure,constructed,nested_constructed,destructed,started,go,retry_attempts,handler_calls;
static unsigned stages[4];
static uint64_t retry_guard;
static unsigned retry_value;
static void *handler_victim;
#define CC(x) do{if(!(x)){unsigned expected=0;__atomic_compare_exchange_n(&failure,&expected,__LINE__,false,__ATOMIC_RELAXED,__ATOMIC_RELAXED);}}while(0)
struct Nested {unsigned value;Nested():value(17){__atomic_fetch_add(&nested_constructed,1,__ATOMIC_RELAXED);cpp_test_pause();}};
static Nested& nested(){static Nested n;return n;}
struct Once {unsigned value;Once():value(0){__atomic_fetch_add(&constructed,1,__ATOMIC_RELAXED);cpp_test_pause();value=0xc000u+nested().value;}};
static Once& once(){static Once o;return o;}
struct alignas(4096) Aligned {unsigned char bytes[4096];};
struct Item {unsigned value=0xabc;~Item(){__atomic_fetch_add(&destructed,1,__ATOMIC_RELAXED);}};
static void rescue(){__atomic_fetch_add(&handler_calls,1,__ATOMIC_RELAXED);void *p=handler_victim;handler_victim=nullptr;CC(p);tmc_heap_free(p);std::set_new_handler(nullptr);}
extern "C" unsigned cpp_test_basics(void)
{
 cpp_test_progress("CPP phase=allocation-basics");
 CC(std::get_new_handler()==nullptr);
 void *zero=::operator new(0);CC(zero&&!(reinterpret_cast<uintptr_t>(zero)&15));::operator delete(zero);
 alignas(unsigned) unsigned char storage[sizeof(unsigned)];unsigned *p=new(storage) unsigned(42);CC(p==reinterpret_cast<unsigned*>(storage)&&*p==42);
 volatile size_t huge=SIZE_MAX;CC(!::operator new(huge,std::nothrow));CC(!::operator new[](huge,std::nothrow));
 CC(!::operator new(32,std::align_val_t(3),std::nothrow));
 size_t aligns[]={16,64,4096,2097152};
 for(size_t a:aligns){void*q=::operator new(13,std::align_val_t(a));CC(q&&!(reinterpret_cast<uintptr_t>(q)&(a-1)));::operator delete(q,size_t(13),std::align_val_t(a));}
 void*q=::operator new[](37,std::align_val_t(64),std::nothrow);CC(q);::operator delete[](q,std::align_val_t(64),std::nothrow);
 q=::operator new(19,std::nothrow);CC(q);::operator delete(q,std::nothrow);
 q=::operator new[](19,std::nothrow);CC(q);::operator delete[](q,std::nothrow);
 ::operator delete(nullptr);::operator delete[](nullptr,size_t(0));
 uint64_t local_guard=0;CC(__cxa_guard_acquire(&local_guard)==1);__cxa_guard_abort(&local_guard);CC(local_guard==0);CC(__cxa_guard_acquire(&local_guard)==1);__cxa_guard_release(&local_guard);CC(__cxa_guard_acquire(&local_guard)==0);
 cpp_test_progress("CPP phase=handler-reserve");
 // Exhaust native S_VM reservations, then let a real new_handler free one.
 void*chunks[32]={};unsigned count=0;
 while(count<32&&(chunks[count]=tmc_heap_alloc(1u<<20)))count++;
 cpp_test_progress("CPP phase=handler-recover");
 CC(count==32);handler_victim=count?chunks[0]:nullptr;
 if(handler_victim){
  CC(std::set_new_handler(rescue)==nullptr);q=::operator new(2u<<20);CC(q&&handler_calls==1&&handler_victim==nullptr);::operator delete(q);chunks[0]=nullptr;
 }
 cpp_test_progress("CPP phase=handler-cleanup");
 for(unsigned i=0;i<count;i++)tmc_heap_free(chunks[i]);
 auto st=tmc_heap_statistics();CC(!st.spans&&!st.live);return failure;
}
extern "C" void*cpp_test_handoff(unsigned seed)
{
 unsigned*p=new unsigned[128];for(unsigned i=0;i<128;i++)p[i]=seed^i;return p;
}
extern "C" void cpp_test_start(void){__atomic_store_n(&go,1,__ATOMIC_RELEASE);}
extern "C" void*cpp_test_worker(void*arg)
{
 auto*j=static_cast<cpp_job*>(arg);__atomic_fetch_add(&started,1,__ATOMIC_RELEASE);
 while(!__atomic_load_n(&go,__ATOMIC_ACQUIRE))cpp_test_pause();
 unsigned *p=static_cast<unsigned*>(j->handoff);for(unsigned i=0;i<128;i++)CC(p[i]==(j->seed^i));delete[]p;j->handoff=nullptr;
 __atomic_store_n(&stages[j->seed-78],1,__ATOMIC_RELEASE);
 // Every contender must observe a fully published object, including nesting.
 CC(once().value==0xc011u);
 __atomic_store_n(&stages[j->seed-78],2,__ATOMIC_RELEASE);
 for(;;){
  if(!__cxa_guard_acquire(&retry_guard)){CC(retry_value==0xbeef);break;}
  unsigned attempt=__atomic_fetch_add(&retry_attempts,1,__ATOMIC_RELAXED);cpp_test_pause();
  if(!attempt){__cxa_guard_abort(&retry_guard);continue;}
  retry_value=0xbeef;__cxa_guard_release(&retry_guard);break;
 }
 __atomic_store_n(&stages[j->seed-78],3,__ATOMIC_RELEASE);
 for(unsigned round=0;round<j->rounds;round++){
  Item*a=new Item[3];for(unsigned i=0;i<3;i++)CC(a[i].value==0xabc);delete[]a;
  if(!(round%32)){Aligned*a=new(std::nothrow) Aligned{};CC(a);if(a){CC(!(reinterpret_cast<uintptr_t>(a)&4095));for(unsigned i=0;i<4096;i++)CC(!a->bytes[i]);delete a;}cpp_test_pause();}
 }
 __atomic_store_n(&stages[j->seed-78],4,__ATOMIC_RELEASE);
 return arg;
}
extern "C" unsigned cpp_test_snapshot(void){unsigned v=0;for(unsigned i=0;i<4;i++)v|=__atomic_load_n(stages+i,__ATOMIC_ACQUIRE)<<(i*4);return v;}
extern "C" unsigned cpp_test_finish(void)
{
 CC(started==4&&constructed==1&&nested_constructed==1&&retry_attempts==2&&destructed==12000&&handler_calls==1);
 CC(!tmc_heap_set_cache_limit(0)&&!tmc_heap_trim());
 auto st=tmc_heap_statistics();CC(!st.spans&&!st.reserved&&!st.live&&!st.requested);return failure;
}
extern "C" void cpp_test_oom_trap(void){volatile size_t huge=SIZE_MAX;void*p=::operator new(huge);::operator delete(p);}
struct Recursive {Recursive(){cpp_test_recursion_trap();}};
extern "C" void cpp_test_recursion_trap(void){static Recursive r;(void)r;}
