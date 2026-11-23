/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../kernel/k.h"
#include "linux_usb.h"
#include "usbhost.h"
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

static u64 linux_tsc_per_ms;
static u64 linux_tsc(void)
{
 u32 lo,hi;asm volatile("lfence; rdtsc":"=a"(lo),"=d"(hi)::"memory");return (u64)hi<<32|lo;
}
u64 tmc_lx_os_clock_ms(void)
{
 /* Polled USB core runs on CPU0; timeouts must advance even while the
  * compatibility/anchor IRQ-safe lock has disabled scheduler interrupts.
  * Derive TSC frequency from CPUID crystal ratio, or measure it against
  * the same independent PIT channel-2 used by native HCD reset delays.
  * This is not a cross-CPU clock or suspend/resume timekeeping port.
  */
 if(!linux_tsc_per_ms){
  u32 flags=irq_save(),max,b,c,d,a;asm volatile("cpuid":"=a"(max),"=b"(b),"=c"(c),"=d"(d):"a"(0),"c"(0));
  u64 rate=0;
  if(max>=0x15){asm volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x15),"c"(0));if(a&&b&&c)rate=(u64)c*b/a/1000;}
  if(!rate){u64 start=linux_tsc();usb_delay_ms(10);rate=(linux_tsc()-start)/10;}
  linux_tsc_per_ms=rate?rate:1;irq_restore(flags);
 }
 return linux_tsc()/linux_tsc_per_ms;
}
