/* Same source consumer uses conventional pthread names on host and target.
 * Host transport is a SEPARATE TU using the system pthread library. */
#include <pthread.h>
#include <errno.h>
#include "tmc_heap.h"
#include "pthread_cases.h"
static unsigned failure,ready,go,completed,updates,once_calls,once_value;
static unsigned destructor_calls[4];
static pthread_once_t once=PTHREAD_ONCE_INIT;
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER,foreign=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t done=PTHREAD_COND_INITIALIZER;
static pthread_key_t key;
static pthread_t handles[4];
#define PC(x) do{if(!(x)){unsigned z=0;__atomic_compare_exchange_n(&failure,&z,__LINE__,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED);}}while(0)
static void initializer(void)
{
 once_calls++;PC(pthread_once(&once,initializer)==EDEADLK);pthread_test_pause();once_value=0xca11;
}
struct payload{unsigned worker,pass;};
static void destructor(void *p)
{
 struct payload *v=p;PC(!pthread_getspecific(key));destructor_calls[v->worker]++;
 if(!v->pass++){PC(!pthread_setspecific(key,v));}else tmc_heap_free(v);
}
static void *worker(void *p)
{
 unsigned w=(unsigned)(uintptr_t)p;__atomic_fetch_add(&ready,1,__ATOMIC_RELEASE);
 while(!__atomic_load_n(&go,__ATOMIC_ACQUIRE))pthread_test_pause();
 PC(pthread_equal(pthread_self(),handles[w]));PC(pthread_mutex_unlock(&foreign)==EPERM);
 PC(pthread_mutex_trylock(&foreign)==EBUSY);errno=91;PC(!pthread_once(&once,initializer)&&errno==91&&once_value==0xca11);
 struct payload *v=tmc_heap_calloc(1,sizeof *v);PC(v);if(v){v->worker=w;PC(!pthread_setspecific(key,v)&&pthread_getspecific(key)==v);}
 for(unsigned n=0;n<500;n++){
  size_t size=32+(n%97);unsigned char *b=tmc_heap_alloc(size);PC(b);if(b){
   for(size_t i=0;i<size;i++)b[i]=(unsigned char)(i+w+n);
   unsigned char *larger=tmc_heap_realloc(b,size+32);PC(larger);if(larger)b=larger;
   for(size_t i=0;i<size;i++)PC(b[i]==(unsigned char)(i+w+n));
   tmc_heap_free(b);
  }
  PC(!pthread_mutex_lock(&gate));updates++;PC(!pthread_mutex_unlock(&gate));
  pthread_cpp_touch();
  if(!w&&!(n%64))PC(!tmc_heap_trim());
  if(!(n%32))pthread_test_pause();
 }
 PC(!pthread_mutex_lock(&gate));completed++;PC(pthread_cond_destroy(&done)==EBUSY);
 if(completed==4)PC(!pthread_cond_broadcast(&done));
 PC(!pthread_mutex_unlock(&gate));
 if(w==2)pthread_exit(p);
 return p;
}
static unsigned park_release,join_ready,join_go,join_completed;
static pthread_t join_target;
static int join_codes[2];
static void *park(void *p){while(!__atomic_load_n(&park_release,__ATOMIC_ACQUIRE))pthread_test_pause();return p;}
static void *joiner(void *p)
{
 unsigned i=(unsigned)(uintptr_t)p;__atomic_fetch_add(&join_ready,1,__ATOMIC_RELEASE);
 while(!__atomic_load_n(&join_go,__ATOMIC_ACQUIRE))pthread_test_pause();
 void *result=0;errno=88;join_codes[i]=pthread_join(join_target,&result);PC(errno==88);
 if(!join_codes[i])PC(result==(void*)(uintptr_t)123);
 __atomic_fetch_add(&join_completed,1,__ATOMIC_RELEASE);return p;
}
unsigned pthread_cases_run(void)
{
 PC(pthread_self()&&pthread_join(pthread_self(),0)==EDEADLK&&pthread_join(0,0)==ESRCH);
 pthread_attr_t attr;PC(!pthread_attr_init(&attr));PC(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED)==ENOTSUP);PC(pthread_detach(pthread_self())==ENOTSUP);PC(!pthread_attr_destroy(&attr));
 pthread_mutexattr_t ma;PC(!pthread_mutexattr_init(&ma));PC(pthread_mutexattr_setpshared(&ma,PTHREAD_PROCESS_SHARED)==ENOTSUP);PC(pthread_mutexattr_settype(&ma,99)==EINVAL);PC(!pthread_mutexattr_settype(&ma,PTHREAD_MUTEX_RECURSIVE));
 pthread_mutex_t recursive;pthread_cond_t temporary=PTHREAD_COND_INITIALIZER;
 PC(!pthread_mutex_init(&recursive,&ma));PC(!pthread_mutex_lock(&recursive)&&!pthread_mutex_lock(&recursive));PC(pthread_cond_wait(&temporary,&recursive)==EINVAL);PC(pthread_mutex_destroy(&recursive)==EBUSY);
 PC(!pthread_mutex_unlock(&recursive)&&!pthread_mutex_unlock(&recursive)&&pthread_mutex_unlock(&recursive)==EPERM);PC(!pthread_mutex_destroy(&recursive));PC(pthread_mutex_lock(&recursive)==EINVAL);PC(!pthread_mutexattr_destroy(&ma));
 PC(!pthread_cond_signal(&temporary));PC(!pthread_cond_destroy(&temporary));PC(pthread_cond_signal(&temporary)==EINVAL);
 pthread_condattr_t ca;PC(!pthread_condattr_init(&ca));PC(pthread_condattr_setpshared(&ca,PTHREAD_PROCESS_SHARED)==ENOTSUP);PC(!pthread_condattr_destroy(&ca));
 PC(!pthread_mutex_lock(&gate));errno=74;PC(tmc_pthread_cond_wait_until(&done,&gate,pthread_test_ticks()+2)==ETIMEDOUT&&errno==74);PC(pthread_mutex_lock(&gate)==EDEADLK);PC(!pthread_mutex_unlock(&gate));
 PC(!tmc_heap_set_cache_limit(256u<<10));PC(!pthread_key_create(&key,destructor));PC(!pthread_mutex_lock(&foreign));
 for(unsigned w=0;w<4;w++){int r=pthread_create(handles+w,0,worker,(void*)(uintptr_t)w);if(r)__builtin_trap();}
 while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=4)pthread_test_pause();
 PC(!pthread_mutex_lock(&gate));__atomic_store_n(&go,1,__ATOMIC_RELEASE);
 while(completed!=4)PC(!pthread_cond_wait(&done,&gate));
 PC(!pthread_mutex_unlock(&gate));
 for(unsigned w=0;w<4;w++){void *r=0;PC(!pthread_join(handles[w],&r)&&r==(void*)(uintptr_t)w);PC(pthread_join(handles[w],0)==ESRCH&&destructor_calls[w]==2);}
 PC(updates==2000&&once_calls==1);PC(!pthread_mutex_unlock(&foreign));PC(!pthread_mutex_destroy(&foreign));PC(!pthread_cond_destroy(&done));PC(!pthread_mutex_destroy(&gate));
 pthread_key_t stale=key;PC(!pthread_key_delete(key));PC(pthread_setspecific(stale,(void*)1)==EINVAL);PC(!pthread_key_create(&key,0));PC(key!=stale&&!pthread_getspecific(key));PC(!pthread_key_delete(key));
 // Exactly one of two concurrent joiners may consume the same target.
 PC(!pthread_create(&join_target,0,park,(void*)(uintptr_t)123));pthread_t joiners[2];
 for(unsigned i=0;i<2;i++)PC(!pthread_create(joiners+i,0,joiner,(void*)(uintptr_t)i));
 while(__atomic_load_n(&join_ready,__ATOMIC_ACQUIRE)!=2)pthread_test_pause();
 __atomic_store_n(&join_go,1,__ATOMIC_RELEASE);
 while(!__atomic_load_n(&join_completed,__ATOMIC_ACQUIRE))pthread_test_pause();
 __atomic_store_n(&park_release,1,__ATOMIC_RELEASE);
 for(unsigned i=0;i<2;i++)PC(!pthread_join(joiners[i],0));
 PC((!join_codes[0]&&join_codes[1]==EINVAL)||(!join_codes[1]&&join_codes[0]==EINVAL));PC(pthread_join(join_target,0)==ESRCH);
 pthread_t previous=join_target;
 for(unsigned n=0;n<16;n++){pthread_t t=0;PC(!pthread_create(&t,0,park,(void*)(uintptr_t)n));PC(t>previous);PC(!pthread_join(t,0));PC(pthread_join(previous,0)==ESRCH);previous=t;}
 // Exhaust the actual native/host-transport thread capacity, then recover.
 __atomic_store_n(&park_release,0,__ATOMIC_RELEASE);pthread_t held[64];unsigned count=0;int error=0;
 for(;count<64;count++){pthread_t out=UINT64_MAX;errno=83;error=pthread_create(&out,0,park,0);PC(errno==83);if(error){PC(error==EAGAIN&&out==UINT64_MAX);break;}held[count]=out;}
 PC(count>0&&count<64&&error==EAGAIN);__atomic_store_n(&park_release,1,__ATOMIC_RELEASE);
 for(unsigned n=0;n<count;n++)PC(!pthread_join(held[n],0));
 pthread_t recovered=0;PC(!pthread_create(&recovered,0,park,0)&&!pthread_join(recovered,0));
 PC(!pthread_cpp_finish());PC(!tmc_heap_set_cache_limit(0)&&!tmc_heap_trim());
 struct tmc_heap_stats st=tmc_heap_statistics();PC(!st.live&&!st.spans&&!st.reserved);
 return failure;
}
