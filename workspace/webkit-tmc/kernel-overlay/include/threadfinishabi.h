#pragma once
/* Native thread final handoff. This is not pthread_join or a detached-thread
 * allocator. QUERY=0 returns 1. FINISH=1 consumes an aligned writable 32-bit
 * user completion word in argument b, with c=d=0. Invalid requests return -1.
 * A valid FINISH never returns: clear FS, release-store 0 to the word, wake
 * private word waiters and retire the calling thread under sched_lock.
 * No user completion storage is accessed after publication. Its caller may
 * reuse/unmap it after observing zero with acquire. Other user stores before
 * the syscall (result, destructor side effects) are published by this release.
 * Last thread exiting terminates the process, as for native S_EXIT. */
#define TMC_THREAD_FINISH_QUERY 0u
#define TMC_THREAD_FINISH 1u
#define TMC_THREAD_FINISH_VERSION 1u
