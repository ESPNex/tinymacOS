/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "base.h"
/* Whole original Linux mm/dmapool.c on implemented native coherent-memory,
 * lock, attribute-registry and managed-resource services. NOT a Linux HCD.
 * Current target: coherent identity-DMA x86-64 without an active IOMMU.
 * Coherent allocations are bounded to 64 KiB and must fit the device mask.
 * Pool/device teardown requires caller quiescence, as the upstream API does. */
typedef struct TmcLinuxDmaDevice TmcLinuxDmaDevice;
typedef struct TmcLinuxDmaPool TmcLinuxDmaPool;
TmcLinuxDmaDevice *tmc_linux_dma_device_create(const char *,u64 mask);
int tmc_linux_dma_device_destroy(TmcLinuxDmaDevice *);
/* Reads the registered ORIGINAL pools_show attribute, not a sysfs VFS port. */
int tmc_linux_dma_device_snapshot(TmcLinuxDmaDevice *,char *,size_t);
TmcLinuxDmaPool *tmc_linux_dma_pool_create(TmcLinuxDmaDevice *,const char *,size_t size,size_t alignment,size_t boundary);
void *tmc_linux_dma_pool_alloc(TmcLinuxDmaPool *,u64 *dma);
int tmc_linux_dma_pool_free(TmcLinuxDmaPool *,void *,u64 dma);
int tmc_linux_dma_pool_destroy(TmcLinuxDmaPool *);
typedef struct {u64 devices,pools,allocations,frees,coherent_allocations,coherent_frees,live_bytes,highwater,rejected,failures,attributes,devres;} TmcLinuxDmaStats;
void tmc_linux_dma_pool_stats(TmcLinuxDmaStats *);
/* Native imports; allocation returns aligned, contiguous, coherent memory.
 * They must not sleep; unsupported flags/masks/limits fail explicitly. */
void *tmc_dma_os_alloc(size_t);
void tmc_dma_os_free(void *);
void *tmc_dma_os_coherent(size_t,u64 *);
void tmc_dma_os_coherent_free(size_t,void *,u64);
u32 tmc_lx_os_irq_save(void);
void tmc_lx_os_irq_restore(u32);
void tmc_lx_os_spin_wait(void);
void tmc_lx_os_error(const char *);
