/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "base.h"
/* Whole original EHCI schedule/PCI/root-hub driver, on a serialized CPU0
 * polling runtime. No hardware IRQ router, system PM, or USB core bus model
 * is claimed. Native UsbInput enumeration/class adapter remains separate. */
typedef struct TmcEhciDevice TmcEhciDevice;
int tmc_linux_ehci_module_init(void);
void tmc_linux_ehci_module_exit(void);
void tmc_linux_ehci_poll(void);
unsigned tmc_linux_ehci_controllers(void);
unsigned tmc_linux_ehci_ports(unsigned);
int tmc_linux_ehci_hub(unsigned,u16,u16,u16,void *,unsigned);
TmcEhciDevice *tmc_linux_ehci_device(unsigned,unsigned);
void tmc_linux_ehci_device_state(TmcEhciDevice *,unsigned address,unsigned maxpacket,unsigned configured);
int tmc_linux_ehci_control(TmcEhciDevice *,unsigned,unsigned,unsigned,unsigned,void *,unsigned,unsigned);
/* Callbacks run after DMA is unmapped and outside the HCD schedule lock.
 * A callback may resubmit; device teardown from callbacks is not allowed. */
typedef void (*TmcEhciComplete)(void *,int,unsigned);
int tmc_linux_ehci_interrupt(TmcEhciDevice *,unsigned ep,unsigned packet,unsigned interval,void *,TmcEhciComplete,void *);
void tmc_linux_ehci_reset_toggle(TmcEhciDevice *,unsigned ep);
int tmc_linux_ehci_interrupt_stop(TmcEhciDevice *,unsigned ep);
int tmc_linux_ehci_translator(TmcEhciDevice *,TmcEhciDevice *hub,unsigned port,unsigned multi,unsigned think_time);
int tmc_linux_ehci_device_destroy(TmcEhciDevice *);
typedef struct {u64 controllers,submissions,givebacks,unlinks,map_failures,irq_polls,schedule_polls,timer_fires,root_polls,errors;} TmcEhciStats;
void tmc_linux_ehci_stats(TmcEhciStats *);

/* Endpoint token: 1..15 IN (legacy ABI), 0x100|1..15 OUT. Exact report length. */
int tmc_linux_ehci_interrupt_report(TmcEhciDevice *,unsigned,unsigned,unsigned,unsigned,void *,TmcEhciComplete,void *);

/* Additive storage bulk transport: original HCD URBs/DMA; <=64KiB per URB.
 * SS burst 0..15, zero for USB2. No streams/UAS/SG claims. */
int tmc_linux_ehci_bulk_submit(TmcEhciDevice *,unsigned,unsigned,unsigned,unsigned,void *,TmcEhciComplete,void *);
int tmc_linux_ehci_bulk_stop(TmcEhciDevice *,unsigned);
