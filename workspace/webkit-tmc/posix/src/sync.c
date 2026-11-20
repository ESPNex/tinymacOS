#include "tmc_sync.h"
#include "tmc_backend.h"
#include "wordwaitabi.h"
#include "abi.h"
#include <errno.h>
static int aligned(const void *p){return p && !((uintptr_t)p&3);}
int tmc_word_query(void)
{
    long r=tmc_syscall(S_WORDWAIT,TMC_WORD_QUERY,0,0,0);
    return r==TMC_WORD_ABI_VERSION?(int)r:-1;
}
int tmc_word_wait(uint32_t *word,uint32_t expected,uint64_t deadline)
{
    if(!aligned(word))return EINVAL;
    long r=tmc_syscall(S_WORDWAIT,TMC_WORD_WAIT,(uintptr_t)word,expected,deadline);
    if(r==0)return 0;
    if(r==TMC_WORD_CHANGED)return EAGAIN;
    if(r==TMC_WORD_TIMEOUT)return ETIMEDOUT;
    return EINVAL;
}
int tmc_word_wake(uint32_t *word,uint32_t count)
{
    if(!aligned(word))return -EINVAL;
    long r=tmc_syscall(S_WORDWAIT,TMC_WORD_WAKE,(uintptr_t)word,count,0);
    return r<0?-EINVAL:(int)r;
}
int tmc_mutex_trylock(struct tmc_mutex *m)
{
    if(!aligned(m))return EINVAL;
    uint32_t expected=0;
    return __atomic_compare_exchange_n(&m->state,&expected,1,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)?0:EBUSY;
}
int tmc_mutex_lock_until(struct tmc_mutex *m,uint64_t deadline)
{
    int r=tmc_mutex_trylock(m);
    if(r!=EBUSY)return r;
    /* 0 unlocked, 1 locked/uncontended, 2 locked/possibly waiters.
     * The exchange and compare-and-park avoid the unlock-before-sleep race.
     * Keep 2 after taking a contended mutex so unlock wakes the next waiter. */
    for(;;){
        if(__atomic_exchange_n(&m->state,2,__ATOMIC_ACQUIRE)==0)return 0;
        r=tmc_word_wait(&m->state,2,deadline);
        if(r && r!=EAGAIN)return r;
    }
}
int tmc_mutex_lock(struct tmc_mutex *m){return tmc_mutex_lock_until(m,0);}
int tmc_mutex_unlock(struct tmc_mutex *m)
{
    if(!aligned(m))return EINVAL;
    uint32_t previous=__atomic_exchange_n(&m->state,0,__ATOMIC_RELEASE);
    if(previous>1 && tmc_word_wake(&m->state,1)<0)return EINVAL;
    return previous?0:EPERM;
}
int tmc_condition_wait(struct tmc_condition *c,struct tmc_mutex *m,uint64_t deadline)
{
    if(!aligned(c)||!aligned(m))return EINVAL;
    uint32_t seq=__atomic_load_n(&c->sequence,__ATOMIC_ACQUIRE);
    int r=tmc_mutex_unlock(m);if(r)return r;
    r=tmc_word_wait(&c->sequence,seq,deadline);
    if(r==EAGAIN)r=0; /* signal happened after unlock but before park */
    int lock=tmc_mutex_lock(m);
    return lock?lock:r;
}
static int notify(struct tmc_condition *c,uint32_t count)
{
    if(!aligned(c))return EINVAL;
    __atomic_fetch_add(&c->sequence,1,__ATOMIC_RELEASE);
    return tmc_word_wake(&c->sequence,count)<0?EINVAL:0;
}
int tmc_condition_signal(struct tmc_condition *c){return notify(c,1);}
int tmc_condition_broadcast(struct tmc_condition *c){return notify(c,UINT32_MAX);}
