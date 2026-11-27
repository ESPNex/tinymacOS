#pragma once
#include "base.h"
/* Shared OS services required by original physical HCDs, not a class facade. */
void *tmc_lx_os_alloc(size_t);void tmc_lx_os_free(void *);
void *tmc_lx_os_dma_alloc(size_t,u64 *);void tmc_lx_os_dma_free(void *,u64);
u32 tmc_lx_os_lock(void);void tmc_lx_os_unlock(u32);
void tmc_lx_os_error(const char *);u32 tmc_lx_os_irq_save(void);void tmc_lx_os_irq_restore(u32);
void tmc_lx_os_spin_wait(void);u64 tmc_lx_os_clock_ms(void);u64 tmc_lx_os_clock_ns(void);
