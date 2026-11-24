/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "base.h"
/* Whole original xHCI ring/memory/hub/PCI controller driver, on a serialized CPU0
 * polling runtime. No hardware IRQ router, system PM, or USB core bus model
 * is claimed. Native UsbInput enumeration/class adapter remains separate. */
typedef struct TmcXhciDevice TmcXhciDevice;
int tmc_linux_xhci_module_init(void);
void tmc_linux_xhci_module_exit(void);
void tmc_linux_xhci_poll(void);
unsigned tmc_linux_xhci_controllers(void);
unsigned tmc_linux_xhci_ports(unsigned);
int tmc_linux_xhci_hub(unsigned,u16,u16,u16,void *,unsigned);
TmcXhciDevice *tmc_linux_xhci_device(unsigned,unsigned);
void tmc_linux_xhci_device_state(TmcXhciDevice *,unsigned address,unsigned maxpacket,unsigned configured);
int tmc_linux_xhci_control(TmcXhciDevice *,unsigned,unsigned,unsigned,unsigned,void *,unsigned,unsigned);
/* Callbacks run after DMA is unmapped and outside the HCD schedule lock.
 * A callback may resubmit; device teardown from callbacks is not allowed. */
typedef void (*TmcXhciComplete)(void *,int,unsigned);
int tmc_linux_xhci_interrupt(TmcXhciDevice *,unsigned ep,unsigned packet,unsigned interval,void *,TmcXhciComplete,void *);
void tmc_linux_xhci_reset_toggle(TmcXhciDevice *,unsigned ep);
/* Exact normalized SuperSpeed interrupt companion from original config.c. */
int tmc_linux_xhci_interrupt_companion(TmcXhciDevice *,unsigned,unsigned,unsigned,unsigned);
int tmc_linux_xhci_interrupt_stop(TmcXhciDevice *,unsigned ep);
int tmc_linux_xhci_translator(TmcXhciDevice *,TmcXhciDevice *hub,unsigned port,unsigned multi,unsigned think_time);
int tmc_linux_xhci_device_destroy(TmcXhciDevice *);
typedef struct {u64 controllers,submissions,givebacks,unlinks,map_failures,irq_polls,schedule_polls,timer_fires,root_polls,errors;} TmcXhciStats;
void tmc_linux_xhci_stats(TmcXhciStats *);

int tmc_linux_xhci_device_port(TmcXhciDevice *,TmcXhciDevice *,unsigned);
int tmc_linux_xhci_hub_device(TmcXhciDevice *,unsigned,unsigned,unsigned);

u32 tmc_linux_xhci_pci_address(unsigned);

/* Actual address assigned by original xHCI address_device, not slot/devnum. */
unsigned tmc_linux_xhci_device_address(TmcXhciDevice *);

/* Endpoint token: 1..15 IN (legacy ABI), 0x100|1..15 OUT. Exact report length. */
int tmc_linux_xhci_interrupt_report(TmcXhciDevice *,unsigned,unsigned,unsigned,unsigned,void *,TmcXhciComplete,void *);
