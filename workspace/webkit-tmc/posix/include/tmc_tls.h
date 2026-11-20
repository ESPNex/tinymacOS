#ifndef TMC_TLS_H
#define TMC_TLS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Caller-owned per-thread control block. Not a complete libc TCB/DTV, C++ TLS,
 * __thread runtime or pthread implementation. Initialize separately in EACH
 * native thread and keep the memory mapped until uninstall/thread exit. */
struct tmc_tls_block {
    uint64_t self;
    int error_number;
    uint32_t reserved;
    uint64_t cookie;
};
long tmc_tls_install(struct tmc_tls_block *);
long tmc_tls_uninstall(void);
struct tmc_tls_block *tmc_tls_current(void);
/* NULL when the native thread has no valid installed TCB; no shared fallback.
 * Target errno.h now resolves through this cell and traps if no TCB exists.
 * Startup/thread attachment remains caller-owned, not a pthread runtime. */
int *tmc_tls_errno_location(void);
#ifdef __cplusplus
}
#endif
#endif
