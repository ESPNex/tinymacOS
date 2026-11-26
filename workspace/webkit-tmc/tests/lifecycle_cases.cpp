/* Compiler-generated global ctor/destructor and local-static registrations.
 * This TU's .init_array is run explicitly, not by the hosted startup code. */
#include <new>
#include <stdint.h>
#include <errno.h>
#include "tmc_threads.h"
#include "tmc_heap.h"
#include "lifecycle_cases.h"
namespace {
unsigned failure,constructed,destroyed,local_constructed,local_destroyed,ready;
unsigned worker_counts[4],next_expected[4]={99,99,99,99},post_registration;
unsigned global_destroy_order[3],global_destroy_count;
unsigned dso_tokens[4],selective_tokens[2],selective_order[6],selective_count;
uint64_t callback_owner;
unsigned callback_depth;
tmc_cpp_initializer const *range_first,*range_last;
struct Argument {unsigned worker,index;};
Argument arguments[4][101];
#define LC(x) do{if(!(x)){unsigned zero=0;__atomic_compare_exchange_n(&failure,&zero,__LINE__,false,__ATOMIC_RELAXED,__ATOMIC_RELAXED);}}while(0)
struct Global {
 unsigned *value,id;
 explicit Global(unsigned n):value(new unsigned(n*101)),id(n){
  LC(++constructed==n);
  LC(tmc_thread_id()!=0);
  LC(tmc_cpp_run_initializers(range_first,range_last)==EDEADLK);
  lifecycle_pause();
 }
 ~Global(){
  LC(value&&*value==id*101);global_destroy_order[global_destroy_count++]=id;destroyed++;delete value;
 }
};
Global first(1),second(2);
struct Local {
 unsigned value;
 Local():value(303){local_constructed++;lifecycle_pause();}
 ~Local(){LC(value==303);local_destroyed++;global_destroy_order[global_destroy_count++]=3;}
};
Local& local(){static Local value;return value;}
void selected(void *p){selective_order[selective_count++]=static_cast<unsigned>(reinterpret_cast<uintptr_t>(p));}
void worker_callback(void *p)
{
 auto *a=static_cast<Argument*>(p);uint64_t id=tmc_thread_id();LC(id);
 if(!callback_depth)callback_owner=id;
 LC(callback_owner==id);callback_depth++;
 unsigned w=a->worker,n=a->index;worker_counts[w]++;
 if(n==100){LC(w==0&&post_registration==0);post_registration++;}
 else{
  LC(n==next_expected[w]);next_expected[w]--;
  if(w==0&&n==99)LC(!__cxa_atexit(worker_callback,&arguments[0][100],&dso_tokens[0]));
  if(w==1&&n==50)__cxa_finalize(&dso_tokens[1]); // same-thread, same-DSO recursion
 }
 lifecycle_pause();LC(callback_owner==id);callback_depth--;if(!callback_depth)callback_owner=0;
}
}
extern "C" unsigned lifecycle_setup(tmc_cpp_initializer const *first_ptr,tmc_cpp_initializer const *last_ptr)
{
 LC(!constructed&&!local_constructed&&!tmc_cpp_pending_destructors());
 range_first=first_ptr;range_last=last_ptr;
 LC(tmc_cpp_run_initializers(first_ptr,nullptr)==EINVAL);
 errno=0;LC(__cxa_atexit(nullptr,nullptr,nullptr)==-1&&errno==EINVAL);
 // Interleaved DSO identities: selective LIFO must skip unmatched records.
 for(unsigned i=0;i<6;i++)LC(!__cxa_atexit(selected,reinterpret_cast<void*>(uintptr_t(i)),&selective_tokens[i%2]));
 errno=73;__cxa_finalize(&selective_tokens[0]);LC(errno==73&&selective_count==3);
 LC(selective_order[0]==4&&selective_order[1]==2&&selective_order[2]==0);
 __cxa_finalize(&selective_tokens[0]);LC(selective_count==3);
 __cxa_finalize(&selective_tokens[1]);LC(selective_count==6);
 LC(selective_order[3]==5&&selective_order[4]==3&&selective_order[5]==1);
 for(unsigned w=0;w<4;w++)for(unsigned n=0;n<=100;n++)arguments[w][n]={w,n};
 return failure;
}
extern "C" void *lifecycle_worker(void *p)
{
 unsigned w=static_cast<unsigned>(reinterpret_cast<uintptr_t>(p));LC(w<4);
 __atomic_fetch_add(&ready,1,__ATOMIC_RELEASE);
 while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=4)lifecycle_pause();
 LC(!tmc_cpp_run_initializers(range_first,range_last));
 LC(constructed==2&&first.value&&*first.value==101&&second.value&&*second.value==202);
 LC(local().value==303);
 for(unsigned n=0;n<100;n++)LC(!__cxa_atexit(worker_callback,&arguments[w][n],&dso_tokens[w]));
 return p;
}
extern "C" void *lifecycle_finalizer(void *p)
{
 unsigned w=static_cast<unsigned>(reinterpret_cast<uintptr_t>(p));LC(w<4);
 errno=91;__cxa_finalize(&dso_tokens[w]);LC(errno==91);return p;
}
extern "C" unsigned lifecycle_finish(void)
{
 LC(ready==4&&constructed==2&&local_constructed==1&&!destroyed&&!local_destroyed);
 for(unsigned w=0;w<4;w++)LC(worker_counts[w]==100u+(w==0)&&next_expected[w]==~0u);
 LC(post_registration==1&&!callback_depth&&!callback_owner);
 LC(tmc_cpp_pending_destructors()==3); // two compiler globals + local static
 LC(!tmc_cpp_run_initializers(range_first,range_last));LC(constructed==2);
 tmc_cpp_initializer other[1]={};LC(tmc_cpp_run_initializers(other,other+1)==EINVAL);
 __cxa_finalize(nullptr);
 LC(destroyed==2&&local_destroyed==1&&global_destroy_count==3);
 LC(global_destroy_order[0]==3&&global_destroy_order[1]==2&&global_destroy_order[2]==1);
 LC(!tmc_cpp_pending_destructors());__cxa_finalize(nullptr);LC(global_destroy_count==3);
 auto st=tmc_heap_statistics();LC(!st.live&&!st.spans&&!st.reserved);
 return failure;
}
