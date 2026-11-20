#ifndef TMC_SYNC_H
#define TMC_SYNC_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native private parking primitives, NOT a POSIX pthread implementation.
 * All words/objects must stay mapped and alive until every waiter returns.
 * Deadlines: absolute 100 Hz kernel ticks, zero=infinite; spurious wakeups
 * permitted. No cancellation, ownership tracking, priority inheritance,
 * recursive/robust mutexes, process-shared storage or destruction protocol.
 * Object words are private implementation details: never write them directly.
 * Initialize by zero BEFORE sharing; do not reinitialize while in use.
 * Errors are RETURNED, pthread-style: errno/TCB is not touched here. */
struct tmc_mutex { uint32_t state; };
struct tmc_condition { uint32_t sequence; };
#define TMC_MUTEX_INITIALIZER {0}
#define TMC_CONDITION_INITIALIZER {0}
int tmc_word_query(void); /* ABI version, -1 if unavailable */
int tmc_word_wait(uint32_t *,uint32_t expected,uint64_t deadline_ticks);
/* wait returns 0/EAGAIN/ETIMEDOUT/EINVAL; wake returns count or -EINVAL. */
int tmc_word_wake(uint32_t *,uint32_t count);
int tmc_mutex_trylock(struct tmc_mutex *);
int tmc_mutex_lock_until(struct tmc_mutex *,uint64_t deadline_ticks);
int tmc_mutex_lock(struct tmc_mutex *);
int tmc_mutex_unlock(struct tmc_mutex *);
/* Caller MUST hold the mutex on entry; it is reacquired before return,
 * including on timeout. Always recheck predicate under mutex in a loop.
 * signal/broadcast should update the predicate under that same mutex.
 * Sequence counter is 32-bit; object lifetime must prevent ABA on wrap. */
int tmc_condition_wait(struct tmc_condition *,struct tmc_mutex *,uint64_t deadline_ticks);
int tmc_condition_signal(struct tmc_condition *);
int tmc_condition_broadcast(struct tmc_condition *);
#ifdef __cplusplus
}
#endif
#endif
