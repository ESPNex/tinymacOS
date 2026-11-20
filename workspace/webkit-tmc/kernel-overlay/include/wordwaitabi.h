#pragma once
/* Private address-space 32-bit compare-and-park ABI, v1. No shared-process
 * futexes, robust lists, PI locks or POSIX signal/cancellation semantics. */
#define TMC_WORD_QUERY 0u
#define TMC_WORD_WAIT 1u
#define TMC_WORD_WAKE 2u
#define TMC_WORD_ABI_VERSION 1u
#define TMC_WORD_INVALID (-1l)
#define TMC_WORD_CHANGED (-2l)
#define TMC_WORD_TIMEOUT (-3l)
/* S_WORDWAIT(op, aligned writable user word, expected/count, deadline).
 * WAIT: atomic compare + enqueue with respect to WAKE under sched_lock.
 * deadline is ABSOLUTE native 100 Hz ticks; 0 means indefinitely.
 * WAIT returns 0 after wake (possibly spurious), CHANGED, TIMEOUT or INVALID.
 * WAKE returns number awakened, count 0 is a no-op, UINT32_MAX wakes all.
 * Identity is (process/address space, virtual address), never physical page.
 * Keep storage live until all waiters return: no requeue, unmap cancellation
 * or object lifetime management. Waking does not transfer mutex ownership.
 */
