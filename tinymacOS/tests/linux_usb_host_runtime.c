/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Host-only OS import provider for existing real native HCD/input contracts.
 * Links the actual Linux driver and compatibility core, not a report mock. */
#include <stdlib.h>
#include <assert.h>
#include "linux_usb.h"
extern void hid_relative(int,int,int,int);
static unsigned lock_word;
void *tmc_lx_os_alloc(size_t n){return malloc(n);}
void tmc_lx_os_free(void *p){free(p);}
void *tmc_lx_os_dma_alloc(size_t n,u64 *dma){if(!n||n>4096)return 0;void *p=aligned_alloc(4096,4096);*dma=(u64)p;return p;}
void tmc_lx_os_dma_free(void *p,u64 dma){assert((u64)p==dma);free(p);}
u32 tmc_lx_os_lock(void){while(__sync_lock_test_and_set(&lock_word,1)){}return 0;}
void tmc_lx_os_unlock(u32 f){(void)f;__sync_lock_release(&lock_word);}
void tmc_lx_os_pointer(int x,int y,int b,int w){hid_relative(x,y,b,w);}
void tmc_lx_os_error(const char *s){(void)s;}
