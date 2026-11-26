#include "tmc_runloop.h"
#include "tmc_threads.h"
#include "tmc_heap.h"
#include "tmc_time.h"
#include <errno.h>
struct tmc_loop_task {struct tmc_loop_task *next;uint64_t deadline,token;void(*callback)(void*);void(*cleanup)(void*);void *argument;};
static uint64_t last_token;
static uint64_t token(void)
{
 uint64_t n=__atomic_load_n(&last_token,__ATOMIC_RELAXED);
 do{if(n==UINT64_MAX)return 0;}while(!__atomic_compare_exchange_n(&last_token,&n,n+1,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED));
 return n+1;
}
static int finish(int code,int saved){errno=saved;return code;}
static void lock(struct tmc_runloop*l){if(tmc_mutex_lock(&l->lock))__builtin_trap();}
static void unlock(struct tmc_runloop*l){if(tmc_mutex_unlock(&l->lock))__builtin_trap();}
static int valid(struct tmc_runloop*l){return l&&!((uintptr_t)l&7)&&tmc_thread_id();}
static void dispose(struct tmc_loop_task*t)
{void(*cleanup)(void*)=t->cleanup;void *arg=t->argument;tmc_heap_free(t);if(cleanup)cleanup(arg);}
int tmc_runloop_post(struct tmc_runloop*l,uint64_t deadline,void(*cb)(void*),void*arg,void(*cleanup)(void*),uint64_t*out)
{
 int saved=errno;if(!valid(l)||!cb||!out)return EINVAL;
 lock(l);int r=l->dead?EINVAL:l->stopped?ESHUTDOWN:0;unlock(l);if(r)return finish(r,saved);
 struct tmc_loop_task*t=tmc_heap_alloc(sizeof *t);if(!t)return finish(ENOMEM,saved);
 uint64_t id=token();if(!id){tmc_heap_free(t);return finish(EOVERFLOW,saved);}
 *t=(struct tmc_loop_task){0,deadline,id,cb,cleanup,arg};
 lock(l);r=l->dead?EINVAL:l->stopped?ESHUTDOWN:0;
 if(!r){
  struct tmc_loop_task **p=&l->head;while(*p&&(*p)->deadline<=deadline)p=&(*p)->next;
  t->next=*p;*p=t;*out=id;if(tmc_condition_signal(&l->changed))__builtin_trap();
 }
 unlock(l);if(r)tmc_heap_free(t);return finish(r,saved);
}
int tmc_runloop_cancel(struct tmc_runloop*l,uint64_t id)
{
 int saved=errno;if(!valid(l)||!id)return EINVAL;
 lock(l);if(l->dead){unlock(l);return finish(EINVAL,saved);}
 if(l->active==id){unlock(l);return finish(EBUSY,saved);}
 struct tmc_loop_task **p=&l->head;while(*p&&(*p)->token!=id)p=&(*p)->next;
 struct tmc_loop_task*t=*p;
 if(t){*p=t->next;if(tmc_condition_signal(&l->changed))__builtin_trap();}
 unlock(l);if(t)dispose(t);return finish(t?0:ESRCH,saved);
}
int tmc_runloop_run(struct tmc_runloop*l)
{
 int saved=errno;if(!valid(l))return EINVAL;
 lock(l);if(l->dead||l->running){int r=l->dead?EINVAL:EALREADY;unlock(l);return finish(r,saved);}
 l->running=1;l->owner=tmc_thread_id();int error=0;
 while(!l->stopped){
  struct tmc_loop_task*t=l->head;uint64_t now;
  error=tmc_time_now_ticks(&now);if(error)break;
  if(!t||t->deadline>now){
   error=tmc_condition_wait(&l->changed,&l->lock,t?t->deadline:0);
   if(error==ETIMEDOUT)error=0;
   if(error)break;
   continue;
  }
  l->head=t->next;l->active=t->token;unlock(l);
  t->callback(t->argument);dispose(t);
  lock(l);l->active=0;
 }
 l->owner=0;l->running=0;unlock(l);return finish(error,saved);
}
int tmc_runloop_stop(struct tmc_runloop*l)
{
 int saved=errno;if(!valid(l))return EINVAL;
 lock(l);if(l->dead){unlock(l);return finish(EINVAL,saved);}
 l->stopped=1;struct tmc_loop_task*t=l->head;l->head=0;
 if(tmc_condition_broadcast(&l->changed))__builtin_trap();
 unlock(l);
 while(t){struct tmc_loop_task*next=t->next;dispose(t);t=next;}
 return finish(0,saved);
}
int tmc_runloop_destroy(struct tmc_runloop*l)
{
 int saved=errno;if(!valid(l))return EINVAL;
 lock(l);int r=l->dead?EINVAL:!l->stopped||l->running?EBUSY:0;
 if(!r)l->dead=1;
 unlock(l);return finish(r,saved);
}
