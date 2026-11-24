/* SPDX-License-Identifier: GPL-2.0-only */
#include "ehci-include/tmc_ehci_compat.h"
#include "linux_ehci_hcd.h"
#undef module_init
#undef module_exit
#define module_init(fn) int tmc_linux_ehci_core_init(void){return fn();}
#define module_exit(fn) void tmc_linux_ehci_core_exit(void){fn();}
static int tmc_ehci_handoff(struct pci_dev *,void *);
static void ehci_dispatch_hrtimers(void);
static void usb_giveback_urb_bh(struct work_struct *);
static void ehci_dispatch_bh(void);
static unsigned long usb_hcds_loaded;
#include "../../vendor/linux-v6.12/urb.c"
#include "../../vendor/linux-v6.12/ehci-hcd.c"
#include "../../vendor/linux-v6.12/hcd-bus-time.inc"
#define EHCI_CONTROLLERS 8
#define EHCI_MAPPINGS 128
static struct pci_dev *ehci_devices[EHCI_CONTROLLERS];
static unsigned ehci_device_count,ehci_poll_depth;
static struct pci_driver *registered_driver;
static struct timer_list *timers;
static TmcEhciStats ehci_stats;
#define UCOUNT(f) (++ehci_stats.f)
void ehci_log(unsigned level,const char *format,...)
{(void)format;if(level){UCOUNT(errors);tmc_lx_os_error("Linux EHCI: driver diagnostic");}}
void tmc_lx_hid_log(void *dev,unsigned level,const char *format,...)
{(void)dev;ehci_log(level,format);}
void *ehci_calloc(size_t n,size_t size,gfp_t flags)
{size_t bytes;if(__builtin_mul_overflow(n,size,&bytes))return NULL;return kzalloc(bytes,flags);}
struct kmem_cache *kmem_cache_create(const char *name,size_t size,size_t align,unsigned flags,void (*ctor)(void *))
{
 if(!name||!size||size>65536||align||flags||ctor)return NULL;
 struct kmem_cache *c=kzalloc(sizeof(*c),GFP_KERNEL);if(c)c->size=size;return c;
}
void *kmem_cache_zalloc(struct kmem_cache *c,gfp_t flags)
{if(!c)return NULL;void *p=kzalloc(c->size,flags);if(p)c->active++;return p;}
void kmem_cache_free(struct kmem_cache *c,void *p)
{if(c&&p){if(!c->active){WARN_ON(1);return;}c->active--;kfree(p);}}
void kmem_cache_destroy(struct kmem_cache *c)
{if(c){if(c->active){WARN_ON(1);return;}kfree(c);}}
/* Timers are removed from the registry on cancellation/expiry. No registry
 * retains a pointer into a freed controller. Callback rearm is supported. */
