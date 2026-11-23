#pragma once
#include "base.h"
#include "hidtablet.h"
/* Real upstream Linux Boot mouse driver on a native HCD transport adapter.
 * This API does not claim a Linux xHCI/EHCI/USB-core port. */
int tmc_linux_mouse_attach(void *key,unsigned vid,unsigned pid,unsigned packet,unsigned interval);
int tmc_linux_mouse_report(void *key,const u8 *bytes,unsigned length);
void tmc_linux_mouse_detach(void *key);
/* Negative Linux errno: transient failures resubmit; cancellation/shutdown stop. */
int tmc_linux_mouse_error(void *key,int status);
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
/* Real upstream usbkbd on the same native-HCD adapter. Control requests are
 * copied to native EP0 outside the compatibility lock; completion is deferred
 * and generation-checked. This does not implement the general Linux core.
 */
typedef int (*TmcLinuxUsbControl)(void *,const u8 setup[8],u8 *,unsigned);
int tmc_linux_keyboard_attach(void *,unsigned,unsigned,unsigned,unsigned,unsigned,TmcLinuxUsbControl,void *);
int tmc_linux_keyboard_report(void *,const u8 *,unsigned);
void tmc_linux_keyboard_detach(void *);
void tmc_linux_keyboard_leds(unsigned);
void tmc_linux_usb_service(void);
typedef struct {u64 probes,attached,completions,frames,rejected,disconnected,controls,control_errors,stale_controls;} TmcLinuxKeyboardStats;
void tmc_linux_keyboard_stats(TmcLinuxKeyboardStats *);
void tmc_lx_os_keyboard(const u8 bitmap[32],unsigned modifiers);
u32 tmc_lx_os_irq_save(void);
void tmc_lx_os_irq_restore(u32);

void tmc_lx_os_spin_wait(void);

/* Upstream core/urb.c is distinct from this limited native-HCD backend. */
typedef struct {u64 submits,givebacks,unlinks,rejected,active,highwater;} TmcLinuxUrbStats;
void tmc_linux_urb_stats(TmcLinuxUrbStats *);
u64 tmc_lx_os_clock_ms(void);
