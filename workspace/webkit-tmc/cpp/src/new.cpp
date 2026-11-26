/* SPDX-License-Identifier: MIT
 * Opt-in no-exception allocation ABI over the actual native process heap.
 * Target consumers must not link a competing C++ standard-library allocator.
 * Host UBSan diagnostics may load libstdc++ transitively; tests verify that
 * strong executable definitions still provide this allocation/guard ABI. */
#include <new>
#include <stdint.h>
#include "tmc_heap.h"
namespace std {
const nothrow_t nothrow{};
static new_handler handler;
new_handler get_new_handler() noexcept{return __atomic_load_n(&handler,__ATOMIC_ACQUIRE);}
new_handler set_new_handler(new_handler f) noexcept{return __atomic_exchange_n(&handler,f,__ATOMIC_ACQ_REL);}
}
static void *attempt(size_t n,size_t alignment)
{
 if(!alignment)return tmc_heap_alloc(n);
 if(alignment<sizeof(void*)||(alignment&(alignment-1))||alignment>2097152)return nullptr;
 if(!n)n=1;
 if(n>SIZE_MAX-(alignment-1))return nullptr;
 return tmc_heap_aligned_alloc(alignment,(n+alignment-1)&~(alignment-1));
}
static void *allocate(size_t n,size_t a,bool nothrow)
{
 for(;;){
  if(void*p=attempt(n,a))return p;
  auto f=std::get_new_handler();
  if(f){f();continue;}
  if(nothrow)return nullptr;
  // Returning null from ordinary new would violate the compiler's contract.
  // Exception-enabled allocation is deliberately not implemented.
  __builtin_trap();
 }
}
void *operator new(size_t n){return allocate(n,0,false);}
void *operator new[](size_t n){return allocate(n,0,false);}
void *operator new(size_t n,const std::nothrow_t&) noexcept{return allocate(n,0,true);}
void *operator new[](size_t n,const std::nothrow_t&) noexcept{return allocate(n,0,true);}
void *operator new(size_t n,std::align_val_t a){return allocate(n,(size_t)a,false);}
void *operator new[](size_t n,std::align_val_t a){return allocate(n,(size_t)a,false);}
void *operator new(size_t n,std::align_val_t a,const std::nothrow_t&) noexcept{return allocate(n,(size_t)a,true);}
void *operator new[](size_t n,std::align_val_t a,const std::nothrow_t&) noexcept{return allocate(n,(size_t)a,true);}
void operator delete(void*p) noexcept{tmc_heap_free(p);}
void operator delete[](void*p) noexcept{tmc_heap_free(p);}
void operator delete(void*p,size_t) noexcept{tmc_heap_free(p);}
void operator delete[](void*p,size_t) noexcept{tmc_heap_free(p);}
void operator delete(void*p,const std::nothrow_t&) noexcept{tmc_heap_free(p);}
void operator delete[](void*p,const std::nothrow_t&) noexcept{tmc_heap_free(p);}
void operator delete(void*p,std::align_val_t) noexcept{tmc_heap_free(p);}
void operator delete[](void*p,std::align_val_t) noexcept{tmc_heap_free(p);}
void operator delete(void*p,size_t,std::align_val_t) noexcept{tmc_heap_free(p);}
void operator delete[](void*p,size_t,std::align_val_t) noexcept{tmc_heap_free(p);}
void operator delete(void*p,std::align_val_t,const std::nothrow_t&) noexcept{tmc_heap_free(p);}
void operator delete[](void*p,std::align_val_t,const std::nothrow_t&) noexcept{tmc_heap_free(p);}
