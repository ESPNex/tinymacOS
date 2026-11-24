/* SPDX-License-Identifier: GPL-2.0-only */
#include "../kernel/k.h"
#include "usbhost.h"
#include "linux_usb.h"
#include "linux_ehci_hcd.h"
/* Platform services + class/enumeration boundary only. All frame/QH/TD,
 * bandwidth, toggles, completion, unlink and root-hub hardware operations
 * are in byte-exact original Linux EHCI sources in ehci_runtime.c.
 * UsbInput class bindings are still the separate native-to-Linux adapter;
 * this does not purport to be complete Linux USB core/usbhid integration. */
struct EhciInput;
struct EhciPipe {struct EhciInput *device;unsigned index;};
static struct EhciInput {UsbInput usb;TmcEhciDevice *device;unsigned controller,configured;struct EhciPipe context[USB_INPUT_INTERFACES];} inputs[4][USB_INPUT_DEVICES];
static unsigned used[4];
u8 tmc_ehci_os_in8(u16 port){return inb(port);}
u16 tmc_ehci_os_in16(u16 port){return inw(port);}
u32 tmc_ehci_os_in32(u16 port){return inl(port);}
void tmc_ehci_os_out8(u16 port,u8 value){outb(port,value);}
void tmc_ehci_os_out16(u16 port,u16 value){outw(port,value);}
void tmc_ehci_os_out32(u16 port,u32 value){outl(port,value);}
u32 tmc_ehci_os_pci_next(u32 class,u32 after){return pci_find_after(class,after);}
u32 tmc_ehci_os_pci_read(u32 address,unsigned off){return pci_reg(address,off);}
void tmc_ehci_os_pci_write(u32 address,unsigned off,u32 value){u32 f=irq_save();outl(0xcf8,address|(off&252));outl(0xcfc,value);irq_restore(f);}
void tmc_ehci_os_pci_write16(u32 address,unsigned off,u16 value){u32 f=irq_save();outl(0xcf8,address|(off&252));outw(0xcfc+(off&3),value);irq_restore(f);}
void tmc_ehci_os_pci_write8(u32 address,unsigned off,u8 value){u32 f=irq_save();outl(0xcf8,address|(off&252));outb(0xcfc+(off&3),value);irq_restore(f);}
const char *tmc_ehci_os_board_name(void){return NULL;} /* DMI inventory not implemented; absence is not an invented board ID. */
void tmc_ehci_os_delay_us(unsigned us)
{
 /* Independent PIT channel 2, including pre-IRQ startup and IF-disabled reset.
  * Use the calibrated millisecond delay conservatively for sub-ms requests. */
 usb_delay_ms((us+999)/1000);
}
void *tmc_ehci_os_mmio(u32 address,unsigned bar){return pci_mmio(address,bar);}
u32 tmc_ehci_os_read32(const volatile void *p){return *(const volatile u32 *)p;}
void tmc_ehci_os_write32(volatile void *p,u32 value){*(volatile u32 *)p=value;asm volatile("":::"memory");}
u64 tmc_ehci_os_clock_ns(void){extern u64 tmc_lx_os_clock_ns(void);return tmc_lx_os_clock_ns();}
void tmc_ehci_os_fatal(void){logstr("Linux EHCI fatal invariant\n");for(;;)asm volatile("cli; hlt");}
static int linux_control(UsbInput *u,int rt,int request,int value,int index,int n)
{
 struct EhciInput *d=u->host;if(n<0||(unsigned)n>sizeof u->desc)return -1;
 tmc_linux_ehci_device_state(d->device,u->address,u->max_packet0,d->configured);
 int rc=tmc_linux_ehci_control(d->device,rt,request,value,index,u->desc,n,1000);
 u->ctrl_bytes=rc<0?0:(unsigned)rc;if(rc>=0&&rt==0&&request==9)d->configured=!!value;
 return rc==-32?-2:rc<0?-1:0;
}
static int root_control(void *context,unsigned rt,unsigned request,unsigned value,unsigned index,u8 *buffer,unsigned n)
{return tmc_linux_ehci_hub((unsigned)(u64)context,(rt<<8)|request,value,index,buffer,n);}
static void arm(struct EhciInput *,unsigned);
static void complete(void *context,int status,unsigned n)
{
 struct EhciPipe *c=context;struct EhciInput *d=c->device;UsbInputPipe *p=d->usb.pipe+c->index;p->armed=0;
 if(status){
  input_diag.transfer_errors++;
  if(p->proto==1){const u8 zero[8]={0};if(p->linux_keyboard)(void)tmc_linux_keyboard_report(p,zero,8);else hid_keys(zero);}
  if(p->linux_mouse)(void)tmc_linux_mouse_error(p,p->failures<3?status:-108);
  if(p->linux_generic)tmc_linux_generic_error(p,p->failures>=3);
  if(status!=-108&&status!=-2&&p->failures++<3){p->armed=-1;p->retry_at=ticks+100;}
  return;
 }
 p->failures=0;usb_input_report(&d->usb,c->index,n);arm(d,c->index);
}
static void arm(struct EhciInput *d,unsigned index)
{
 UsbInputPipe *p=d->usb.pipe+index;d->context[index]=(struct EhciPipe){d,index};
 int rc=tmc_linux_ehci_interrupt(d->device,p->ep,p->packet,d->usb.speed==3?(1u<<MIN((p->interval?p->interval:1)-1,15u)):(p->interval?p->interval:1),p->report,complete,d->context+index);
 p->armed=rc?0:1;if(rc)input_diag.transfer_errors++;
}
static void attach(unsigned c,unsigned speed,unsigned hub,unsigned hub_port,unsigned depth);
static void hub_children(struct EhciInput *d,unsigned depth)
{
 UsbInput *u=&d->usb;if(depth>=3||usb_input_control(u,0xa0,6,0x2900,0,u->desc,9)<7)return;
 if(u->desc[0]<7||u->desc[1]!=0x29)return;unsigned ports=MIN(u->desc[2],15u),delay=MAX(20u,(unsigned)u->desc[5]*2);
 for(unsigned p=1;p<=ports;p++)(void)usb_input_control(u,0x23,3,8,p,0,0);usb_delay_ms(delay);
 for(unsigned p=1;p<=ports&&used[d->controller]<USB_INPUT_DEVICES;p++){
  unsigned status;if(tmc_linux_hub_port_init(usb_input_control,u,p,u->speed==1?2:u->speed==2?1:3,&status))continue;
  attach(d->controller,status&1024?3:status&512?2:1,u->speed==3?u->address:u->hub,u->speed==3?p:u->hub_port,depth+1);
 }
}
static void attach(unsigned c,unsigned speed,unsigned hub,unsigned hub_port,unsigned depth)
{
 if(used[c]>=USB_INPUT_DEVICES)return;struct EhciInput *d=&inputs[c][used[c]++];d->controller=c;d->usb.speed=speed;d->usb.hub=hub;d->usb.hub_port=hub_port;
 d->device=tmc_linux_ehci_device(c,speed==2?1:speed==3?3:2);if(!d->device)return;
 if(hub){struct EhciInput *tt=NULL;for(unsigned i=0;i+1<used[c];i++)if(inputs[c][i].usb.address==hub)tt=&inputs[c][i];if(!tt||tmc_linux_ehci_translator(d->device,tt->device,hub_port,0,666))return;}
 d->usb.host=d;d->usb.control=linux_control;int kind=usb_input_enumerate(&d->usb,used[c]);
 if(kind==1){tmc_linux_ehci_device_state(d->device,d->usb.address,d->usb.max_packet0,1);for(unsigned j=0;j<d->usb.npipe;j++)arm(d,j);}
 else if(kind==2){tmc_linux_ehci_device_state(d->device,d->usb.address,d->usb.max_packet0,1);hub_children(d,depth);}
}
void ehci_init(void)
{
 int rc=tmc_linux_ehci_module_init();if(rc){logstr("Linux EHCI initialization failed\n");return;}
 for(unsigned c=0;c<tmc_linux_ehci_controllers()&&c<4;c++){
  for(unsigned port=1;port<=tmc_linux_ehci_ports(c)&&used[c]<USB_INPUT_DEVICES;port++){
   (void)tmc_linux_ehci_hub(c,0x2303,8,port,NULL,0);usb_delay_ms(20);unsigned status=0;
   if(tmc_linux_hub_port_init(root_control,(void *)(u64)c,port,3,&status))continue;
   attach(c,status&1024?3:status&512?2:1,0,0,0);
  }
  device_add("Linux EHCI host controller (polled)","usb",0);
 }
}
void ehci_poll(void)
{
 tmc_linux_ehci_poll();
 for(unsigned c=0;c<tmc_linux_ehci_controllers()&&c<4;c++)for(unsigned i=0;i<used[c];i++){
  struct EhciInput *d=&inputs[c][i];for(unsigned j=0;j<d->usb.npipe;j++){
   UsbInputPipe *p=d->usb.pipe+j;if(p->armed!=-1||ticks<p->retry_at)continue;p->armed=0;
   if(usb_input_control(&d->usb,2,1,0,0x80|p->ep,0,0)>=0){tmc_linux_ehci_reset_toggle(d->device,p->ep);arm(d,j);}
  }
 }
}
