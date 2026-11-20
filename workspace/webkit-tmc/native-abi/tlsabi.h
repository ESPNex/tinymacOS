#pragma once
/* Native x86-64 thread-pointer ABI. Not a POSIX pthread or ELF TLS loader. */
#define TMC_TLS_GET 0u
#define TMC_TLS_SET 1u
#define TMC_TLS_QUERY 2u
#define TMC_TLS_ABI_VERSION 1u
/* SET(0) clears the thread pointer. Otherwise require an aligned writable user
 * word at FS base. The caller owns the TCB/mappings and must keep them live.
 * Native new threads start with FS=0 (never inherit another thread's TCB).
 * GS remains reserved for the kernel's per-CPU block. */
