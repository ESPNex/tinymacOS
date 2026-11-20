/* Hosted transport ONLY. Native guest tests use actual S_THREAD/FINISH/FS.
 * Host pthreads are detached implementation scaffolding, not target pthread. */
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "abi.h"
#include "wordwaitabi.h"
#include "threadfinishabi.h"
#include "tmc_tls.h"
static _Thread_local struct tmc_tls_block *host_tcb;
long tmc_tls_install(struct tmc_tls_block *p)
{
 if(!p || ((uintptr_t)p&7))return -1;
 p->self=(uintptr_t)p;p->error_number=0;p->reserved=0;p->cookie=1;host_tcb=p;return 0;
}
long tmc_tls_uninstall(void){host_tcb=0;return 0;}
struct tmc_tls_block *tmc_tls_current(void){return host_tcb;}
int *tmc_tls_errno_location(void){return host_tcb?&host_tcb->error_number:0;}
static uint64_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*100+t.tv_nsec/10000000;}
struct start {void (*entry)(uint64_t);uint64_t argument;};
static void *host_start(void *p){struct start s=*(struct start *)p;free(p);s.entry(s.argument);abort();}
long tmc_syscall(long n,uint64_t a,uint64_t b,uint64_t c,uint64_t d)
{
 if(n==S_TICKS)return now();
 if(n==S_SLEEP){struct timespec t={(time_t)(a/100),(long)(a%100)*10000000};nanosleep(&t,0);return 0;}
 if(n==S_EXIT)abort();
 if(n==S_THREAD){
  struct start *s=malloc(sizeof *s);if(!s)return -1;s->entry=(void *)(uintptr_t)a;s->argument=b;
  pthread_attr_t attr;pthread_t thread;pthread_attr_init(&attr);pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  int error=pthread_create(&thread,&attr,host_start,s);pthread_attr_destroy(&attr);if(error){free(s);return -1;}return 1;
 }
 if(n==S_THREADFINISH){
  if(a==TMC_THREAD_FINISH_QUERY)return TMC_THREAD_FINISH_VERSION;
  if(a!=TMC_THREAD_FINISH || !b || (b&3) || c || d)return -1;
  host_tcb=0;__atomic_store_n((uint32_t *)(uintptr_t)b,0,__ATOMIC_RELEASE);
  syscall(SYS_futex,(void *)(uintptr_t)b,FUTEX_WAKE_PRIVATE,INT_MAX,0,0,0);
  pthread_exit(0);
 }
 if(n!=S_WORDWAIT)return -1;
 if(a==TMC_WORD_QUERY)return TMC_WORD_ABI_VERSION;
 if(!b || (b&3) || c>UINT32_MAX)return TMC_WORD_INVALID;
 uint32_t *word=(void *)(uintptr_t)b;
 if(a==TMC_WORD_WAKE)return syscall(SYS_futex,word,FUTEX_WAKE_PRIVATE,c>INT_MAX?INT_MAX:(int)c,0,0,0);
 if(a!=TMC_WORD_WAIT)return TMC_WORD_INVALID;
 if(__atomic_load_n(word,__ATOMIC_ACQUIRE)!=(uint32_t)c)return TMC_WORD_CHANGED;
 uint64_t t=now();if(d && d<=t)return TMC_WORD_TIMEOUT;
 struct timespec ts={0};if(d){uint64_t ticks=d-t;ts.tv_sec=ticks/100;ts.tv_nsec=(ticks%100)*10000000;}
 long r=syscall(SYS_futex,word,FUTEX_WAIT_PRIVATE,(uint32_t)c,d?&ts:0,0,0);
 if(!r)return 0;
 if(errno==EAGAIN)return TMC_WORD_CHANGED;
 if(errno==ETIMEDOUT)return TMC_WORD_TIMEOUT;
 if(errno==EINTR)return 0;
 return TMC_WORD_INVALID;
}
