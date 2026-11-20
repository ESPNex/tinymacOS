#ifndef TMC_VM_BACKEND_H
#define TMC_VM_BACKEND_H
#include "tmc_backend.h"
#include "vmabi.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native VM foundation, NOT mmap/madvise and not yet WTF::OSAllocator.
 * Sizes are nonzero page multiples, without implicit rounding. Reserve returns
 * payload address excluding optional guards; guards cannot be committed.
 * Commit requires an entirely uncommitted subrange; protect requires an entirely
 * committed subrange, including retained PROT_NONE pages. Decommit is idempotent,
 * releases physical backing and recommit reads zeroes. Release must be exact.
 * v1 only supports 1 online CPU, 32 reservations, 64 MiB per region and 240 MiB
 * arena; mutations fail ENOTSUP on SMP. READ or READ|WRITE or NONE only, NO EXEC.
 * Callers must serialize their own accesses while protections/lifetimes change. */
int tmc_vm_info(struct TmcVMInfo *);
void *tmc_vm_reserve(size_t bytes,size_t alignment,unsigned guard_pages_each_end);
int tmc_vm_commit(void *,size_t,unsigned protection);
int tmc_vm_protect(void *,size_t,unsigned protection);
int tmc_vm_decommit(void *,size_t);
int tmc_vm_release(void *,size_t);
#ifdef __cplusplus
}
#endif
#endif
