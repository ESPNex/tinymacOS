#ifndef TMC_PTHREAD_H
#define TMC_PTHREAD_H
#include "tmc_threads.h"
#include "tmc_sync.h"
#include "tmc_time.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Source-level opt-in pthread subset, NOT a foreign libpthread binary ABI.
 * Initial thread must attach a tmc runtime first. Thread handles are native
 * generation identities, never raw control-block pointers. Joined handles
 * become invalid; one joiner only. Only joinable threads/private primitives.
 * No detach/cancel/cleanup stack, signals, atfork, scheduling, stack attrs,
 * realtime timedwait or compiler TLS. Child pthread_exit is supported; initial
 * thread pthread_exit is NOT. Once callbacks must return, not exit/throw.
 * Object init/destroy/lifetime require normal external POSIX ordering; no
 * concurrent destroy/use. No copying live objects. Errors are returned, errno
 * preserved. Condition predicates MUST be checked under the mutex in a loop.
 * DEFAULT is ERRORCHECK. Recursive wait with depth >1 returns EINVAL. */
typedef uint64_t tmc_pthread_t;
typedef tmc_key_t tmc_pthread_key_t;
typedef struct {int detachstate;} tmc_pthread_attr_t;
typedef struct {int type,pshared;} tmc_pthread_mutexattr_t;
typedef struct {int pshared,clock;} tmc_pthread_condattr_t;
typedef struct {struct tmc_mutex gate;uint64_t owner;unsigned depth;int type,dead;} tmc_pthread_mutex_t;
typedef struct {struct tmc_condition native;uint32_t waiters;int dead,clock;} tmc_pthread_cond_t;
typedef struct {struct tmc_mutex gate;struct tmc_condition changed;uint64_t owner;unsigned state;} tmc_pthread_once_t;
#define TMC_PTHREAD_MUTEX_INITIALIZER {TMC_MUTEX_INITIALIZER,0,0,0,0}
#define TMC_PTHREAD_COND_INITIALIZER {TMC_CONDITION_INITIALIZER,0,0,TMC_CLOCK_REALTIME}
#define TMC_PTHREAD_ONCE_INIT {TMC_MUTEX_INITIALIZER,TMC_CONDITION_INITIALIZER,0,0}
enum {TMC_PTHREAD_MUTEX_ERRORCHECK=0,TMC_PTHREAD_MUTEX_RECURSIVE=1,TMC_PTHREAD_MUTEX_NORMAL=2};
enum {TMC_PTHREAD_CREATE_JOINABLE=0,TMC_PTHREAD_CREATE_DETACHED=1,TMC_PTHREAD_PROCESS_PRIVATE=0,TMC_PTHREAD_PROCESS_SHARED=1};
int tmc_pthread_attr_init(tmc_pthread_attr_t*);
int tmc_pthread_attr_destroy(tmc_pthread_attr_t*);
int tmc_pthread_attr_setdetachstate(tmc_pthread_attr_t*,int);
int tmc_pthread_create(tmc_pthread_t*,const tmc_pthread_attr_t*,void*(*)(void*),void*);
int tmc_pthread_join(tmc_pthread_t,void**);
int tmc_pthread_detach(tmc_pthread_t);
tmc_pthread_t tmc_pthread_self(void);
int tmc_pthread_equal(tmc_pthread_t,tmc_pthread_t);
__attribute__((noreturn)) void tmc_pthread_exit(void*);
int tmc_pthread_key_create(tmc_pthread_key_t*,void(*)(void*));
int tmc_pthread_key_delete(tmc_pthread_key_t);
int tmc_pthread_setspecific(tmc_pthread_key_t,const void*);
void *tmc_pthread_getspecific(tmc_pthread_key_t);
int tmc_pthread_mutexattr_init(tmc_pthread_mutexattr_t*);
int tmc_pthread_mutexattr_destroy(tmc_pthread_mutexattr_t*);
int tmc_pthread_mutexattr_settype(tmc_pthread_mutexattr_t*,int);
int tmc_pthread_mutexattr_setpshared(tmc_pthread_mutexattr_t*,int);
int tmc_pthread_mutex_init(tmc_pthread_mutex_t*,const tmc_pthread_mutexattr_t*);
int tmc_pthread_mutex_destroy(tmc_pthread_mutex_t*);
int tmc_pthread_mutex_lock(tmc_pthread_mutex_t*);
int tmc_pthread_mutex_trylock(tmc_pthread_mutex_t*);
int tmc_pthread_mutex_unlock(tmc_pthread_mutex_t*);
int tmc_pthread_condattr_init(tmc_pthread_condattr_t*);
int tmc_pthread_condattr_destroy(tmc_pthread_condattr_t*);
int tmc_pthread_condattr_setpshared(tmc_pthread_condattr_t*,int);
int tmc_pthread_condattr_setclock(tmc_pthread_condattr_t*,int);
int tmc_pthread_condattr_getclock(const tmc_pthread_condattr_t*,int*);
/* Default REALTIME timedwait fails ENOTSUP: no trusted UTC source yet.
 * Set condattr clock to MONOTONIC for actual absolute timespec waits. */
int tmc_pthread_cond_timedwait(tmc_pthread_cond_t*,tmc_pthread_mutex_t*,const struct tmc_timespec*);
int tmc_pthread_cond_init(tmc_pthread_cond_t*,const tmc_pthread_condattr_t*);
int tmc_pthread_cond_destroy(tmc_pthread_cond_t*);
int tmc_pthread_cond_wait(tmc_pthread_cond_t*,tmc_pthread_mutex_t*);
int tmc_pthread_cond_signal(tmc_pthread_cond_t*);
int tmc_pthread_cond_broadcast(tmc_pthread_cond_t*);
int tmc_pthread_once(tmc_pthread_once_t*,void(*)(void));
/* Explicit NON-POSIX extension: absolute native monotonic 100Hz ticks, 0=forever.
 * Unlike pthread_cond_timedwait, this is not a wall-clock timespec contract. */
int tmc_pthread_cond_wait_until(tmc_pthread_cond_t*,tmc_pthread_mutex_t*,uint64_t);
#ifdef __cplusplus
}
#endif
#endif
