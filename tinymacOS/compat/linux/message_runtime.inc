/* SPDX-License-Identifier: GPL-2.0-only */
/* Real original synchronous control message functions. The native runtime is
 * polled, NOT a Linux sleepable scheduler. Completion waiting drives the real
 * queued EP0 worker; callbacks are bounded synchronous native operations.
 * A late callback is completed as timeout without copying its stale response.
 * This cannot preempt a callback or port the rest of message.c/device model. */
typedef u8 __u8;
typedef u16 __u16;
#define GFP_NOIO GFP_KERNEL
#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif
#ifndef ENODATA
#define ENODATA 61
#endif
#define MAX_SCHEDULE_TIMEOUT LONG_MAX
#define USB_QUIRK_DELAY_CTRL_MSG (1UL<<13)
#define USB_REQ_GET_DESCRIPTOR 6
#define USB_CTRL_GET_TIMEOUT 5000
static unsigned message_gate;
static struct {u64 requests,completed,errors,rejected,detached,waits,timeouts;} tmc_linux_message_counters;
#define MSTAT(f) __atomic_add_fetch(&tmc_linux_message_counters.f,1,__ATOMIC_RELAXED)
struct completion {unsigned done;};
static void init_completion(struct completion *c){__atomic_store_n(&c->done,0,__ATOMIC_RELEASE);}
static void complete(struct completion *c){__atomic_add_fetch(&c->done,1,__ATOMIC_RELEASE);}
static unsigned long wait_for_completion_timeout(struct completion *c,unsigned long timeout)
{
 u64 start=tmc_lx_os_clock_ms();MSTAT(waits);
 for(;;){
  if(__atomic_load_n(&c->done,__ATOMIC_ACQUIRE)){__atomic_sub_fetch(&c->done,1,__ATOMIC_ACQ_REL);u64 elapsed=tmc_lx_os_clock_ms()-start;return elapsed<timeout?timeout-elapsed:1;}
  if(tmc_lx_os_clock_ms()-start>=timeout){MSTAT(timeouts);return 0;}
  tmc_linux_usb_service();tmc_lx_os_spin_wait();
 }
}
static int tmc_message_submit(struct urb *u,gfp_t flags)
{u32 irq=tmc_lx_os_lock();int result=usb_submit_urb(u,flags);tmc_lx_os_unlock(irq);return result;}
static void tmc_message_kill(struct urb *u)
{u32 irq=tmc_lx_os_lock();usb_kill_urb(u);tmc_lx_os_unlock(irq);}
static void *tmc_message_memdup(const void *data,size_t n,gfp_t flags)
{if(!data)return 0;void *p=tmc_lx_zalloc(n,flags);if(p)memcpy(p,data,n);return p;}
static void tmc_message_delay(unsigned n)
{u64 start=tmc_lx_os_clock_ms();while(tmc_lx_os_clock_ms()-start<n)tmc_lx_os_spin_wait();}
static unsigned usb_rcvctrlpipe(struct usb_device *d,unsigned ep){return usb_sndctrlpipe(d,ep)|USB_DIR_IN;}
#define usb_submit_urb tmc_message_submit
#define usb_kill_urb tmc_message_kill
#define kmemdup tmc_message_memdup
#define msleep tmc_message_delay
#include "../../vendor/linux-v6.12/message-control.inc"
#include "../../vendor/linux-v6.12/message-descriptor.inc"
#undef usb_submit_urb
#undef usb_kill_urb
#undef kmemdup
#undef msleep
int tmc_linux_core_control_busy(void)
{u32 irq=tmc_lx_os_lock();int busy=service_busy||__atomic_load_n(&message_gate,__ATOMIC_ACQUIRE);tmc_lx_os_unlock(irq);return busy;}
int tmc_linux_core_control_detach(void *key)
{
 u32 irq=tmc_lx_os_lock();struct LxBinding *b=binding(key);int result=0;
 if(b){
  if(b->kind!=3)result=-EINVAL;
  else if(b->message_busy||b->servicing||b->active_control)result=-EBUSY;
  else{memset(b,0,sizeof *b);MSTAT(detached);}
 }
 tmc_lx_os_unlock(irq);return result;
}
int tmc_linux_core_control(void *key,unsigned speed,unsigned ep0,TmcLinuxHidControl transport,void *context,unsigned rt,unsigned request,unsigned value,unsigned index,u8 *data,unsigned n,unsigned timeout)
{
 if(!key||!transport||rt>255||request>255||value>65535||index>65535||n>1024||(n&&!data)||!timeout||timeout>10000||
    (speed!=1&&speed!=2&&speed!=3&&speed!=5&&speed!=6)||
    (ep0!=8&&ep0!=16&&ep0!=32&&ep0!=64&&ep0!=512)||(speed==1&&ep0!=8)||(speed<5&&ep0==512)){
  MSTAT(rejected);return -EINVAL;
 }
 if(__atomic_exchange_n(&message_gate,1,__ATOMIC_ACQUIRE)){MSTAT(rejected);return -EBUSY;}
 int result=-EBUSY;u32 irq=tmc_lx_os_lock();struct LxBinding *b=binding(key);
 if(service_busy)goto unlock;
 if(b&&(b->kind!=3||b->message_busy||b->active_control||b->servicing||b->message_control!=transport||b->control_context!=context))goto unlock;
 if(!b){
  for(unsigned i=0;i<LX_BINDINGS;i++)if(!bindings[i].key){b=bindings+i;break;}
  if(!b){result=-ENOSPC;goto unlock;}
  memset(b,0,sizeof *b);b->key=key;b->kind=3;b->generation=++generation;b->hcd.self.bus_name="native-ep0";
  b->usb=(struct usb_device){.bus=&b->hcd.self,.owner=b,.devpath="control",.maxpacket=ep0};core_endpoints(b);
  b->usb.state=USB_STATE_DEFAULT; /* actual initial/default state, not a fabricated configuration */
  b->message_control=transport;b->control_context=context;b->opened=1;
 }
 b->usb.speed=speed;b->ep0.desc.wMaxPacketSize=ep0;b->message_busy=1;b->deadline=tmc_lx_os_clock_ms()+timeout;
 tmc_lx_os_unlock(irq);MSTAT(requests);
 if(rt==0x80&&request==6&&!index&&n&&((value>>8)==1||(value>>8)==2||(value>>8)==15))
  result=usb_get_descriptor(&b->usb,value>>8,value&255,data,n);
 else result=usb_control_msg(&b->usb,(rt&128)?usb_rcvctrlpipe(&b->usb,0):usb_sndctrlpipe(&b->usb,0),request,rt,value,index,data,n,timeout);
 irq=tmc_lx_os_lock();b->message_busy=0;
 if(result>=0){
  if(!rt&&request==5)b->usb.state=USB_STATE_ADDRESS;
  if(!rt&&request==9)b->usb.state=value?USB_STATE_CONFIGURED:USB_STATE_ADDRESS;
  MSTAT(completed);
 }else MSTAT(errors);
unlock:
 tmc_lx_os_unlock(irq);__atomic_store_n(&message_gate,0,__ATOMIC_RELEASE);return result;
}
