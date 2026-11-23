/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../kernel/k.h"
#include "linux_usb.h"
/* Kernel-side services for the Linux USB/input compatibility subset.
 * The pre-existing user-space POSIX libc cannot perform these operations.
 * Both GFP classes are non-sleeping here and fail on heap exhaustion.
 */
static Spin linux_usb_lock;
void *tmc_lx_os_alloc(size_t n){return n&&n<=65536?kmalloc(n):0;}
void tmc_lx_os_free(void *p){kfree(p);}
void *tmc_lx_os_dma_alloc(size_t n,u64 *dma)
{
 if(!n||n>PAGE||!dma)return 0;
 u64 p=page_get();if(!p)return 0;*dma=p;return (void *)p;
}
void tmc_lx_os_dma_free(void *p,u64 dma){if(p&&(u64)p==dma)page_put(dma);}
u32 tmc_lx_os_lock(void){u32 flags;spin_lock(&linux_usb_lock,&flags);return flags;}
void tmc_lx_os_unlock(u32 flags){spin_unlock(&linux_usb_lock,flags);}
void tmc_lx_os_pointer(int x,int y,int buttons,int wheel){hid_relative(x,y,buttons,wheel);}
void tmc_lx_os_error(const char *message){logstr(message);logstr("\n");}

void tmc_lx_os_keyboard(const u8 bitmap[32],unsigned mods){hid_linux_keys(bitmap,mods);}
u32 tmc_lx_os_irq_save(void){return irq_save();}
void tmc_lx_os_irq_restore(u32 flags){irq_restore(flags);}
void tmc_lx_os_spin_wait(void){tlb_shoot_poll();asm volatile("pause");}
