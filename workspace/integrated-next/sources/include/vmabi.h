#ifndef TMC_VM_ABI_H
#define TMC_VM_ABI_H
#include "base.h"
/* Native VM ABI v1, NOT Linux mmap. No executable mappings/JIT support.
 * Mutations serialize against syscall uaccess; backing reuse follows TLB ACK.
 * This ABI is not a complete POSIX mmap/mprotect implementation. */
#define TMC_VM_VERSION 1
#define TMC_VM_QUERY 0
#define TMC_VM_RESERVE 1
#define TMC_VM_COMMIT 2
#define TMC_VM_PROTECT 3
#define TMC_VM_DECOMMIT 4
#define TMC_VM_RELEASE 5
#define TMC_VM_READ 1
#define TMC_VM_WRITE 2
#define TMC_VM_EXEC 4
#define TMC_VM_SINGLE_CPU_ONLY 1
#define TMC_VM_NO_EXEC 2
#define TMC_VM_AVAILABLE 4
#define TMC_VM_INVALID (-1)
#define TMC_VM_NOMEM (-2)
#define TMC_VM_UNSUPPORTED (-3)
#define TMC_VM_BUSY (-4)
#define TMC_VM_NOT_COMMITTED (-5)
#define TMC_VM_PAGE 4096ull
#define TMC_VM_SLOTS 32
#define TMC_VM_ARENA_START (0x8000000000ull+0x30000000ull)
#define TMC_VM_ARENA_END (0x8000000000ull+0x3f000000ull)
#define TMC_VM_MAX_REGION (64ull*1024*1024)
#define TMC_VM_MAX_ALIGNMENT (2ull*1024*1024)
#define TMC_VM_MAX_GUARD_PAGES 16
struct TmcVMInfo {
    u32 version,page_size,flags,max_regions;
    u64 arena_start,arena_end,max_region,max_alignment;
};
#endif
