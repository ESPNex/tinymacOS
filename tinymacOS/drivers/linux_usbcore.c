#ifdef TMC_USB_CANONICAL
/* SPDX-License-Identifier: GPL-2.0-only */
#include "../kernel/k.h"
#include <stdarg.h>
#include "linux_xhci_hcd.h"
#include "linux_ehci_hcd.h"
#include "usbroute.h"
struct CanonicalCounters {u64 started,keys,pointers,errors,bound_keys,bound_pointers,worker_polls;};
volatile struct CanonicalCounters tmc_usb_canonical_counters;
char tmc_usb_canonical_log[16384];static unsigned canonical_logused;
extern int tmc_usb_stack_init(void);
extern void tmc_usb_stack_poll(void),tmc_usb_stack_leds(unsigned),tmc_usb_physical_poll(void);
extern int tmc_usbcore_vsnprintf(char *,size_t,const char *,va_list);
extern u64 tmc_lx_os_clock_ms(void),tmc_lx_os_clock_ns(void);
extern void usb_delay_ms(unsigned);
void *tmc_usbcore_os_alloc(size_t n){return n&&n<=2*1024*1024?kmalloc(n):NULL;}
void tmc_usbcore_os_free(void *p){kfree(p);}
u64 tmc_usbcore_os_clock_ms(void){return tmc_lx_os_clock_ms();}
void tmc_usbcore_os_delay_us(unsigned n){if(n>=1000){usb_delay_ms((n+999)/1000);return;}u64 start=tmc_lx_os_clock_ns();while(tmc_lx_os_clock_ns()-start<(u64)n*1000)asm volatile("pause");}
void tmc_usbcore_os_poll(void){tmc_usb_physical_poll();}
u32 tmc_usbcore_os_irq_save(void){return irq_save();}
void tmc_usbcore_os_irq_restore(u32 f){irq_restore(f);}
void tmc_usbcore_os_log(unsigned level,const char *f,va_list a){char s[256];tmc_usbcore_vsnprintf(s,sizeof(s),f,a);unsigned z=0;while(s[z]&&z<sizeof(s)-1)z++;if(canonical_logused+z+2<sizeof(tmc_usb_canonical_log)){memcpy(tmc_usb_canonical_log+canonical_logused,s,z);canonical_logused+=z;tmc_usb_canonical_log[canonical_logused++]='\n';tmc_usb_canonical_log[canonical_logused]=0;}if(level){tmc_usb_canonical_counters.errors++;boot_logstr("[USB core] ");boot_logstr(s);boot_logstr("\n");}}
void tmc_usbcore_os_fatal(void){logstr("canonical USB fatal invariant\n");for(;;)asm volatile("cli; hlt");}
void tmc_usbcore_os_entropy(const void *p,unsigned n){(void)p;(void)n;/* no kernel entropy ingestion API: never a USB I/O operation */}
void tmc_usbcore_os_input_registered(unsigned kind,int add){volatile u64 *n=kind==1?&tmc_usb_canonical_counters.bound_keys:&tmc_usb_canonical_counters.bound_pointers;if(add)(*n)++;else if(*n)(*n)--;}
void tmc_usbcore_os_keys(const u8 bitmap[32],unsigned mods){tmc_usb_canonical_counters.keys++;hid_linux_keys(bitmap,mods);}
void tmc_usbcore_os_absolute(int x,int y,unsigned buttons,int wheel,int hwheel){tmc_usb_canonical_counters.pointers++;hid_absolute(((u64)x*(boot.w-1)+16383)/32767,((u64)y*(boot.h-1)+16383)/32767,buttons,wheel);if(hwheel)hid_horizontal(hwheel);}
void tmc_usbcore_os_pointer(int x,int y,unsigned buttons,int wheel,int hwheel){tmc_usb_canonical_counters.pointers++;hid_relative(x,y,buttons,wheel);if(hwheel)hid_horizontal(hwheel);}
int tmc_usb_canonical_start(void){int r=tmc_linux_xhci_module_init();if(r){boot_logstr("[canonical] XHCI error\n");return r;}for(unsigned c=0;c<tmc_linux_xhci_controllers();c+=2)usb_route_intel_xhci(tmc_linux_xhci_pci_address(c));r=tmc_linux_ehci_module_init();if(r){boot_logstr("[canonical] EHCI error\n");return r;}boot_logstr("[canonical] controller transports ready\n");r=tmc_usb_stack_init();boot_logstr("[canonical] stack result=");boot_lognum((u64)(i64)r);boot_logstr("\n");if(!r){tmc_usb_canonical_counters.started=1;
 /* Finish initial asynchronous discovery before presenting an interactive
  * login. Otherwise a visible form can silently lose its first keystrokes.
  * Headless/disconnected devices must not stall boot indefinitely. */
 u64 deadline=tmc_usbcore_os_clock_ms()+1500;
 do{tmc_usb_stack_poll();usb_delay_ms(1);}while(tmc_usbcore_os_clock_ms()<deadline&&(!tmc_usb_canonical_counters.bound_keys||!tmc_usb_canonical_counters.bound_pointers));
 }return r;}

#endif

#ifdef TMC_USB_CANONICAL
void tmc_usb_kernel_worker(void)
{
 for(;;){
  tmc_usb_canonical_counters.worker_polls++;
  tmc_usb_stack_poll();
  int leds=input_led_take();if(leds>=0)tmc_usb_stack_leds((unsigned)leds);
  asm volatile("sti; int $129"::"D"(1ul):"memory");
 }
}
#endif
