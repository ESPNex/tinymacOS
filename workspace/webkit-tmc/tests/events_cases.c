#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "tmc_heap.h"
#include "events_cases.h"
static unsigned failure,barrier_entered,barrier_release,done,cleanups,sequence;
static unsigned race_callbacks,race_cleanups,signal_ready,signal_release;
static struct tmc_runloop loop=TMC_RUNLOOP_INITIALIZER;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition;
#define EC(x) do{if(!(x)){unsigned z=0;__atomic_compare_exchange_n(&failure,&z,__LINE__,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED);}}while(0)
static uint64_t now(void){uint64_t n=0;EC(!tmc_time_now_ticks(&n));return n;}
static struct timespec absolute(uint64_t tick){return (struct timespec){(int64_t)(tick/100),(long)(tick%100)*10000000};}
static void wait_count(unsigned*p,unsigned n){while(__atomic_load_n(p,__ATOMIC_ACQUIRE)!=n)events_pause();}
struct job {unsigned number;uint64_t deadline;};
static struct job jobs[68];
static void cleanup(void*p){(void)p;__atomic_fetch_add(&cleanups,1,__ATOMIC_RELEASE);errno=65;}
static void task(void*p)
{
 struct job*j=p;EC(now()>=j->deadline);EC(sequence++==j->number);
 if(!j->number){uint64_t id=0;EC(!tmc_runloop_post(&loop,0,task,jobs+64,cleanup,&id));}
 __atomic_fetch_add(&done,1,__ATOMIC_RELEASE);errno=64;
}
static void forbidden(void*p){(void)p;EC(0);}
static void barrier(void*p)
{
 unsigned phase=(unsigned)(uintptr_t)p;__atomic_store_n(&barrier_entered,phase,__ATOMIC_RELEASE);
 wait_count(&barrier_release,phase);
}
static void *dispatcher(void*p){errno=81;EC(!tmc_runloop_run(p)&&errno==81);return p;}
static void race_cleanup(void*p){(void)p;__atomic_fetch_add(&race_cleanups,1,__ATOMIC_RELEASE);}
static void race_task(void*p){(void)p;__atomic_fetch_add(&race_callbacks,1,__ATOMIC_RELAXED);}
static void *producer(void*p)
{
 for(unsigned n=0;n<100;n++){
  uint64_t id=0;errno=82;EC(!tmc_runloop_post(&loop,0,race_task,0,race_cleanup,&id)&&errno==82);
  int r=tmc_runloop_cancel(&loop,id);EC(r==0||r==EBUSY||r==ESRCH);EC(errno==82);
  EC(!events_cpp_post(&loop));if(!(n%16))events_pause();
 }
 return p;
}
static void *signaler(void*p)
{
 EC(!pthread_mutex_lock(&mutex));signal_ready=1;EC(!pthread_cond_broadcast(&condition));
 while(!signal_release)EC(!pthread_cond_wait(&condition,&mutex));
 EC(!pthread_mutex_unlock(&mutex));return p;
}
unsigned events_cases_run(void)
{
 struct timespec t,res;errno=71;EC(!clock_getres(CLOCK_MONOTONIC,&res)&&res.tv_sec==0&&res.tv_nsec==10000000&&errno==71);
 EC(!clock_gettime(CLOCK_MONOTONIC,&t));uint64_t before=now();
 struct timespec tiny={0,1};EC(!nanosleep(&tiny,0)&&errno==71&&now()>=before+2);
 struct timespec past={0,0};EC(!clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&past,0));
 EC(clock_gettime(CLOCK_REALTIME,&t)==-1&&errno==ENOTSUP);
 pthread_condattr_t attr;EC(!pthread_condattr_init(&attr));int selected=-1;EC(!pthread_condattr_getclock(&attr,&selected)&&selected==CLOCK_REALTIME);
 EC(pthread_condattr_setclock(&attr,77)==EINVAL);EC(!pthread_condattr_setclock(&attr,CLOCK_MONOTONIC));
 EC(!pthread_condattr_getclock(&attr,&selected)&&selected==CLOCK_MONOTONIC);EC(!pthread_cond_init(&condition,&attr));EC(!pthread_condattr_destroy(&attr));
 EC(!pthread_mutex_lock(&mutex));before=now();t=absolute(before+2);errno=72;
 EC(pthread_cond_timedwait(&condition,&mutex,&t)==ETIMEDOUT&&errno==72&&now()>=before+2);
 EC(pthread_mutex_trylock(&mutex)==EBUSY);EC(pthread_mutex_lock(&mutex)==EDEADLK);
 t=(struct timespec){0,1000000000L};EC(pthread_cond_timedwait(&condition,&mutex,&t)==EINVAL);EC(pthread_mutex_lock(&mutex)==EDEADLK);
 t=absolute(now()+1);pthread_cond_t realtime=PTHREAD_COND_INITIALIZER;EC(pthread_cond_timedwait(&realtime,&mutex,&t)==ENOTSUP);EC(!pthread_cond_destroy(&realtime));
 EC(pthread_cond_timedwait(&condition,&mutex,&past)==ETIMEDOUT);
 EC(!pthread_mutex_unlock(&mutex));
 // Real notification before an absolute deadline; always recheck predicate.
 pthread_t notifier;EC(!pthread_create(&notifier,0,signaler,0));EC(!pthread_mutex_lock(&mutex));
 t=absolute(now()+100);while(!signal_ready)EC(!pthread_cond_timedwait(&condition,&mutex,&t));signal_release=1;
 EC(!pthread_cond_signal(&condition)&&!pthread_mutex_unlock(&mutex)&&!pthread_join(notifier,0));
 EC(!pthread_cond_destroy(&condition)&&!pthread_mutex_destroy(&mutex));
 EC(!tmc_heap_set_cache_limit(256u<<10));EC(tmc_runloop_destroy(&loop)==EBUSY);
 uint64_t canceled=0,active=0;EC(!tmc_runloop_post(&loop,UINT64_MAX,forbidden,0,cleanup,&canceled));EC(!tmc_runloop_cancel(&loop,canceled)&&tmc_runloop_cancel(&loop,canceled)==ESRCH);
 EC(!tmc_runloop_post(&loop,0,barrier,(void*)(uintptr_t)1,cleanup,&active));
 for(unsigned n=0;n<68;n++)jobs[n]=(struct job){n,0};
 for(unsigned n=0;n<64;n++){uint64_t id;EC(!tmc_runloop_post(&loop,0,task,jobs+n,cleanup,&id));}
 uint64_t future=now()+30;
 for(unsigned n=65;n<68;n++){uint64_t id;jobs[n].deadline=future;EC(!tmc_runloop_post(&loop,future,task,jobs+n,cleanup,&id));}
 pthread_t runner;EC(!pthread_create(&runner,0,dispatcher,&loop));wait_count(&barrier_entered,1);
 EC(tmc_runloop_run(&loop)==EALREADY&&tmc_runloop_destroy(&loop)==EBUSY&&tmc_runloop_cancel(&loop,active)==EBUSY);
 __atomic_store_n(&barrier_release,1,__ATOMIC_RELEASE);wait_count(&done,68);wait_count(&cleanups,70);EC(sequence==68);
 // Four producers race cancellation against dispatch; C++ RAII tasks are not canceled.
 pthread_t producers[4];for(unsigned i=0;i<4;i++)EC(!pthread_create(producers+i,0,producer,(void*)(uintptr_t)i));
 for(unsigned i=0;i<4;i++)EC(!pthread_join(producers[i],0));
 wait_count(&race_cleanups,400);while(events_cpp_completed()!=400)events_pause();EC(race_callbacks<=400);
 // Interrupt an idle far-future wait with an immediate event, then stop while
 // callback is active: active finishes, pending cleanup runs, late post fails.
 uint64_t pending=0;EC(!tmc_runloop_post(&loop,now()+10000,forbidden,0,cleanup,&pending));
 tiny=(struct timespec){0,20000000};EC(!nanosleep(&tiny,0));
 EC(!tmc_runloop_post(&loop,0,barrier,(void*)(uintptr_t)2,cleanup,&active));wait_count(&barrier_entered,2);
 EC(tmc_runloop_cancel(&loop,active)==EBUSY);errno=73;EC(!tmc_runloop_stop(&loop)&&errno==73);EC(!tmc_runloop_stop(&loop));
 uint64_t unchanged=UINT64_MAX;EC(tmc_runloop_post(&loop,0,forbidden,0,cleanup,&unchanged)==ESHUTDOWN&&unchanged==UINT64_MAX);EC(tmc_runloop_cancel(&loop,pending)==ESRCH);
 __atomic_store_n(&barrier_release,2,__ATOMIC_RELEASE);EC(!pthread_join(runner,0));EC(cleanups==72);EC(!tmc_runloop_destroy(&loop)&&tmc_runloop_stop(&loop)==EINVAL);
 struct tmc_runloop empty=TMC_RUNLOOP_INITIALIZER;EC(!tmc_runloop_stop(&empty)&&!tmc_runloop_run(&empty)&&!tmc_runloop_destroy(&empty));
 EC(!events_cpp_finish());EC(!tmc_heap_set_cache_limit(0)&&!tmc_heap_trim());struct tmc_heap_stats st=tmc_heap_statistics();EC(!st.live&&!st.spans&&!st.reserved);
 return failure;
}