void timer_setup(struct timer_list *t,void (*fn)(struct timer_list *),unsigned flags)
{memset(t,0,sizeof(*t));t->function=fn;if(flags)WARN_ON(1);}
int del_timer(struct timer_list *t)
{
 struct timer_list **link=&timers;while(*link&&*link!=t)link=&(*link)->next;
 int old=t->pending;if(*link)*link=t->next;t->next=NULL;t->pending=0;return old;
}
int mod_timer(struct timer_list *t,unsigned long expires)
{int old=del_timer(t);t->expires=expires;t->pending=1;t->next=timers;timers=t;return old;}
void del_timer_sync(struct timer_list *t)
{del_timer(t);/* Single serialized executor; no parallel timer worker exists. */if(t->running)WARN_ON(1);}
void ehci_delay_us(unsigned us){tmc_ehci_os_delay_us(us);}
const char *dmi_get_system_info(unsigned key)
{return key==DMI_BOARD_NAME?tmc_ehci_os_board_name():NULL;}
int pci_read_config_word(struct pci_dev *d,int off,u16 *v)
{if(!v)return -EINVAL;*v=0xffff;if(!d||off<0||off>254||off&1)return -EINVAL;*v=tmc_ehci_os_pci_read(d->address,off&~3)>>(8*(off&3));return 0;}
int pci_write_config_word(struct pci_dev *d,int off,u16 v)
{
 if(!d||off<0||off>254||off&1)return -EINVAL;
 /* Width-specific writes prevent read/modify/write from acknowledging PCI
  * status or chipset W1C bits in the other half of the config dword. */
 extern void tmc_ehci_os_pci_write16(u32,unsigned,u16);
 tmc_ehci_os_pci_write16(d->address,off,v);return 0;
}
int pci_write_config_byte(struct pci_dev *d,int off,u8 v)
{if(!d||off<0||off>255)return -EINVAL;extern void tmc_ehci_os_pci_write8(u32,unsigned,u8);tmc_ehci_os_pci_write8(d->address,off,v);return 0;}
int usb_disabled(void){return 0;}
static void root_timer(struct timer_list *t)
{struct usb_hcd *h=container_of(t,struct usb_hcd,rh_timer);usb_hcd_poll_rh_status(h);}
int usb_hcd_pci_probe(struct pci_dev *p,const struct hc_driver *driver)
{
 extern void *tmc_ehci_os_mmio(u32,unsigned);
 void *regs=tmc_ehci_os_mmio(p->address,0);if(!regs)return -ENODEV;
 if(driver->hcd_priv_size>65536-sizeof(struct usb_hcd))return -EINVAL;
 struct usb_hcd *h=kzalloc(sizeof(*h)+driver->hcd_priv_size,GFP_KERNEL);if(!h)return -ENOMEM;
 struct usb_device *root=kzalloc(sizeof(*root),GFP_KERNEL);if(!root){kfree(h);return -ENOMEM;}
 h->driver=driver;h->self.bus_name="linux-ehci";h->self.controller=h->self.sysdev=&p->dev;h->self.busnum=ehci_device_count+1;
 h->self.root_hub=root;root->bus=&h->self;root->state=USB_STATE_CONFIGURED;root->speed=USB_SPEED_HIGH;root->can_submit=1;atomic_set(&root->references,1);
 h->regs=regs;h->rsrc_start=tmc_ehci_os_pci_read(p->address,0x10)&~15u;h->rsrc_len=4096;h->accessible=h->running=1;h->irq=p->irq;
 h->high_prio_bh.high_prio=true;INIT_LIST_HEAD(&h->high_prio_bh.head);INIT_LIST_HEAD(&h->low_prio_bh.head);h->high_prio_bh.bh.function=h->low_prio_bh.bh.function=usb_giveback_urb_bh;
 timer_setup(&h->rh_timer,root_timer,0);p->hcd=h;p->driver_data=h;
 u16 command;pci_read_config_word(p,PCI_COMMAND,&command);
 /* Hardware IRQ delivery is deliberately disabled at PCI while the exact
  * driver's IRQ and schedule callbacks are dispatched by the polled executor. */
 pci_write_config_word(p,PCI_COMMAND,command|0x406);
 int rc=tmc_ehci_handoff(p,regs);if(!rc)rc=driver->reset(h);if(!rc)rc=driver->start(h);
 if(rc){del_timer_sync(&h->rh_timer);p->hcd=NULL;p->driver_data=NULL;kfree(root);kfree(h);return rc;}
 root->maxchild=HCS_N_PORTS(hcd_to_ehci(h)->hcs_params);mod_timer(&h->rh_timer,jiffies);UCOUNT(controllers);return 0;
}
void usb_hcd_pci_remove(struct pci_dev *p)
{
 struct usb_hcd *h=p?p->hcd:NULL;if(!h)return;
 /* Failure to halt must never cause DMA use-after-free. Keep resources and
  * the reserved PCI slot, report failure, and do not claim clean teardown. */
 struct ehci_hcd *e=hcd_to_ehci(h);
 if(ehci_halt(e)){WARN_ON(1);return;}
 del_timer_sync(&h->rh_timer);h->running=0;h->driver->stop(h);h->accessible=0;
 kfree(h->self.root_hub);kfree(h);p->hcd=NULL;p->driver_data=NULL;
}
int pci_register_driver(struct pci_driver *driver)
{
 if(registered_driver)return -EBUSY;registered_driver=driver;u32 address=0;
 while(ehci_device_count<EHCI_CONTROLLERS&&(address=tmc_ehci_os_pci_next(PCI_CLASS_SERIAL_USB_EHCI,address))){
  struct pci_dev *p=kzalloc(sizeof(*p),GFP_KERNEL);if(!p){pci_unregister_driver(driver);return -ENOMEM;}
  p->address=address;u32 id=tmc_ehci_os_pci_read(address,0);p->vendor=id;p->device=id>>16;p->revision=tmc_ehci_os_pci_read(address,8)&255;p->irq=tmc_ehci_os_pci_read(address,0x3c)&255;
  p->dev.name="linux-ehci";p->dev.mask=0xffffffffu;INIT_LIST_HEAD(&p->dev.dma_pools);
  int rc=driver->probe(p,driver->id_table);if(rc){kfree(p);continue;}
  ehci_devices[ehci_device_count++]=p;
 }
 return 0;
}
void pci_unregister_driver(struct pci_driver *driver)
{
 if(registered_driver!=driver)return;
 for(unsigned i=0;i<ehci_device_count;i++){struct pci_dev *p=ehci_devices[i];driver->remove(p);if(p->hcd)return;kfree(p);ehci_devices[i]=NULL;}
 ehci_device_count=0;registered_driver=NULL;
}
void synchronize_irq(unsigned irq)
{for(unsigned i=0;i<ehci_device_count;i++)if(ehci_devices[i]&&ehci_devices[i]->irq==irq&&ehci_devices[i]->hcd&&ehci_devices[i]->hcd->irq_running)WARN_ON(1);}
void usb_hc_died(struct usb_hcd *h){h->running=0;UCOUNT(errors);}
void usb_hcd_start_port_resume(struct usb_bus *b,int port){if(port>0&&port<=31)set_bit(port,&b->resuming_ports);}
void usb_hcd_end_port_resume(struct usb_bus *b,int port){if(port>0&&port<=31)clear_bit(port,&b->resuming_ports);}
void usb_hcd_resume_root_hub(struct usb_hcd *h){h->root_resume=1;mod_timer(&h->rh_timer,jiffies);}
void usb_hcd_poll_rh_status(struct usb_hcd *h)
{
 if(!h||!h->accessible||h->polling)return;h->polling=1;char status[8]={0};
 int n=h->driver->hub_status_data(h,status);if(n>0){unsigned bits=0;for(int i=0;i<n&&i<4;i++)bits|=(unsigned)(u8)status[i]<<(8*i);h->root_changes|=bits;}
 h->polling=0;UCOUNT(root_polls);
 if(h->accessible)mod_timer(&h->rh_timer,jiffies+32);
}
void ehci_runtime_progress(void)
{
 tmc_lx_os_spin_wait();if(ehci_poll_depth)return;ehci_poll_depth++;
 /* Executor and all clients are serialized on CPU0; save IF to preserve an
  * enclosing USB core lock instead of enabling IRQs during callbacks. */
 u32 flags=tmc_lx_os_irq_save();
 for(unsigned i=0;i<ehci_device_count;i++){
  struct usb_hcd *h=ehci_devices[i]->hcd;if(!h||!h->accessible)continue;
  h->irq_running=1;h->driver->irq(h);h->irq_running=0;UCOUNT(irq_polls);
  UCOUNT(schedule_polls);
 }
 /* A callback can rearm/cancel other timers. Restart traversal after each
  * callback, bounded to avoid starving hardware if it rearms for 'now'. */
 for(unsigned fired=0;fired<64;fired++){
  struct timer_list *t=timers;while(t&&!time_after_eq(jiffies,t->expires))t=t->next;
  if(!t)break;del_timer(t);t->running=1;UCOUNT(timer_fires);t->function(t);t->running=0;
 }
 ehci_dispatch_hrtimers();
 tmc_lx_os_irq_restore(flags);ehci_poll_depth--;ehci_dispatch_bh();ehci_dispatch_work();
}
void tmc_linux_ehci_poll(void){ehci_runtime_progress();}
unsigned tmc_linux_ehci_controllers(void){return ehci_device_count;}
unsigned tmc_linux_ehci_ports(unsigned c){return c<ehci_device_count?ehci_devices[c]->hcd->self.root_hub->maxchild:0;}
int tmc_linux_ehci_hub(unsigned c,u16 request,u16 value,u16 index,void *buffer,unsigned n)
{
 if(c>=ehci_device_count||n>65535||(n&&!buffer))return -EINVAL;
 if(((request==GetPortStatus||request==GetHubStatus)&&n<4)||(request==GetHubDescriptor&&n<9))return -EINVAL;
 struct usb_hcd *h=ehci_devices[c]->hcd;u8 scratch[32]={0};
 int rc=h->driver->hub_control(h,request,value,index,(char *)scratch,n);if(rc<0)return rc;
 unsigned bytes=request==GetHubDescriptor?scratch[0]:(request==GetPortStatus||request==GetHubStatus)?4:0;
 if(bytes>sizeof scratch)return -EOVERFLOW;bytes=min(bytes,n);if(bytes)memcpy(buffer,scratch,bytes);return bytes;
}
void tmc_linux_ehci_stats(TmcEhciStats *out){if(out)*out=ehci_stats;}

