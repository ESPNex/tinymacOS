#pragma once
#include "base.h"
#include "hidtablet.h"
/* Real upstream Linux Boot mouse driver on a native HCD transport adapter.
 * This API does not claim a Linux xHCI/EHCI/USB-core port. */
int tmc_linux_mouse_attach(void *key,unsigned vid,unsigned pid,unsigned packet,unsigned interval);
int tmc_linux_mouse_report(void *key,const u8 *bytes,unsigned length);
void tmc_linux_mouse_detach(void *key);
int tmc_linux_mouse_boot_layout(const TabletLayout *layout);
typedef struct {u64 probes,attached,completions,frames,errors,rejected,disconnected,allocations,frees;} TmcLinuxUsbStats;
void tmc_linux_mouse_stats(TmcLinuxUsbStats *out);
/* OS imports implemented by the native kernel, not user-space syscalls. */
void *tmc_lx_os_alloc(size_t);
void tmc_lx_os_free(void *);
void *tmc_lx_os_dma_alloc(size_t,u64 *);
void tmc_lx_os_dma_free(void *,u64);
u32 tmc_lx_os_lock(void);
void tmc_lx_os_unlock(u32);
void tmc_lx_os_pointer(int,int,int,int);
void tmc_lx_os_error(const char *);
