/* Actual native xHCI/EHCI/UHCI adapter callbacks with a faulting transport.
 * No hardware certification: specifically reproduces permanent HID stop after
 * four transient completions, failed initial submit and failed halt recovery. */
#include <assert.h>
#include <stdio.h>
#ifdef TMC_USB_RETRY_NATIVE_DRIVER
#include TMC_USB_RETRY_NATIVE_DRIVER
#endif
#if HOST_KIND==1
#ifndef TMC_USB_RETRY_NATIVE_DRIVER
#include "../drivers/xhci.c"
#endif
#define DEV XhciInput
#define CTX XhciPipe
#define POLL xhci_controller_poll
#define INT tmc_linux_xhci_interrupt
#define NC tmc_linux_xhci_controllers
#define HP tmc_linux_xhci_poll
#define RESET tmc_linux_xhci_reset_toggle
#elif HOST_KIND==2
#ifndef TMC_USB_RETRY_NATIVE_DRIVER
#include "../drivers/ehci.c"
#endif
#define DEV EhciInput
#define CTX EhciPipe
#define POLL ehci_poll
#define INT tmc_linux_ehci_interrupt
#define NC tmc_linux_ehci_controllers
#define HP tmc_linux_ehci_poll
#define RESET tmc_linux_ehci_reset_toggle
#else
#ifndef TMC_USB_RETRY_NATIVE_DRIVER
#include "../drivers/uhci.c"
#endif
#define DEV UhciInput
#define CTX UhciPipe
#define POLL uhci_poll
#define INT tmc_linux_uhci_interrupt
#define NC tmc_linux_uhci_controllers
#define HP tmc_linux_uhci_poll
#define RESET tmc_linux_uhci_reset_toggle
#endif
u64 ticks;
InputDiag input_diag;
static int submit_error,clear_error,submits,clears,resets,reports,fatal_notifications;
int tmc_linux_mouse_error(void*k,int status){(void)k;if(status==-108)fatal_notifications++;return 1;}
void tmc_linux_generic_error(void*k,int fatal){(void)k;if(fatal)fatal_notifications++;}
int tmc_linux_keyboard_report(void*k,const u8*r,unsigned n){(void)k;(void)r;(void)n;return 1;}
void hid_keys(const u8*r){(void)r;}
void usb_input_report(UsbInput*u,unsigned i,unsigned n){(void)u;(void)i;assert(n==3);reports++;}
int usb_input_control(void*x,unsigned rt,unsigned rq,unsigned v,unsigned i,u8*b,unsigned n){(void)x;(void)v;(void)i;(void)b;assert(rt==2&&rq==1&&!n);clears++;return clear_error;}
unsigned NC(void){return 1;}
void HP(void){}
#if HOST_KIND==1
int tmc_linux_xhci_interrupt_companion(TmcXhciDevice*d,unsigned e,unsigned b,unsigned m,unsigned n){(void)d;(void)e;(void)b;(void)m;(void)n;return 0;}
int INT(TmcXhciDevice*d,unsigned e,unsigned p,unsigned i,void*b,TmcXhciComplete cb,void*c)
#elif HOST_KIND==2
int INT(TmcEhciDevice*d,unsigned e,unsigned p,unsigned i,void*b,TmcEhciComplete cb,void*c)
#else
int INT(TmcUhciDevice*d,unsigned e,unsigned p,unsigned i,void*b,TmcUhciComplete cb,void*c)
#endif
{(void)d;(void)e;(void)p;(void)i;(void)b;(void)cb;(void)c;submits++;return submit_error;}
#if HOST_KIND==1
void RESET(TmcXhciDevice*d,unsigned e)
#elif HOST_KIND==2
void RESET(TmcEhciDevice*d,unsigned e)
#else
void RESET(TmcUhciDevice*d,unsigned e)
#endif
{(void)d;(void)e;resets++;}
int main(void){
 struct DEV*d=&inputs[0][0];used[0]=1;d->usb.speed=1;d->usb.npipe=2;d->context[0]=(struct CTX){d,0};
 UsbInputPipe*p=d->usb.pipe;p->ep=1;p->packet=8;p->interval=10;p->proto=2;p->linux_mouse=1;p->linux_generic=2;
 UsbInputPipe*keyboard=d->usb.pipe+1;keyboard->ep=2;keyboard->packet=8;keyboard->interval=10;keyboard->proto=1;arm(d,1);assert(keyboard->armed==1);
 arm(d,0);assert(p->armed==1);
 for(unsigned i=0;i<12;i++){
  fprintf(stderr,"transient completion %u\n",i+1);complete(d->context,-5,0);assert(p->armed==-1&&keyboard->armed==1);assert(!fatal_notifications);assert(p->retry_at-ticks>=100&&p->retry_at-ticks<=800&&p->failures<=10);
  int prev=submits;POLL();assert(submits==prev);ticks=p->retry_at;POLL();assert(submits==prev+1&&p->armed==1);
 }
 assert(!clears);complete(d->context,0,3);assert(reports==1&&p->armed==1&&!p->failures);
 puts("PASS >4 transient completion errors recover on actual native adapter, no fake shutdown or motion; scheduled polling backoff");
 submit_error=-12;complete(d->context,0,3);assert(p->armed==-1);submit_error=0;ticks=p->retry_at;POLL();assert(p->armed==1);
 puts("PASS transient submit allocation failure is retried, not permanent unarmed pointer");
 complete(d->context,-32,0);assert(p->armed==-1);clear_error=-1;ticks=p->retry_at;int before=submits;POLL();assert(p->armed==-1&&submits==before);clear_error=0;ticks=p->retry_at;POLL();assert(p->armed==1&&clears==2&&resets==1);
 complete(d->context,0,3);assert(reports==3);
 puts("PASS failed CLEAR_HALT remains retryable; successful clear/reset rearms and real motion resumes");
 const int terminal[]={-2,-19,-104,-108,-22};
 for(unsigned i=0;i<sizeof terminal/sizeof*terminal;i++){
  complete(d->context,terminal[i],0);assert(!p->armed);ticks+=100000;int n=submits;POLL();assert(submits==n);arm(d,0);
 }
 assert(fatal_notifications==10);puts("PASS disconnect/shutdown/cancellation/invalid endpoint do not auto-resubmit");
}