/* Checked per-URB bounce mappings. DMA bus addresses are never CPU casts.
 * Records retain original CPU buffers; payload copies happen only after the
 * original HCD has retired descriptors and before driver completion. */
struct EhciMap {struct urb *urb;void *data,*setup;dma_addr_t data_dma,setup_dma;unsigned length;};
static struct EhciMap mappings[EHCI_MAPPINGS];
static struct EhciMap *find_map(struct urb *u)
{for(unsigned i=0;i<EHCI_MAPPINGS;i++)if(mappings[i].urb==u)return &mappings[i];return NULL;}
static void unmap_urb_for_dma(struct usb_hcd *h,struct urb *u)
{
 struct EhciMap *m=find_map(u);if(!m)return;
 if(m->data){if(usb_pipein(u->pipe)&&u->actual_length>0){unsigned n=min_t(unsigned,u->actual_length,m->length);memcpy(u->transfer_buffer,m->data,n);}dma_free_coherent(h->self.controller,m->length,m->data,m->data_dma);}
 if(m->setup)dma_free_coherent(h->self.controller,8,m->setup,m->setup_dma);
 u->transfer_dma=u->setup_dma=0;u->transfer_flags&=~(URB_DMA_MAP_SINGLE|URB_SETUP_MAP_SINGLE);memset(m,0,sizeof(*m));
}
static int map_urb_for_dma(struct usb_hcd *h,struct urb *u,gfp_t flags)
{
 /* Streaming SG and externally pre-mapped buffers are not exposed by this
  * runtime yet. The original EHCI SG scheduler remains unmodified. */
 if(u->sg||u->num_sgs||(u->transfer_flags&URB_NO_TRANSFER_DMA_MAP))return -EOPNOTSUPP;
 if(u->transfer_buffer_length<0||u->transfer_buffer_length>65536||(u->transfer_buffer_length&&!u->transfer_buffer)||find_map(u))return -EINVAL;
 struct EhciMap *m=NULL;for(unsigned i=0;i<EHCI_MAPPINGS;i++)if(!mappings[i].urb){m=&mappings[i];break;}if(!m)return -ENOSPC;
 m->urb=u;m->length=u->transfer_buffer_length;
 if(usb_pipetype(u->pipe)==PIPE_CONTROL){m->setup=dma_alloc_coherent(h->self.controller,8,&m->setup_dma,flags);if(!m->setup)goto fail;memcpy(m->setup,u->setup_packet,8);u->setup_dma=m->setup_dma;u->transfer_flags|=URB_SETUP_MAP_SINGLE;}
 if(m->length){m->data=dma_alloc_coherent(h->self.controller,m->length,&m->data_dma,flags);if(!m->data)goto fail;if(!usb_pipein(u->pipe))memcpy(m->data,u->transfer_buffer,m->length);u->transfer_dma=m->data_dma;u->transfer_flags|=URB_DMA_MAP_SINGLE;}
 return 0;
fail:UCOUNT(map_failures);unmap_urb_for_dma(h,u);return -ENOMEM;
}
static spinlock_t hcd_urb_list_lock,hcd_urb_unlink_lock;
#include "../../vendor/linux-v6.12/hcd-endpoint.inc"
static struct usb_device *usb_get_dev(struct usb_device *d){if(d)atomic_inc(&d->references);return d;}
static void usb_put_dev(struct usb_device *d){if(d)atomic_dec(&d->references);}
/* Root-hub descriptors/enumeration are not a fake-success emulation of the
 * missing USB bus core. The public hub bridge calls original hub_control. */
