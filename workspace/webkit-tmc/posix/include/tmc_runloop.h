#ifndef TMC_RUNLOOP_H
#define TMC_RUNLOOP_H
#include "tmc_sync.h"
#ifdef __cplusplus
extern "C" {
#endif
struct tmc_loop_task;
struct tmc_runloop {
 struct tmc_mutex lock;
 struct tmc_condition changed;
 struct tmc_loop_task *head;
 uint64_t active,owner;
 unsigned running,stopped,dead;
};
#define TMC_RUNLOOP_INITIALIZER {TMC_MUTEX_INITIALIZER,TMC_CONDITION_INITIALIZER,0,0,0,0,0,0}
/* Native substrate, NOT WTF::RunLoop or a file/socket event multiplexer.
 * Attach a native TCB first. Stable caller-owned object; no live copy/reset.
 * One dispatcher calls run; many producers may post/cancel/stop. No internal
 * thread creation. Idle waits park, earliest timer changes wake the dispatcher.
 * Absolute boot MONOTONIC ticks at 100Hz, zero means immediate. Equal deadlines
 * execute in posting order. No callback executes early. Overdue work runs ASAP.
 * All API errors are returned, errno preserved. Allocation failure leaves
 * output token/queue/argument ownership unchanged. Tokens never reused.
 * On successful post, cleanup (if supplied) runs exactly once AFTER callback,
 * or instead of it after pending cancellation/stop; no callbacks under locks.
 * Callbacks/cleanup must return normally, may post/cancel/stop (not destroy).
 * They run on dispatcher, cancel caller, or stop caller respectively: argument
 * ownership must accommodate this. cancel(active)=EBUSY, consumed/unknown=ESRCH.
 * stop rejects new posts (ESHUTDOWN), cancels pending, lets active finish; run
 * then returns. stop is idempotent, including before run. Stop is not a join:
 * join dispatcher AND quiesce ALL API/cleanup callers before destroy.
 * Destroy requires stopped and no dispatcher; it never frees caller storage.
 * Queue is dynamically allocated, O(n) insertion/cancel; no hard realtime,
 * GUI integration, IO polling, recurring timers or security isolation claim. */
int tmc_runloop_post(struct tmc_runloop*,uint64_t,void(*)(void*),void*,void(*)(void*),uint64_t*);
int tmc_runloop_cancel(struct tmc_runloop*,uint64_t);
int tmc_runloop_run(struct tmc_runloop*);
int tmc_runloop_stop(struct tmc_runloop*);
int tmc_runloop_destroy(struct tmc_runloop*);
#ifdef __cplusplus
}
#endif
#endif
