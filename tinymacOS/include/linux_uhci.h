/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "base.h"
/* Whole original UHCI schedule/PCI/root-hub driver, on a serialized CPU0
 * polling runtime. No hardware IRQ router, system PM, or USB core bus model
 * is claimed. Native UsbInput enumeration/class adapter remains separate. */
typedef struct TmcUhciDevice TmcUhciDevice;
int tmc_linux_uhci_module_init(void);
void tmc_linux_uhci_module_exit(void);
void tmc_linux_uhci_poll(void);
unsigned tmc_linux_uhci_controllers(void);
unsigned tmc_linux_uhci_ports(unsigned);
int tmc_linux_uhci_hub(unsigned,u16,u16,u16,void *,unsigned);
TmcUhciDevice *tmc_linux_uhci_device(unsigned,unsigned);
void tmc_linux_uhci_device_state(TmcUhciDevice *,unsigned address,unsigned maxpacket,unsigned configured);
int tmc_linux_uhci_control(TmcUhciDevice *,unsigned,unsigned,unsigned,unsigned,void *,unsigned,unsigned);
/* Callbacks run after DMA is unmapped and outside the HCD schedule lock.
 * A callback may resubmit; device teardown from callbacks is not allowed. */
typedef void (*TmcUhciComplete)(void *,int,unsigned);
int tmc_linux_uhci_interrupt(TmcUhciDevice *,unsigned ep,unsigned packet,unsigned interval,void *,TmcUhciComplete,void *);
void tmc_linux_uhci_reset_toggle(TmcUhciDevice *,unsigned ep);
int tmc_linux_uhci_interrupt_stop(TmcUhciDevice *,unsigned ep);
int tmc_linux_uhci_device_destroy(TmcUhciDevice *);
typedef struct {u64 controllers,submissions,givebacks,unlinks,map_failures,irq_polls,schedule_polls,timer_fires,root_polls,errors;} TmcUhciStats;
void tmc_linux_uhci_stats(TmcUhciStats *);
