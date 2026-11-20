#include "tmc_threads.h"
#include "tmc_sync.h"
#include "tmc_backend.h"
#include "abi.h"
#include "threadfinishabi.h"
#include <errno.h>
#define RUNTIME_COOKIE 0x544d4352u
#define LIVE 1u
#define JOINED 3u
#define KEY_BITS 5u
#define KEY_MASK (TMC_TSD_KEYS-1u)
struct key_entry { uint64_t generation; void (*destructor)(void *); int used; };
static struct key_entry keys[TMC_TSD_KEYS];
static struct tmc_mutex keys_lock=TMC_MUTEX_INITIALIZER;
static uint64_t last_identity;
static int aligned(const void *p){return p && !((uintptr_t)p&7);}
static uint64_t identity(void)
{
    uint64_t old=__atomic_load_n(&last_identity,__ATOMIC_RELAXED);
    do {if(old==UINT64_MAX)return 0;}
    while(!__atomic_compare_exchange_n(&last_identity,&old,old+1,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED));
    return old+1;
}
static int claim(struct tmc_thread *t)
{
    if(!aligned(t))return EINVAL;
    uint32_t old=__atomic_load_n(&t->state,__ATOMIC_ACQUIRE);
    for(;;){
        if(old!=0 && old!=JOINED)return EBUSY;
        if(__atomic_compare_exchange_n(&t->state,&old,LIVE,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return 0;
    }
}
static void prepare(struct tmc_thread *t)
{
    t->identity=identity();t->result=0;t->join_claim=0;t->exiting=0;
    t->alive=1;
    for(unsigned i=0;i<TMC_TSD_KEYS;i++)t->specific[i]=(struct tmc_specific_slot){0,0};
}
static int attach(struct tmc_thread *t)
{
    if(tmc_tls_current() || tmc_tls_install(&t->tls))return EBUSY;
    t->tls.reserved=RUNTIME_COOKIE;
    return 0;
}
struct tmc_thread *tmc_thread_current(void)
{
    struct tmc_tls_block *t=tmc_tls_current();
    return t && t->reserved==RUNTIME_COOKIE?(struct tmc_thread *)t:0;
}
uint64_t tmc_thread_id(void)
{
    struct tmc_thread *t=tmc_thread_current();return t?t->identity:0;
}
int tmc_runtime_enter(struct tmc_thread *t)
{
    if(tmc_tls_current())return EBUSY;
    int r=claim(t);if(r)return r;
    prepare(t);t->initial_thread=1;t->entry=0;t->argument=0;
    r=t->identity?attach(t):EAGAIN;
    if(r){t->alive=0;__atomic_store_n(&t->state,0,__ATOMIC_RELEASE);}
    return r;
}
static int valid_key(tmc_key_t key,unsigned *index)
{
    unsigned i=(unsigned)(key&KEY_MASK);uint64_t g=key>>KEY_BITS;
    if(!g || !keys[i].used || keys[i].generation!=g)return 0;
    *index=i;return 1;
}
int tmc_key_create(tmc_key_t *out,void (*destructor)(void *))
{
    if(!out)return EINVAL;
    int r=tmc_mutex_lock(&keys_lock);if(r)return r;
    r=EAGAIN;
    for(unsigned i=0;i<TMC_TSD_KEYS;i++)if(!keys[i].used && keys[i].generation<(UINT64_MAX>>KEY_BITS)){
        keys[i].generation++;keys[i].destructor=destructor;keys[i].used=1;
        *out=(keys[i].generation<<KEY_BITS)|i;r=0;break;
    }
    tmc_mutex_unlock(&keys_lock);return r;
}
int tmc_key_delete(tmc_key_t key)
{
    int r=tmc_mutex_lock(&keys_lock);if(r)return r;
    unsigned i;if(valid_key(key,&i)){keys[i].used=0;keys[i].destructor=0;r=0;}else r=EINVAL;
    tmc_mutex_unlock(&keys_lock);return r;
}
int tmc_setspecific(tmc_key_t key,const void *value)
{
    struct tmc_thread *t=tmc_thread_current();if(!t)return EINVAL;
    int r=tmc_mutex_lock(&keys_lock);if(r)return r;
    unsigned i;
    if(valid_key(key,&i)){t->specific[i]=(struct tmc_specific_slot){key>>KEY_BITS,(void *)value};r=0;}
    else r=EINVAL;
    tmc_mutex_unlock(&keys_lock);return r;
}
void *tmc_getspecific(tmc_key_t key)
{
    struct tmc_thread *t=tmc_thread_current();if(!t)return 0;
    if(tmc_mutex_lock(&keys_lock))return 0;
    unsigned i;void *result=0;
    if(valid_key(key,&i) && t->specific[i].generation==(key>>KEY_BITS))result=t->specific[i].value;
    tmc_mutex_unlock(&keys_lock);return result;
}
static void cleanup(struct tmc_thread *t)
{
    t->exiting=1;
    for(unsigned pass=0;pass<TMC_TSD_DESTRUCTOR_ITERATIONS;pass++){
        unsigned called=0;
        for(unsigned i=0;i<TMC_TSD_KEYS;i++){
            void *value=0;void (*destructor)(void *)=0;
            if(tmc_mutex_lock(&keys_lock))__builtin_trap();
            if(keys[i].used && t->specific[i].generation==keys[i].generation && t->specific[i].value && keys[i].destructor){
                value=t->specific[i].value;destructor=keys[i].destructor;
                t->specific[i].value=0; /* POSIX-style clear BEFORE callback */
            }
            tmc_mutex_unlock(&keys_lock);
            /* No registry lock across user callback: reentrant get/set,
             * deletion/new-key allocation and repopulation are permitted. */
            if(destructor){called++;destructor(value);}
        }
        if(!called)break;
    }
    for(unsigned i=0;i<TMC_TSD_KEYS;i++)t->specific[i]=(struct tmc_specific_slot){0,0};
}
int tmc_runtime_leave(void)
{
    struct tmc_thread *t=tmc_thread_current();
    if(!t || !t->initial_thread)return EINVAL;
    if(t->exiting)return EDEADLK;
    cleanup(t);
    int r=(int)tmc_tls_uninstall();if(r)return EINVAL;
    t->alive=0;__atomic_store_n(&t->state,JOINED,__ATOMIC_RELEASE);return 0;
}
static __attribute__((noreturn)) void fatal(void)
{
    /* Broken startup/exit is not successful thread completion. Terminate the
     * process, never publish a bogus successful join. */
    tmc_syscall(S_EXIT,0,0,0,0);__builtin_trap();
}
void tmc_thread_exit(void *result)
{
    struct tmc_thread *t=tmc_thread_current();
    if(!t || t->initial_thread || t->exiting)fatal();
    cleanup(t);t->result=result;
    /* Kernel clears FS and completion only once user TCB/stack use is over.
     * Nothing dereferences t after this publication; FINISH never returns. */
    tmc_syscall(S_THREADFINISH,TMC_THREAD_FINISH,(uintptr_t)&t->alive,0,0);
    fatal();
}
static void trampoline(uint64_t argument)
{
    struct tmc_thread *t=(void *)(uintptr_t)argument;
    if(attach(t))fatal();
    void *result=t->entry(t->argument);
    tmc_thread_exit(result);
}
int tmc_thread_create(struct tmc_thread *t,void *(*entry)(void *),void *argument)
{
    if(!entry)return EINVAL;
    if(tmc_syscall(S_THREADFINISH,TMC_THREAD_FINISH_QUERY,0,0,0)!=TMC_THREAD_FINISH_VERSION || tmc_word_query()!=1)return ENOSYS;
    int r=claim(t);if(r)return r;
    prepare(t);t->initial_thread=0;t->entry=entry;t->argument=argument;
    if(!t->identity || tmc_syscall(S_THREAD,(uintptr_t)trampoline,(uintptr_t)t,0,0)<0){
        t->alive=0;__atomic_store_n(&t->state,0,__ATOMIC_RELEASE);return EAGAIN;
    }
    return 0;
}
int tmc_thread_join_until(struct tmc_thread *t,void **result,uint64_t deadline)
{
    if(!aligned(t))return EINVAL;
    if(t==tmc_thread_current())return EDEADLK;
    if(__atomic_load_n(&t->state,__ATOMIC_ACQUIRE)!=LIVE || t->initial_thread)return EINVAL;
    uint32_t zero=0;
    if(!__atomic_compare_exchange_n(&t->join_claim,&zero,1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return EBUSY;
    while(__atomic_load_n(&t->alive,__ATOMIC_ACQUIRE)){
        int r=tmc_word_wait(&t->alive,1,deadline);
        if(r && r!=EAGAIN){__atomic_store_n(&t->join_claim,0,__ATOMIC_RELEASE);return r;}
    }
    if(result)*result=t->result;
    /* Final access to t. Reuse cannot race a trailing store by this joiner. */
    __atomic_store_n(&t->state,JOINED,__ATOMIC_RELEASE);return 0;
}
int tmc_thread_join(struct tmc_thread *t,void **result){return tmc_thread_join_until(t,result,0);}
