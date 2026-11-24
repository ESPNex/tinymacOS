/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
/* Host fixture only. Hardware imports are deliberately undefined: only real
 * DMA setup/build/rearm/rollback code is retained by --gc-sections. */
#include "base.h"
#include "inputdiag.h"
extern u64 ticks;
extern InputDiag input_diag;
u32 pci_reg(u32,unsigned);
u32 pci_find_after(u32,u32);
void *pci_mmio(u32,int);
void outb(u16,u8);
void outw(u16,u16);
void outl(u16,u32);
u16 inw(u16);
void device_add(const char *,const char *,int);
void hid_keys(const u8 [8]);
