/* Hosted Linux futex transport tests the actual target lock algorithms.
 * This is NOT a test of the native scheduler: see guest_sync_probe.c. */
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "tmc_sync.h"
#include "abi.h"
#include "wordwaitabi.h"
static unsigned parks,wakes;
static uint64_t now(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*100+t.tv_nsec/10000000;}
long tmc_syscall(long n,uint64_t op,uint64_t address,uint64_t value,uint64_t deadline)
{
 assert(n==S_WORDWAIT);
 if(op==TMC_WORD_QUERY)return TMC_WORD_ABI_VERSION;
 if(!address || (address&3) || value>UINT32_MAX)return TMC_WORD_INVALID;
 uint32_t *word=(void *)(uintptr_t)address;
 if(op==TMC_WORD_WAKE){__atomic_add_fetch(&wakes,1,__ATOMIC_RELAXED);return syscall(SYS_futex,word,FUTEX_WAKE_PRIVATE,value>INT_MAX?INT_MAX:(int)value,0,0,0);}
 if(op!=TMC_WORD_WAIT)return TMC_WORD_INVALID;
 if(__atomic_load_n(word,__ATOMIC_ACQUIRE)!=(uint32_t)value)return TMC_WORD_CHANGED;
 uint64_t t=now();if(deadline && deadline<=t)return TMC_WORD_TIMEOUT;
 struct timespec ts={0};if(deadline){uint64_t ticks=deadline-t;ts.tv_sec=ticks/100;ts.tv_nsec=(ticks%100)*10000000;}
 __atomic_add_fetch(&parks,1,__ATOMIC_RELAXED);
 long r=syscall(SYS_futex,word,FUTEX_WAIT_PRIVATE,(uint32_t)value,deadline?&ts:0,0,0);
 if(!r)return 0;
 if(errno==EAGAIN)return TMC_WORD_CHANGED;
 if(errno==ETIMEDOUT)return TMC_WORD_TIMEOUT;
 if(errno==EINTR)return 0; /* permitted spurious return */
 return TMC_WORD_INVALID;
}
static struct tmc_mutex mutex=TMC_MUTEX_INITIALIZER;
static struct tmc_condition condition=TMC_CONDITION_INITIALIZER;
static unsigned ready,go,done,counter,inside;
static void *worker(void *unused)
{
 (void)unused;
 assert(!tmc_mutex_lock(&mutex));ready++;assert(!tmc_condition_broadcast(&condition));
 while(!go)assert(!tmc_condition_wait(&condition,&mutex,0));
 assert(!tmc_mutex_unlock(&mutex));
 for(unsigned j=0;j<2000;j++){
  assert(!tmc_mutex_lock(&mutex));assert(!inside);inside=1;
  unsigned old=counter;sched_yield();counter=old+1;inside=0;
  assert(!tmc_mutex_unlock(&mutex));
 }
 assert(!tmc_mutex_lock(&mutex));done++;assert(!tmc_condition_broadcast(&condition));assert(!tmc_mutex_unlock(&mutex));return 0;
}
int main(void)
{
 assert(tmc_word_query()==1);uint32_t word=3;
 assert(tmc_word_wait(&word,4,0)==EAGAIN);assert(tmc_word_wake(&word,0)==0);
 assert(tmc_word_wait((void *)1,0,0)==EINVAL);assert(tmc_word_wake(0,1)==-EINVAL);
 assert(tmc_word_wait(&word,3,now()+2)==ETIMEDOUT);
 assert(!tmc_mutex_lock(&mutex));assert(tmc_mutex_trylock(&mutex)==EBUSY);
 assert(tmc_mutex_lock_until(&mutex,now()+2)==ETIMEDOUT);
 assert(tmc_condition_wait(&condition,&mutex,now()+2)==ETIMEDOUT);
 assert(tmc_mutex_trylock(&mutex)==EBUSY);assert(!tmc_mutex_unlock(&mutex));assert(tmc_mutex_unlock(&mutex)==EPERM);
 pthread_t threads[4];for(unsigned j=0;j<4;j++)assert(!pthread_create(threads+j,0,worker,0));
 assert(!tmc_mutex_lock(&mutex));
 while(ready!=4)assert(!tmc_condition_wait(&condition,&mutex,now()+500));
 go=1;assert(!tmc_condition_broadcast(&condition));
 while(done!=4)assert(!tmc_condition_wait(&condition,&mutex,now()+500));
 assert(counter==8000 && !inside);assert(!tmc_mutex_unlock(&mutex));
 for(unsigned j=0;j<4;j++)assert(!pthread_join(threads[j],0));
 assert(parks>3 && wakes>0);printf("PASS real sync.c via host futex: 8000 serialized updates, broadcast, condition/mutex timeout and mutex reacquisition; parks=%u wakes=%u\n",parks,wakes);
}
