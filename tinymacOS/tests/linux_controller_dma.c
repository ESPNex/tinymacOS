/* SPDX-License-Identifier: GPL-2.0-only */
/* The former native EHCI scheduler was replaced, not retained as a test-only
 * implementation. Exercise its actual whole-original replacement, including
 * 128 complete controller/device DMA lifetimes, on nonidentity bus handles. */
#define TMC_EHCI_DMA_STRESS 128
#include "linux_ehci_hcd.c"
