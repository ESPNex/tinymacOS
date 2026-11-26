#ifndef TMC_HEAP_H
#define TMC_HEAP_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native VM-backed process heap. Requires attached per-thread errno/TCB and
 * S_VM/S_WORDWAIT. All metadata is protected by the native parking mutex.
 * Not bmalloc, a collector, executable memory, or a complete libc/C++ runtime.
 * 16-byte fundamental alignment; aligned_alloc follows size%alignment==0.
 * malloc(0) returns a freeable allocation; realloc(p,0) frees and returns NULL.
 * Overflow/OOM returns NULL/ENOMEM, preserving a failed realloc's old object.
 * Default: whole free spans return to S_VM. Opt-in bounded cache retains at
 * most 256KiB of fully free spans; large spans still return immediately.
 * A failed release always retains tracked ownership, even over the budget.
 * free(NULL) and successful free preserve errno. Invalid/double free traps;
 * buffer overruns and arbitrary invalid pointers are outside the C contract.
 * No async-signal safety/fork/allocator interposition protocol is provided. */
void *tmc_heap_alloc(size_t);
void *tmc_heap_calloc(size_t,size_t);
void *tmc_heap_realloc(void *,size_t);
void *tmc_heap_aligned_alloc(size_t,size_t);
void tmc_heap_free(void *);
/* Set cache budget 0..256KiB, initially 0. Shrinking releases eligible empty
 * spans. trim releases ALL empty/uncommitted spans, never live objects, and
 * keeps the budget for future frees. Failed releases remain tracked/retryable;
 * -1/errno reports failure, success preserves errno. Slot/memory pressure can
 * reclaim the cache before allocation fails. All operations are synchronized. */
int tmc_heap_set_cache_limit(size_t);
int tmc_heap_trim(void);
struct tmc_heap_stats {
 size_t reserved,requested,live,spans;
 size_t cached,cache_limit,reserve_calls,release_calls,released_spans,cache_hits;
};
struct tmc_heap_stats tmc_heap_statistics(void);
#ifdef __cplusplus
}
#endif
#endif
