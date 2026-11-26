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
 * Whole free spans return to S_VM; a failed release retains a reusable span.
 * free(NULL) and successful free preserve errno. Invalid/double free traps;
 * buffer overruns and arbitrary invalid pointers are outside the C contract.
 * No async-signal safety/fork/allocator interposition protocol is provided. */
void *tmc_heap_alloc(size_t);
void *tmc_heap_calloc(size_t,size_t);
void *tmc_heap_realloc(void *,size_t);
void *tmc_heap_aligned_alloc(size_t,size_t);
void tmc_heap_free(void *);
struct tmc_heap_stats {size_t reserved,requested,live,spans;};
struct tmc_heap_stats tmc_heap_statistics(void);
#ifdef __cplusplus
}
#endif
#endif