static int rh_urb_enqueue(struct usb_hcd *h,struct urb *u){(void)h;(void)u;return -EOPNOTSUPP;}
static int usb_rh_urb_dequeue(struct usb_hcd *h,struct urb *u,int status){(void)h;(void)u;(void)status;return -EOPNOTSUPP;}
#define usbmon_urb_submit(b,u) UCOUNT(submissions)
#define usbmon_urb_submit_error(b,u,s) do{}while(0)
#define usbmon_urb_complete(b,u,s) UCOUNT(givebacks)
#define USB_LED_EVENT_HOST 0
#define usb_led_activity(e) do{}while(0)
#define kcov_remote_start_usb_softirq(id) 0ul
#define kcov_remote_stop_softirq(f) do{(void)(f);}while(0)
#include "../../vendor/linux-v6.12/hcd-lifecycle.inc"
#include "../../vendor/linux-v6.12/hcd-giveback-bh.inc"
wait_queue_head_t usb_kill_urb_queue;

struct EhciInterrupt {struct urb *urb;TmcEhciComplete complete;void *context;unsigned callback_depth;};
struct TmcEhciDevice {struct usb_device core;struct usb_hub hub;struct usb_host_endpoint ep0,eps[30];struct EhciInterrupt interrupt[30];};
TmcEhciDevice *tmc_linux_ehci_device(unsigned controller,unsigned speed)
{
 if(controller>=ehci_device_count||(speed!=USB_SPEED_LOW&&speed!=USB_SPEED_FULL&&speed!=USB_SPEED_HIGH))return NULL;
 TmcEhciDevice *d=kzalloc(sizeof(*d),GFP_KERNEL);if(!d)return NULL;
 d->core.bus=&ehci_devices[controller]->hcd->self;d->core.speed=speed;d->core.state=USB_STATE_DEFAULT;d->core.can_submit=1;atomic_set(&d->core.references,1);
 d->ep0.enabled=1;d->ep0.desc=(struct usb_endpoint_descriptor){7,5,0,0,8,0};INIT_LIST_HEAD(&d->ep0.urb_list);d->core.ep_in[0]=d->core.ep_out[0]=&d->ep0;return d;
}
void tmc_linux_ehci_device_state(TmcEhciDevice *d,unsigned address,unsigned maxpacket,unsigned configured)
{
 if(!d||address>127||(maxpacket!=8&&maxpacket!=16&&maxpacket!=32&&maxpacket!=64)||!list_empty(&d->ep0.urb_list))return;
 /* The original hub core performs usb_ep0_reinit after SET_ADDRESS and an
  * EP0 maxpacket change. Retire the original EHCI's cached QH rather than
  * patching a live QH overlay or continuing to address the device at zero. */
 if(d->ep0.hcpriv&&(d->core.devnum!=(int)address||d->ep0.desc.wMaxPacketSize!=maxpacket))bus_to_hcd(d->core.bus)->driver->endpoint_disable(bus_to_hcd(d->core.bus),&d->ep0);
 d->core.devnum=d->core.devaddr=address;d->ep0.desc.wMaxPacketSize=maxpacket;d->core.state=configured?USB_STATE_CONFIGURED:address?USB_STATE_ADDRESS:USB_STATE_DEFAULT;
}

