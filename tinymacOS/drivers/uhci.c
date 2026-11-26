/* SPDX-License-Identifier: GPL-2.0-only */
#include "../kernel/k.h"
#include "usbhost.h"
#include "usb_retry.h"
#include "linux_usb.h"
#include "linux_uhci.h"
/* Platform services + class/enumeration boundary only. All frame/QH/TD,
 * bandwidth, toggles, completion, unlink and root-hub hardware operations
 * are in byte-exact original Linux UHCI sources in uhci_runtime.c.
 * UsbInput class bindings are still the separate native-to-Linux adapter;
 * this does not purport to be complete Linux USB core/usbhid integration. */
struct UhciInput;
struct UhciPipe {struct UhciInput *device;unsigned index;};
static struct UhciInput {UsbInput usb;TmcUhciDevice *device;unsigned controller,configured;struct UhciPipe context[USB_INPUT_INTERFACES];} inputs[8][USB_INPUT_DEVICES];
static unsigned used[8];
u8 tmc_uhci_os_in8(u16 port){return inb(port);}
u16 tmc_uhci_os_in16(u16 port){return inw(port);}
u32 tmc_uhci_os_in32(u16 port){return inl(port);}
void tmc_uhci_os_out8(u16 port,u8 value){outb(port,value);}
void tmc_uhci_os_out16(u16 port,u16 value){outw(port,value);}
void tmc_uhci_os_out32(u16 port,u32 value){outl(port,value);}
u32 tmc_uhci_os_pci_next(u32 class,u32 after){return pci_find_after(class,after);}
u32 tmc_uhci_os_pci_read(u32 address,unsigned off){return pci_reg(address,off);}
void tmc_uhci_os_pci_write(u32 address,unsigned off,u32 value){u32 f=irq_save();outl(0xcf8,address|(off&252));outl(0xcfc,value);irq_restore(f);}
void tmc_uhci_os_pci_write16(u32 address,unsigned off,u16 value){u32 f=irq_save();outl(0xcf8,address|(off&252));outw(0xcfc+(off&3),value);irq_restore(f);}
void tmc_uhci_os_pci_write8(u32 address,unsigned off,u8 value){u32 f=irq_save();outl(0xcf8,address|(off&252));outb(0xcfc+(off&3),value);irq_restore(f);}
const char *tmc_uhci_os_board_name(void){return NULL;} /* DMI inventory not implemented; absence is not an invented board ID. */
void tmc_uhci_os_delay_us(unsigned us)
{
 /* Independent PIT channel 2, including pre-IRQ startup and IF-disabled reset.
  * Use the calibrated millisecond delay conservatively for sub-ms requests. */
 usb_delay_ms((us+999)/1000);
}
static int linux_control(UsbInput *u,int rt,int request,int value,int index,int n)
{
 struct UhciInput *d=u->host;if(n<0||(unsigned)n>sizeof u->desc)return -1;
 tmc_linux_uhci_device_state(d->device,u->address,u->max_packet0,d->configured);
 int rc=tmc_linux_uhci_control(d->device,rt,request,value,index,u->desc,n,1000);
 u->ctrl_bytes=rc<0?0:(unsigned)rc;if(rc>=0&&rt==0&&request==9)d->configured=!!value;
 return rc==-32?-2:rc<0?-1:0;
}
static int root_control(void *context,unsigned rt,unsigned request,unsigned value,unsigned index,u8 *buffer,unsigned n)
{return tmc_linux_uhci_hub((unsigned)(u64)context,(rt<<8)|request,value,index,buffer,n);}
static void arm(struct UhciInput *,unsigned);
static void complete(void *context,int status,unsigned n)
{
 struct UhciPipe *c=context;struct UhciInput *d=c->device;UsbInputPipe *p=d->usb.pipe+c->index;p->armed=0;
 if(status){
  input_diag.transfer_errors++;
  if(p->proto==1){const u8 zero[8]={0};if(p->linux_keyboard)(void)tmc_linux_keyboard_report(p,zero,8);else hid_keys(zero);}
  int retry=usb_input_retry(p,status,ticks);
  if(p->linux_mouse)(void)tmc_linux_mouse_error(p,retry?status:-108);
  if(p->linux_generic)tmc_linux_generic_error(p,!retry);
  return;
 }
 p->failures=0;p->halted=0;usb_input_report(&d->usb,c->index,n);arm(d,c->index);
}
static void arm(struct UhciInput *d,unsigned index)
{
 UsbInputPipe *p=d->usb.pipe+index;d->context[index]=(struct UhciPipe){d,index};
 int rc=tmc_linux_uhci_interrupt(d->device,p->ep,p->packet,p->interval?p->interval:1,p->report,complete,d->context+index);
 if(rc){input_diag.transfer_errors++;usb_input_retry(p,rc,ticks);}else p->armed=1;
}
void uhci_init(void)
{
 int rc=tmc_linux_uhci_module_init();if(rc){logstr("Linux UHCI initialization failed\n");return;}
 for(unsigned c=0;c<tmc_linux_uhci_controllers()&&c<8;c++){
  for(unsigned port=1;port<=tmc_linux_uhci_ports(c)&&used[c]<USB_INPUT_DEVICES;port++){
   unsigned status=0;
   if(tmc_linux_hub_port_init(root_control,(void *)(u64)c,port,2,&status))continue;
   struct UhciInput *d=&inputs[c][used[c]++];d->controller=c;d->usb.speed=status&512?2:1;
   d->device=tmc_linux_uhci_device(c,d->usb.speed==2?1:2);if(!d->device)continue;
   d->usb.host=d;d->usb.control=linux_control;
   if(usb_input_enumerate(&d->usb,used[c])==1){
    tmc_linux_uhci_device_state(d->device,d->usb.address,d->usb.max_packet0,1);
    for(unsigned j=0;j<d->usb.npipe;j++)arm(d,j);
   }
  }
  device_add("Linux UHCI host controller (polled)","usb",0);
 }
}
void uhci_poll(void)
{
 tmc_linux_uhci_poll();
 for(unsigned c=0;c<tmc_linux_uhci_controllers()&&c<8;c++)for(unsigned i=0;i<used[c];i++){
  struct UhciInput *d=&inputs[c][i];for(unsigned j=0;j<d->usb.npipe;j++){
   UsbInputPipe *p=d->usb.pipe+j;if(p->armed!=-1||ticks<p->retry_at)continue;p->armed=0;
   if(p->halted){
    if(usb_input_control(&d->usb,2,1,0,0x80|p->ep,0,0)<0){usb_input_retry(p,-32,ticks);continue;}
    tmc_linux_uhci_reset_toggle(d->device,p->ep);p->halted=0;
   }
   arm(d,j);
  }
 }
}
