#ifndef TMC_THREADS_H
#define TMC_THREADS_H
#include <stdint.h>
#include "tmc_tls.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native caller-owned runtime, NOT full pthread/ELF TLS/C++ standard library.
 * Zero-initialize storage before first use. Never move/copy a live object.
 * create/join for the same object require ordinary external lifecycle ordering.
 * Storage stays mapped through a successful join; timeout does NOT end lifetime.
 * Reuse is allowed after successful join. No detach/cancel, dynamic allocation,
 * compiler thread_local/__thread or exception unwinding implementation here.
 * All int errors are returned directly; these routines do not set errno. */
#define TMC_TSD_KEYS 32u
#define TMC_TSD_DESTRUCTOR_ITERATIONS 4u
typedef uint64_t tmc_key_t;
struct tmc_specific_slot { uint64_t generation; void *value; };
struct tmc_thread {
    struct tmc_tls_block tls; /* MUST remain first: native FS points here */
    uint64_t identity;
    uint32_t state, alive, join_claim, initial_thread, exiting;
    void *(*entry)(void *);
    void *argument, *result;
    struct tmc_specific_slot specific[TMC_TSD_KEYS];
};
#define TMC_THREAD_INITIALIZER {0}
/* Explicit bootstrap only for initial/foreign native thread. New threads made
 * by tmc_thread_create attach TLS automatically BEFORE invoking the callback. */
int tmc_runtime_enter(struct tmc_thread *);
int tmc_runtime_leave(void); /* initial/foreign only; runs TSD cleanup */
struct tmc_thread *tmc_thread_current(void);
uint64_t tmc_thread_id(void); /* 0 if unattached; process-local, never reused */
int tmc_thread_create(struct tmc_thread *,void *(*)(void *),void *);
int tmc_thread_join_until(struct tmc_thread *,void **result,uint64_t absolute_ticks);
int tmc_thread_join(struct tmc_thread *,void **result);
/* Managed child only. Destructors run BEFORE kernel publishes completion.
 * Recursive exit from a destructor is unsupported and fails closed. */
__attribute__((noreturn)) void tmc_thread_exit(void *result);
int tmc_key_create(tmc_key_t *,void (*destructor)(void *));
int tmc_key_delete(tmc_key_t); /* no destructor on delete; stale handles invalid */
int tmc_setspecific(tmc_key_t,const void *);
void *tmc_getspecific(tmc_key_t); /* NULL for absent/deleted/stale key */
#ifdef __cplusplus
}
#endif
#endif
