/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Host-only OS import provider for existing real native HCD/input contracts.
 * Links the actual Linux driver and compatibility core, not a report mock. */
#include <time.h>
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

/* Fixtures without the real native input consumer keep their existing Boot
 * report observation API. The actual Linux driver still supplies key events. */
extern void hid_linux_keys(const u8[32],unsigned) __attribute__((weak));
extern void hid_keys(const u8[8]);
void tmc_lx_os_keyboard(const u8 bitmap[32],unsigned mods)
{
 if(hid_linux_keys){hid_linux_keys(bitmap,mods);return;}
 u8 report[8]={(u8)mods,0};unsigned n=2;
 for(unsigned i=4;i<224;i++)if(bitmap[i/8]&(1u<<(i%8))){assert(n<8);report[n++]=i;}
 hid_keys(report);
}
u32 tmc_lx_os_irq_save(void){return 0;}
void tmc_lx_os_irq_restore(u32 flags){assert(flags==0||flags==0x200);}
void tmc_lx_os_spin_wait(void){__asm__ volatile("pause");}

u64 tmc_lx_os_clock_ms(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (u64)t.tv_sec*1000+t.tv_nsec/1000000;}
