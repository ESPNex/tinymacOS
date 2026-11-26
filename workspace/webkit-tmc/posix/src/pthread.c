/* SPDX-License-Identifier: MIT — explicit source-compatible pthread subset. */
#include "tmc_pthread.h"
#include "tmc_heap.h"
#include <errno.h>
struct control {struct tmc_thread native;int joining;struct control *next;};
static struct control *threads;
static struct tmc_mutex registry=TMC_MUTEX_INITIALIZER;
static void lock(void){if(tmc_mutex_lock(&registry))__builtin_trap();}
static void unlock(void){if(tmc_mutex_unlock(&registry))__builtin_trap();}
static int result(int code,int saved){errno=saved;return code;}
static int private(int p){return p==TMC_PTHREAD_PROCESS_PRIVATE?0:p==TMC_PTHREAD_PROCESS_SHARED?ENOTSUP:EINVAL;}
int tmc_pthread_attr_init(tmc_pthread_attr_t*a){if(!a)return EINVAL;*a=(tmc_pthread_attr_t){0};return 0;}
int tmc_pthread_attr_destroy(tmc_pthread_attr_t*a){return a?0:EINVAL;}
int tmc_pthread_attr_setdetachstate(tmc_pthread_attr_t*a,int state)
{if(!a)return EINVAL;if(state==TMC_PTHREAD_CREATE_DETACHED)return ENOTSUP;if(state)return EINVAL;a->detachstate=0;return 0;}
int tmc_pthread_create(tmc_pthread_t*out,const tmc_pthread_attr_t*a,void*(*entry)(void*),void*arg)
{
 int saved=errno;if(!out||!entry||!tmc_thread_id())return EINVAL;
 if(a&&a->detachstate)return a->detachstate==TMC_PTHREAD_CREATE_DETACHED?ENOTSUP:EINVAL;
 struct control *c=tmc_heap_calloc(1,sizeof *c);if(!c)return result(EAGAIN,saved);
 lock();c->next=threads;threads=c;
 int r=tmc_thread_create(&c->native,entry,arg);
 if(r)threads=c->next;else *out=c->native.identity;
 unlock();if(r)tmc_heap_free(c);return result(r,saved);
}
int tmc_pthread_join(tmc_pthread_t id,void **out)
{
 int saved=errno;if(!id)return ESRCH;if(id==tmc_thread_id())return EDEADLK;
 lock();struct control *c=threads;while(c&&c->native.identity!=id)c=c->next;
 if(!c){unlock();return result(ESRCH,saved);}
 if(c->joining){unlock();return result(EINVAL,saved);}c->joining=1;unlock();
 int r=tmc_thread_join(&c->native,out);
 lock();if(r)c->joining=0;else{struct control **p=&threads;while(*p!=c)p=&(*p)->next;*p=c->next;}unlock();
 if(!r)tmc_heap_free(c);
 return result(r,saved);
}
int tmc_pthread_detach(tmc_pthread_t id){(void)id;return ENOTSUP;}
tmc_pthread_t tmc_pthread_self(void){return tmc_thread_id();}
int tmc_pthread_equal(tmc_pthread_t a,tmc_pthread_t b){return a==b;}
void tmc_pthread_exit(void*p){tmc_thread_exit(p);}
int tmc_pthread_key_create(tmc_pthread_key_t*k,void(*d)(void*)){int e=errno;return result(tmc_key_create(k,d),e);}
int tmc_pthread_key_delete(tmc_pthread_key_t k){int e=errno;return result(tmc_key_delete(k),e);}
int tmc_pthread_setspecific(tmc_pthread_key_t k,const void*p){int e=errno;return result(tmc_setspecific(k,p),e);}
void *tmc_pthread_getspecific(tmc_pthread_key_t k){int e=errno;void*p=tmc_getspecific(k);errno=e;return p;}
int tmc_pthread_mutexattr_init(tmc_pthread_mutexattr_t*a){if(!a)return EINVAL;*a=(tmc_pthread_mutexattr_t){0};return 0;}
int tmc_pthread_mutexattr_destroy(tmc_pthread_mutexattr_t*a){return a?0:EINVAL;}
int tmc_pthread_mutexattr_settype(tmc_pthread_mutexattr_t*a,int type)
{if(!a||type<0||type>TMC_PTHREAD_MUTEX_NORMAL)return EINVAL;a->type=type;return 0;}
int tmc_pthread_mutexattr_setpshared(tmc_pthread_mutexattr_t*a,int p)
{if(!a)return EINVAL;int r=private(p);if(!r)a->pshared=p;return r;}
int tmc_pthread_mutex_init(tmc_pthread_mutex_t*m,const tmc_pthread_mutexattr_t*a)
{
 if(!m)return EINVAL;
 if(a&&(a->type<0||a->type>TMC_PTHREAD_MUTEX_NORMAL))return EINVAL;
 if(a){int r=private(a->pshared);if(r)return r;}
 *m=(tmc_pthread_mutex_t)TMC_PTHREAD_MUTEX_INITIALIZER;if(a)m->type=a->type;return 0;
}
int tmc_pthread_mutex_destroy(tmc_pthread_mutex_t*m)
{if(!m||m->dead)return EINVAL;if(__atomic_load_n(&m->owner,__ATOMIC_ACQUIRE)||__atomic_load_n(&m->gate.state,__ATOMIC_ACQUIRE))return EBUSY;m->dead=1;return 0;}
static int acquire(tmc_pthread_mutex_t*m,int attempt)
{
 int saved=errno;uint64_t id=tmc_thread_id();if(!m||m->dead||!id)return EINVAL;
 if(__atomic_load_n(&m->owner,__ATOMIC_ACQUIRE)==id){
  if(m->type==TMC_PTHREAD_MUTEX_RECURSIVE){if(m->depth==~0u)return EAGAIN;m->depth++;return 0;}
  if(attempt)return EBUSY;
  if(m->type==TMC_PTHREAD_MUTEX_ERRORCHECK)return EDEADLK;
 }
 int r=attempt?tmc_mutex_trylock(&m->gate):tmc_mutex_lock(&m->gate);
 if(!r){m->depth=1;__atomic_store_n(&m->owner,id,__ATOMIC_RELEASE);}return result(r,saved);
}
int tmc_pthread_mutex_lock(tmc_pthread_mutex_t*m){return acquire(m,0);}
int tmc_pthread_mutex_trylock(tmc_pthread_mutex_t*m){return acquire(m,1);}
int tmc_pthread_mutex_unlock(tmc_pthread_mutex_t*m)
{
 int saved=errno;if(!m||m->dead)return EINVAL;
 if(!tmc_thread_id()||__atomic_load_n(&m->owner,__ATOMIC_ACQUIRE)!=tmc_thread_id())return EPERM;
 if(--m->depth)return 0;
 __atomic_store_n(&m->owner,0,__ATOMIC_RELEASE);return result(tmc_mutex_unlock(&m->gate),saved);
}
int tmc_pthread_condattr_init(tmc_pthread_condattr_t*a){if(!a)return EINVAL;*a=(tmc_pthread_condattr_t){0};return 0;}
int tmc_pthread_condattr_destroy(tmc_pthread_condattr_t*a){return a?0:EINVAL;}
int tmc_pthread_condattr_setpshared(tmc_pthread_condattr_t*a,int p)
{if(!a)return EINVAL;int r=private(p);if(!r)a->pshared=p;return r;}
int tmc_pthread_cond_init(tmc_pthread_cond_t*c,const tmc_pthread_condattr_t*a)
{if(!c)return EINVAL;if(a){int r=private(a->pshared);if(r)return r;}*c=(tmc_pthread_cond_t)TMC_PTHREAD_COND_INITIALIZER;return 0;}
int tmc_pthread_cond_destroy(tmc_pthread_cond_t*c)
{if(!c||c->dead)return EINVAL;if(__atomic_load_n(&c->waiters,__ATOMIC_ACQUIRE))return EBUSY;c->dead=1;return 0;}
int tmc_pthread_cond_wait_until(tmc_pthread_cond_t*c,tmc_pthread_mutex_t*m,uint64_t deadline)
{
 int saved=errno;uint64_t id=tmc_thread_id();if(!c||c->dead||!m||m->dead||!id)return EINVAL;
 if(__atomic_load_n(&m->owner,__ATOMIC_ACQUIRE)!=id)return EPERM;
 if(m->depth!=1)return EINVAL;
 __atomic_fetch_add(&c->waiters,1,__ATOMIC_ACQ_REL);
 m->depth=0;__atomic_store_n(&m->owner,0,__ATOMIC_RELEASE);
 int r=tmc_condition_wait(&c->native,&m->gate,deadline);
 m->depth=1;__atomic_store_n(&m->owner,id,__ATOMIC_RELEASE);
 __atomic_fetch_sub(&c->waiters,1,__ATOMIC_RELEASE);return result(r,saved);
}
int tmc_pthread_cond_wait(tmc_pthread_cond_t*c,tmc_pthread_mutex_t*m){return tmc_pthread_cond_wait_until(c,m,0);}
int tmc_pthread_cond_signal(tmc_pthread_cond_t*c){int e=errno;if(!c||c->dead)return EINVAL;return result(tmc_condition_signal(&c->native),e);}
int tmc_pthread_cond_broadcast(tmc_pthread_cond_t*c){int e=errno;if(!c||c->dead)return EINVAL;return result(tmc_condition_broadcast(&c->native),e);}
int tmc_pthread_once(tmc_pthread_once_t*o,void(*callback)(void))
{
 int saved=errno;uint64_t id=tmc_thread_id();if(!o||!callback||!id)return EINVAL;
 int r=tmc_mutex_lock(&o->gate);if(r)return result(r,saved);
 while(o->state==1){
  if(o->owner==id){tmc_mutex_unlock(&o->gate);return result(EDEADLK,saved);}
  r=tmc_condition_wait(&o->changed,&o->gate,0);if(r){tmc_mutex_unlock(&o->gate);return result(r,saved);}
 }
 if(o->state==2){tmc_mutex_unlock(&o->gate);return result(0,saved);}
 o->state=1;o->owner=id;tmc_mutex_unlock(&o->gate);callback();
 if(tmc_mutex_lock(&o->gate))__builtin_trap();
 o->state=2;o->owner=0;
 if(tmc_condition_broadcast(&o->changed)||tmc_mutex_unlock(&o->gate))__builtin_trap();
 return result(0,saved);
}