struct EhciControl {volatile unsigned done;int status;unsigned actual;};
static void control_complete(struct urb *u)
{struct EhciControl *c=u->context;c->status=u->status;c->actual=u->actual_length;__atomic_store_n(&c->done,1,__ATOMIC_RELEASE);}
int tmc_linux_ehci_control(TmcEhciDevice *d,unsigned rt,unsigned request,unsigned value,unsigned index,void *data,unsigned n,unsigned timeout)
{
 if(!d||rt>255||request>255||value>65535||index>65535||n>65535||(n&&!data)||!timeout||timeout>10000)return -EINVAL;
 if(!list_empty(&d->ep0.urb_list))return -EBUSY;
 struct urb *u=usb_alloc_urb(0,GFP_KERNEL);if(!u)return -ENOMEM;
 struct usb_ctrlrequest setup={(u8)rt,(u8)request,(u16)value,(u16)index,(u16)n};struct EhciControl result={0};
 u->dev=&d->core;u->pipe=(PIPE_CONTROL<<30)|(d->core.devnum<<8)|(rt&128);u->setup_packet=(u8 *)&setup;u->transfer_buffer=data;u->transfer_buffer_length=n;u->complete=control_complete;u->context=&result;
 int rc=usb_submit_urb(u,GFP_KERNEL);if(!rc){u64 start=tmc_lx_os_clock_ms();while(!__atomic_load_n(&result.done,__ATOMIC_ACQUIRE)&&tmc_lx_os_clock_ms()-start<timeout)ehci_runtime_progress();if(!result.done){UCOUNT(unlinks);usb_kill_urb(u);rc=-ETIMEDOUT;}else rc=result.status?result.status:(int)result.actual;}
 usb_free_urb(u);return rc;
}
static void interrupt_complete(struct urb *u)
{struct EhciInterrupt *p=u->context;p->callback_depth++;p->complete(p->context,u->status,u->actual_length);p->callback_depth--;}
int tmc_linux_ehci_interrupt_report(TmcEhciDevice *d,unsigned ep,unsigned packet,unsigned interval,unsigned length,void *data,TmcEhciComplete complete,void *context)
{
 if(!d||!(ep&15)||(ep&~0x10f)||length>packet||!packet||packet>(d->core.speed==USB_SPEED_LOW?8:d->core.speed==USB_SPEED_HIGH?1024:64)||!interval||interval>32768||!data||!complete)return -EINVAL;
 struct EhciInterrupt *p=&d->interrupt[(ep&15)-1+((ep&0x100)?15:0)];struct usb_host_endpoint *e=&d->eps[(ep&15)-1+((ep&0x100)?15:0)];
 if(!p->urb){p->urb=usb_alloc_urb(0,GFP_KERNEL);if(!p->urb)return -ENOMEM;e->enabled=1;e->desc=(struct usb_endpoint_descriptor){7,5,((ep&0x100)?0:0x80)|(ep&15),3,packet,d->core.speed==USB_SPEED_HIGH?ilog2(interval)+1:interval};INIT_LIST_HEAD(&e->urb_list);(ep&0x100?d->core.ep_out:d->core.ep_in)[ep&15]=e;}
 else if(!list_empty(&e->urb_list))return -EBUSY;
 p->complete=complete;p->context=context;struct urb *u=p->urb;u->dev=&d->core;u->pipe=(PIPE_INTERRUPT<<30)|(d->core.devnum<<8)|((ep&15)<<15)|((ep&0x100)?0:128);u->interval=interval;u->transfer_buffer=data;u->transfer_buffer_length=length;u->context=p;u->complete=interrupt_complete;return usb_submit_urb(u,GFP_ATOMIC);
}
int tmc_linux_ehci_interrupt_stop(TmcEhciDevice *d,unsigned ep)
{
 if(!d||!(ep&15)||(ep&~0x10f))return -EINVAL;struct EhciInterrupt *p=&d->interrupt[(ep&15)-1+((ep&0x100)?15:0)];if(p->callback_depth)return -EBUSY;
 if(p->urb){if(atomic_read(&p->urb->use_count))UCOUNT(unlinks);usb_kill_urb(p->urb);usb_free_urb(p->urb);p->urb=NULL;}return 0;
}
int tmc_linux_ehci_interrupt(TmcEhciDevice *d,unsigned ep,unsigned packet,unsigned interval,void *data,TmcEhciComplete complete,void *context)
{return tmc_linux_ehci_interrupt_report(d,ep,packet,interval,packet,data,complete,context);}
int tmc_linux_ehci_device_destroy(TmcEhciDevice *d)
{
 if(!d)return -EINVAL;if(atomic_read(&d->core.references)!=1)return -EBUSY;for(unsigned i=0;i<30;i++)if(d->interrupt[i].callback_depth)return -EBUSY;
 if(!list_empty(&d->ep0.urb_list))return -EBUSY;struct usb_hcd *h=bus_to_hcd(d->core.bus);d->core.can_submit=0;
 for(unsigned i=0;i<30;i++){tmc_linux_ehci_interrupt_stop(d,(i%15+1)|(i>=15?0x100:0));if(d->eps[i].hcpriv)h->driver->endpoint_disable(h,&d->eps[i]);}
 h->driver->endpoint_disable(h,&d->ep0);if(h->driver->free_dev)h->driver->free_dev(h,&d->core);if(atomic_read(&d->core.references)!=1)return -EBUSY;if(d->core.tt)usb_put_dev(d->core.tt->hub);kfree(d);return 0;
}

void tmc_linux_ehci_reset_toggle(TmcEhciDevice *d,unsigned ep)
{if(d&&(ep&15)&&!(ep&~0x10f)&&list_empty(&d->eps[(ep&15)-1+((ep&0x100)?15:0)].urb_list))bus_to_hcd(d->core.bus)->driver->endpoint_reset(bus_to_hcd(d->core.bus),&d->eps[(ep&15)-1+((ep&0x100)?15:0)]);}

#include "ehci_services.inc"
