/* Shared assertions for hosted transport and the actual native guest. */
#include "tmc_threads.h"
#include "tmc_sync.h"
#include "tmc_backend.h"
#include "abi.h"
#include <errno.h>
static uint32_t first_failure;
#define CHECK(x) do {if(!(x)){uint32_t z=0;__atomic_compare_exchange_n(&first_failure,&z,__LINE__,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED);}} while(0)
static uint64_t tick(void){return tmc_syscall(S_TICKS,0,0,0,0);}
static void nap(void){tmc_syscall(S_SLEEP,1,0,0,0);}
static struct tmc_thread initial,workers[4],joiner,reused;
static struct tmc_mutex test_lock=TMC_MUTEX_INITIALIZER;
static struct tmc_condition test_condition=TMC_CONDITION_INITIALIZER;
static unsigned ready,start,counter,created,resource_capacity;
static struct tmc_thread resource_threads[32];
static struct tmc_mutex resource_lock=TMC_MUTEX_INITIALIZER;
static struct tmc_condition resource_condition=TMC_CONDITION_INITIALIZER;
static unsigned resource_release;
static tmc_key_t own_key,repeat_key,always_key,reentrant_key;
struct worker_data {uintptr_t id;unsigned repeats,always,reentrant;uint64_t identity;};
static struct worker_data data[4],initial_data,reuse_data;
static void repeat_destructor(void *p)
{
 struct worker_data *d=p;CHECK(tmc_getspecific(repeat_key)==0);CHECK(tmc_getspecific(own_key)==d);
 CHECK(tmc_thread_id()==d->identity);d->repeats++;
 if(d->repeats<2)CHECK(!tmc_setspecific(repeat_key,d));
}
static void always_destructor(void *p)
{
 struct worker_data *d=p;CHECK(!tmc_getspecific(always_key));d->always++;
 CHECK(!tmc_setspecific(always_key,d));
}
static void reentrant_destructor(void *p)
{
 struct worker_data *d=p;tmc_key_t temporary=0;d->reentrant++;
 CHECK(!tmc_key_create(&temporary,0));CHECK(!tmc_setspecific(temporary,d));
 CHECK(tmc_getspecific(temporary)==d);CHECK(!tmc_key_delete(temporary));
}
static void test_sample_cpu(void);
static void *worker(void *p)
{
 struct worker_data *d=p;d->identity=tmc_thread_id();CHECK(d->identity!=0);
 CHECK(tmc_thread_current()!=0);CHECK(tmc_thread_join(tmc_thread_current(),0)==EDEADLK);
 int *error=tmc_tls_errno_location();CHECK(error!=0);if(!error)return 0;
#ifdef TMC_NATIVE_PROBE
 CHECK(&errno==error);
#endif
 *error=(int)(d->id*101);
 CHECK(!tmc_getspecific(own_key));CHECK(!tmc_setspecific(own_key,d));
 CHECK(!tmc_setspecific(repeat_key,d));CHECK(!tmc_setspecific(always_key,d));CHECK(!tmc_setspecific(reentrant_key,d));
 CHECK(!tmc_mutex_lock(&test_lock));ready++;CHECK(!tmc_condition_broadcast(&test_condition));
 while(!start){int r=tmc_condition_wait(&test_condition,&test_lock,tick()+2000);CHECK(!r);if(r)break;}
 CHECK(!tmc_mutex_unlock(&test_lock));
 for(unsigned j=0;j<80;j++){
  nap();CHECK(!tmc_mutex_lock(&test_lock));test_sample_cpu();unsigned old=counter;tmc_syscall(S_SLEEP,0,0,0,0);counter=old+1;
  CHECK(tmc_getspecific(own_key)==d);CHECK(*error==(int)(d->id*101));CHECK(!tmc_mutex_unlock(&test_lock));
 }
 if(d->id==4)tmc_thread_exit((void *)(d->id+100));
 return (void *)(d->id+100);
}
static void *join_worker(void *unused)
{
 (void)unused;void *result=0;CHECK(!tmc_thread_join(&workers[0],&result));CHECK(result==(void *)101);return result;
}
static void *fast_worker(void *argument)
{
 struct worker_data *d=argument;CHECK(tmc_thread_current()!=0 && tmc_thread_id()!=0);
 CHECK(tmc_getspecific(own_key)==0 && tmc_getspecific(reentrant_key)==0);
 CHECK(!tmc_setspecific(reentrant_key,d));return argument;
}
static void *value_worker(void *argument){return argument;}
static void *exit_null_worker(void *argument){(void)argument;tmc_thread_exit(0);}
static void *resource_worker(void *argument)
{
 (void)argument;CHECK(tmc_thread_current()!=0);CHECK(!tmc_mutex_lock(&resource_lock));
 while(!resource_release){int r=tmc_condition_wait(&resource_condition,&resource_lock,tick()+3000);CHECK(!r);if(r)break;}
 CHECK(!tmc_mutex_unlock(&resource_lock));return (void *)99;
}
static int create(struct tmc_thread *t,void *(*entry)(void *),void *argument)
{
 int r=tmc_thread_create(t,entry,argument);CHECK(!r);if(!r)created++;return r;
}
extern int tmc_cpp_thread_smoke(void);
static int test_vm_lifetime(void); /* native wrapper supplies real map/unmap */
static int run_thread_cases(void)
{
 CHECK(!tmc_thread_current() && !tmc_thread_id());
 CHECK(!tmc_runtime_enter(&initial));CHECK(tmc_runtime_enter(&initial)==EBUSY);
 initial_data.identity=tmc_thread_id();CHECK(initial_data.identity!=0);
 *tmc_tls_errno_location()=777;
 CHECK(!tmc_key_create(&own_key,0));CHECK(!tmc_key_create(&repeat_key,repeat_destructor));
 CHECK(!tmc_key_create(&always_key,always_destructor));CHECK(!tmc_key_create(&reentrant_key,reentrant_destructor));
 CHECK(!tmc_setspecific(own_key,&initial_data));
 tmc_key_t spare[TMC_TSD_KEYS];unsigned count=0;
 while(count<TMC_TSD_KEYS-4){CHECK(!tmc_key_create(&spare[count],0));count++;}
 tmc_key_t extra;CHECK(tmc_key_create(&extra,0)==EAGAIN);
 for(unsigned j=0;j<count;j++)CHECK(!tmc_key_delete(spare[j]));
 tmc_key_t stale,fresh;CHECK(!tmc_key_create(&stale,0));CHECK(!tmc_setspecific(stale,&initial_data));
 CHECK(!tmc_key_delete(stale));CHECK(!tmc_key_create(&fresh,0));CHECK(fresh!=stale);
 CHECK(!tmc_getspecific(fresh) && !tmc_getspecific(stale));CHECK(tmc_setspecific(stale,&initial_data)==EINVAL);
 CHECK(tmc_key_delete(stale)==EINVAL);CHECK(!tmc_key_delete(fresh));
 CHECK(tmc_thread_create(0,worker,0)==EINVAL);CHECK(tmc_thread_create(&reused,0,0)==EINVAL);
 for(unsigned j=0;j<4;j++){data[j].id=j+1;create(&workers[j],worker,&data[j]);}
 CHECK(!tmc_mutex_lock(&test_lock));
 while(ready!=4){int r=tmc_condition_wait(&test_condition,&test_lock,tick()+2000);CHECK(!r);if(r)break;}
 void *timeout_result=(void *)0x987;
 CHECK(tmc_thread_join_until(&workers[0],&timeout_result,tick()+3)==ETIMEDOUT);
 CHECK(timeout_result==(void *)0x987);
 create(&joiner,join_worker,0);
 uint64_t deadline=tick()+1000;
 while(!__atomic_load_n(&workers[0].join_claim,__ATOMIC_ACQUIRE) && tick()<deadline)nap();
 CHECK(__atomic_load_n(&workers[0].join_claim,__ATOMIC_ACQUIRE)==1);
 CHECK(tmc_thread_join_until(&workers[0],0,tick()+3)==EBUSY);
 CHECK(tmc_thread_create(&workers[0],worker,0)==EBUSY);
 start=1;CHECK(!tmc_condition_broadcast(&test_condition));CHECK(!tmc_mutex_unlock(&test_lock));
 for(unsigned j=1;j<4;j++){void *result=0;CHECK(!tmc_thread_join_until(&workers[j],&result,tick()+3000));CHECK(result==(void *)(uintptr_t)(j+101));}
 void *result=0;CHECK(!tmc_thread_join_until(&joiner,&result,tick()+3000));CHECK(result==(void *)101);
 CHECK(tmc_thread_join(&workers[0],0)==EINVAL);
 CHECK(counter==320);
 for(unsigned j=0;j<4;j++){
  CHECK(data[j].repeats==2 && data[j].always==TMC_TSD_DESTRUCTOR_ITERATIONS && data[j].reentrant==1);
  CHECK(data[j].identity!=initial_data.identity);
  for(unsigned k=0;k<j;k++)CHECK(data[j].identity!=data[k].identity);
 }
 CHECK(tmc_getspecific(own_key)==&initial_data && *tmc_tls_errno_location()==777);
 /* Exhaust real task slots with live blocked children, require a real error
  * and a reset failed object, then reclaim everything and continue creating. */
 for(unsigned j=0;j<32;j++){
  int r=tmc_thread_create(&resource_threads[j],resource_worker,0);
  if(r){CHECK(r==EAGAIN);CHECK(!resource_threads[j].alive && !resource_threads[j].state);break;}
  resource_capacity++;created++;
 }
 CHECK(resource_capacity>0 && resource_capacity<32);
 CHECK(!tmc_mutex_lock(&resource_lock));resource_release=1;
 CHECK(!tmc_condition_broadcast(&resource_condition));CHECK(!tmc_mutex_unlock(&resource_lock));
 for(unsigned j=0;j<resource_capacity;j++){
  result=0;CHECK(!tmc_thread_join_until(&resource_threads[j],&result,tick()+3000));CHECK(result==(void *)99);
 }
 uint64_t previous=0;
 for(unsigned j=0;j<64;j++){
  reuse_data.reentrant=0;create(&reused,fast_worker,&reuse_data);
  CHECK(reused.identity>previous);previous=reused.identity;
  result=0;CHECK(!tmc_thread_join_until(&reused,&result,tick()+1000));
  CHECK(result==&reuse_data && reuse_data.reentrant==1 && !__atomic_load_n(&reused.alive,__ATOMIC_ACQUIRE));
 }
 void *values[]={0,(void *)(uintptr_t)UINTPTR_MAX,(void *)7};
 for(unsigned j=0;j<3;j++){
  create(&reused,value_worker,values[j]);result=(void *)0x987;
  CHECK(!tmc_thread_join_until(&reused,&result,tick()+1000));CHECK(result==values[j]);
 }
 create(&reused,exit_null_worker,0);result=(void *)0x987;
 CHECK(!tmc_thread_join_until(&reused,&result,tick()+1000));CHECK(!result);
 CHECK(!tmc_cpp_thread_smoke());created++;
 int vm=test_vm_lifetime();CHECK(vm>=0);if(vm>0)created+=(unsigned)vm;
 CHECK(!tmc_setspecific(repeat_key,&initial_data));CHECK(!tmc_runtime_leave());
 CHECK(initial_data.repeats==2 && !tmc_thread_current() && !tmc_tls_current());
 CHECK(tmc_setspecific(own_key,&initial_data)==EINVAL);
 CHECK(!tmc_key_delete(own_key));CHECK(!tmc_key_delete(repeat_key));CHECK(!tmc_key_delete(always_key));CHECK(!tmc_key_delete(reentrant_key));
 CHECK(tmc_runtime_leave()==EINVAL);
 uint64_t previous_main=initial_data.identity;
 CHECK(!tmc_runtime_enter(&initial));CHECK(tmc_thread_id()>previous_main);
 CHECK(!tmc_getspecific(own_key));CHECK(!tmc_runtime_leave());
 return (int)first_failure;
}
