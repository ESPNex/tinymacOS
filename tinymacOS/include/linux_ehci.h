/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "base.h"
/* Hardware qTD prefix; Linux software metadata is private to the adapter.
 * This is the upstream DMA descriptor-building primitive, not a claim that
 * the native EHCI controller/scheduler has become the complete Linux HCD.
 */
typedef struct PACK {
    u32 next,alt,token,buf[5],hi[5];
} TmcLinuxEhciQtd;
/* Returns bytes covered (possibly shorter than n for a >five-page transfer),
 * or a negative error. Leaves the destination unchanged on invalid input.
 * Caller supplies next/alternate links after construction and owns ordering
 * against DMA. Never modify a live descriptor through this function.
 */
int tmc_linux_ehci_qtd_build(void *destination,size_t bytes,u64 dma,size_t n,u32 token,unsigned maxpacket);
typedef struct {u64 calls,bytes,rejected;} TmcLinuxEhciQtdStats;
