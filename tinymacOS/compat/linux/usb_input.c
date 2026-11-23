/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "linux_usb.h"
#include "linux/tmc_compat.h"
/* One CPU0-polled native input pipe supplies each logical interrupt URB.
 * Serialized entry points share the native IRQ-safe lock. There is no claim
 * that these interface/URB objects implement all of Linux USB core or HCDs.
 */
#define LX_BINDINGS 128
struct LxBinding {
 void *key;struct usb_hcd hcd;struct usb_device usb;struct usb_interface intf;
 struct usb_host_interface alt;struct usb_host_endpoint endpoint,ep0;
 struct input_dev *input;struct urb *urb,*control_urb,*active_input,*active_control;int opened,kind,servicing;
 TmcLinuxUsbControl control;void *control_context;u64 generation;
};
static struct LxBinding bindings[LX_BINDINGS];
static struct input_dev *external_pointers[16],*external_keyboards[16];
static int external_keyboard(struct input_dev *d){for(unsigned i=0;i<COUNT(external_keyboards);i++)if(external_keyboards[i]==d)return 1;return 0;}
static TmcLinuxUsbStats tmc_linux_usb_stats;
static TmcLinuxKeyboardStats tmc_linux_kbd_stats;
static u64 generation;
static unsigned service_busy;
wait_queue_head_t usb_kill_urb_queue;
static TmcLinuxUrbStats tmc_linux_urb_counters;
static struct {u64 links,unlinks,queued,givebacks,map_checks,map_rejected,root_rejected;} tmc_linux_hcd_counters;
#define HCOUNT(f) __atomic_add_fetch(&tmc_linux_hcd_counters.f,1,__ATOMIC_RELAXED)
#define ustats tmc_linux_urb_counters
#define kstats tmc_linux_kbd_stats
#define stats tmc_linux_usb_stats
void *tmc_lx_zalloc(size_t n,gfp_t flags)
{
 if(!n||n>65536||(flags!=GFP_KERNEL&&flags!=GFP_ATOMIC))return 0;
 void *p=tmc_lx_os_alloc(n);if(p){memset(p,0,n);stats.allocations++;}return p;
}
void tmc_lx_free(void *p){if(p){stats.frees++;tmc_lx_os_free(p);}}
long tmc_lx_strscpy(char *d,const char *s,size_t n)
{
 if(!n)return -7;size_t i=0;while(i+1<n&&s[i]){d[i]=s[i];i++;}d[i]=0;return s[i]?-7:(long)i;
}
size_t tmc_lx_strlcat(char *d,const char *s,size_t n)
{
 size_t a=0,b=strlen(s);while(a<n&&d[a])a++;if(a==n)return n+b;
 size_t i=0;while(i<b&&a+i+1<n){d[a+i]=s[i];i++;}d[a+i]=0;return a+b;
}
static void charout(char *out,size_t n,size_t *at,char c){if(*at+1<n)out[*at]=c;(*at)++;}
/* POSIX snprintf return/termination rules for the formats used by this driver.
 * Unsupported conversion is reported as an error, never silently succeeds. */
static int tmc_lx_vsnprintf(char *out,size_t n,const char *fmt,va_list ap)
{
 size_t at=0;int error=0;
 for(;*fmt;fmt++){
  if(*fmt!='%'){charout(out,n,&at,*fmt);continue;}
  fmt++;int alternate=*fmt=='#';if(alternate)fmt++;char pad=' ';unsigned width=0;if(*fmt=='0'){pad='0';fmt++;}
  while(*fmt>='0'&&*fmt<='9'){width=width*10+*fmt-'0';fmt++;if(width>128){error=1;break;}}
  if(error)break;
  if(*fmt=='%'){charout(out,n,&at,'%');continue;}
  if(*fmt=='s'){const char *s=va_arg(ap,const char *);if(!s)s="(null)";while(*s)charout(out,n,&at,*s++);continue;}
  if(*fmt=='d'||*fmt=='u'||*fmt=='x'){
   i64 v=*fmt=='d'?va_arg(ap,int):(i64)va_arg(ap,unsigned);u64 u=v<0?(u64)-v:(u64)v;char digits[32];unsigned len=0,base=*fmt=='x'?16:10;
   do{digits[len++]="0123456789abcdef"[u%base];u/=base;}while(u);
   if(v<0){charout(out,n,&at,'-');if(width)width--;}
   if(alternate&&*fmt=='x'&&v){charout(out,n,&at,'0');charout(out,n,&at,'x');width=width>2?width-2:0;}
   while(width>len){charout(out,n,&at,pad);width--;}
   while(len)charout(out,n,&at,digits[--len]);continue;
  }
  error=1;break;
 }
 if(n)out[at<n?at:n-1]=0;return error?-EINVAL:(int)at;
}
int tmc_lx_snprintf(char *out,size_t n,const char *fmt,...)
{
 va_list ap;va_start(ap,fmt);int result=tmc_lx_vsnprintf(out,n,fmt,ap);va_end(ap);return result;
}
void tmc_lx_dev_err(struct device *d,const char *fmt,...)
{
 (void)d;(void)fmt;stats.errors++;tmc_lx_os_error("Linux usbmouse: interrupt URB resubmission failed");
}
struct input_dev *input_allocate_device(void){return tmc_lx_zalloc(sizeof(struct input_dev),GFP_KERNEL);}
void input_free_device(struct input_dev *d){tmc_lx_free(d);}
int input_register_device(struct input_dev *d)
{
 if(!d||d->registered||!d->open||!d->close)return -EINVAL;
 for(unsigned i=0;i<LX_BINDINGS;i++)if(bindings[i].key&&d->dev.parent==&bindings[i].intf.dev){
  if(bindings[i].input)return -EBUSY;
  bindings[i].input=d;d->owner=&bindings[i];d->registered=1;return 0;
 }
 return -ENODEV;
}
void input_unregister_device(struct input_dev *d)
{
 if(!d)return;
 struct LxBinding *b=d->owner;
 if(b&&b->opened){d->close(d);b->opened=0;}
 /* Linux input unregister releases held keys. Do not leave the compositor
  * dragging after a pipe is torn down; pending motion/wheel is not replayed. */
 if(d->registered&&b&&b->kind==1){memset(d->key,0,sizeof d->key);input_sync(d);}
 else if(d->registered&&d->buttons){d->buttons=d->x=d->y=d->wheel=0;input_sync(d);}
 if(b)b->input=0;d->registered=0;input_free_device(d);
}
void input_report_key(struct input_dev *d,unsigned code,int on)
{
 if(!d||!d->registered)return;
 struct LxBinding *b=d->owner;
 if((b&&b->kind==1)||external_keyboard(d)){
  if(code>=256||!test_bit(code,d->keybit))return;
  if(on)set_bit(code,d->key);else clear_bit(code,d->key);
  return;
 }
 if(code<BTN_LEFT||code>BTN_EXTRA)return;
 unsigned bit=1u<<(code-BTN_LEFT);if(on)d->buttons|=bit;else d->buttons&=~bit;
}
void input_report_rel(struct input_dev *d,unsigned axis,int delta)
{
 if(!d||!d->registered)return;
 if(axis==REL_X)d->x=delta;else if(axis==REL_Y)d->y=delta;else if(axis==REL_WHEEL)d->wheel=delta;else if(axis==REL_HWHEEL)d->hwheel=delta;
}
static unsigned held_buttons(void)
{
 unsigned buttons=0;
 for(unsigned i=0;i<LX_BINDINGS;i++){
  struct input_dev *in=bindings[i].input;
  if(bindings[i].kind==2&&in&&in->registered)buttons|=(unsigned)in->buttons;
 }
 for(unsigned i=0;i<COUNT(external_pointers);i++)if(external_pointers[i]&&external_pointers[i]->registered)buttons|=(unsigned)external_pointers[i]->buttons;
 return buttons;
}
void input_sync(struct input_dev *d)
{
 if(!d||!d->registered)return;
 struct LxBinding *owner=d->owner;
 if((owner&&owner->kind==1)||external_keyboard(d)){
  u8 bitmap[32]={0};unsigned modifiers=0;
  for(unsigned i=0;i<LX_BINDINGS;i++){
   struct input_dev *in=bindings[i].input;
   if(bindings[i].kind!=1||!in||!in->registered)continue;
   for(unsigned code=1;code<256;code++)if(test_bit(code,in->key)){
    unsigned usage=tmc_lx_usbkbd_usage(code);
    if(usage>=224&&usage<232)modifiers|=1u<<(usage-224);
    else if(usage>=4&&usage<224)bitmap[usage/8]|=1u<<(usage%8);
   }
  }
  for(unsigned i=0;i<COUNT(external_keyboards);i++){
   struct input_dev *in=external_keyboards[i];if(!in||!in->registered)continue;
   for(unsigned code=1;code<256;code++)if(test_bit(code,in->key)){
    unsigned usage=tmc_lx_usbkbd_usage(code);
    if(usage>=224&&usage<232)modifiers|=1u<<(usage-224);
    else if(usage>=4&&usage<224)bitmap[usage/8]|=1u<<(usage%8);
   }
  }
  tmc_lx_os_keyboard(bitmap,modifiers);if(owner)kstats.frames++;return;
 }
 /* Linux input devices have independent button state. Merge only held
  * buttons, never another device's already-consumed relative motion/wheel.
  * Idle packets or unplugging one mouse must not release another's drag. */
 unsigned buttons=held_buttons();
 tmc_lx_os_pointer(d->x,d->y,buttons,d->wheel);if(d->hwheel)tmc_lx_os_hwheel(d->hwheel);
 d->x=d->y=d->wheel=d->hwheel=0;if(owner)stats.frames++;
}
int tmc_lx_input_register_pointer(struct input_dev *d)
{
 if(!d||d->registered||d->owner)return -EINVAL;
 for(unsigned i=0;i<COUNT(external_pointers);i++)if(!external_pointers[i]){
  d->x=d->y=d->wheel=d->hwheel=d->buttons=0;external_pointers[i]=d;d->registered=1;return 0;
 }
 return -ENOSPC;
}
void tmc_lx_input_unregister_pointer(struct input_dev *d)
{
 if(!d)return;
 for(unsigned i=0;i<COUNT(external_pointers);i++)if(external_pointers[i]==d){
  d->x=d->y=d->wheel=d->hwheel=0;if(d->buttons){d->buttons=0;input_sync(d);}
  external_pointers[i]=0;d->registered=0;return;
 }
}
int tmc_lx_input_register_keyboard(struct input_dev *d)
{
 if(!d||d->registered||d->owner)return -EINVAL;
 for(unsigned i=0;i<COUNT(external_keyboards);i++)if(!external_keyboards[i]){
  memset(d->key,0,sizeof d->key);memset(d->keybit,255,sizeof d->keybit);external_keyboards[i]=d;d->registered=1;return 0;
 }
 return -ENOSPC;
}
void tmc_lx_input_unregister_keyboard(struct input_dev *d)
{
 if(!d)return;
 for(unsigned i=0;i<COUNT(external_keyboards);i++)if(external_keyboards[i]==d){
  memset(d->key,0,sizeof d->key);input_sync(d);external_keyboards[i]=0;d->registered=0;return;
 }
}
void tmc_lx_input_absolute(struct input_dev *d,int x,int y,int wheel,void (*sink)(int,int,int,int))
{
 if(!d||!d->registered||!sink)return;
 sink(x,y,held_buttons(),wheel);if(d->hwheel)tmc_lx_os_hwheel(d->hwheel);d->x=d->y=d->wheel=d->hwheel=0;
}
void usb_to_input_id(struct usb_device *d,struct input_id *id){*id=(struct input_id){3,d->descriptor.idVendor,d->descriptor.idProduct,0};}
int usb_make_path(struct usb_device *d,char *p,size_t n){return tmc_lx_snprintf(p,n,"%s/%s",d->bus->bus_name,d->devpath);}
void *usb_alloc_coherent(struct usb_device *d,size_t n,gfp_t flags,dma_addr_t *dma)
{
 (void)d;if(!dma||!n||n>4096||(flags!=GFP_KERNEL&&flags!=GFP_ATOMIC))return 0;
 void *p=tmc_lx_os_dma_alloc(n,dma);if(p){memset(p,0,n);stats.allocations++;}return p;
}
void usb_free_coherent(struct usb_device *d,size_t n,void *p,dma_addr_t dma){(void)d;(void)n;if(p){stats.frees++;tmc_lx_os_dma_free(p,dma);}}
void usb_fill_int_urb(struct urb *u,struct usb_device *d,unsigned pipe,void *buf,int n,void (*complete)(struct urb *),void *ctx,unsigned interval)
{
 u->type=1;u->dev=d;u->pipe=pipe;u->transfer_buffer=buf;u->transfer_buffer_length=n;u->complete=complete;u->context=ctx;u->interval=interval;
 ((struct LxBinding *)d->owner)->urb=u;
}
static int native_hcd_enqueue(struct usb_hcd *,struct urb *,gfp_t);
static int native_hcd_dequeue(struct usb_hcd *,struct urb *,int);
static void __usb_hcd_giveback_urb(struct urb *);
static const struct hc_driver native_input_hc_driver={native_hcd_enqueue,native_hcd_dequeue};
static void core_endpoints(struct LxBinding *b)
{
 b->usb.state=USB_STATE_CONFIGURED;b->usb.speed=USB_SPEED_FULL;b->usb.can_submit=1;
 atomic_set(&b->usb.references,1);atomic_set(&b->usb.urbnum,0);b->hcd.driver=&native_input_hc_driver;b->hcd.running=1;
 b->ep0.enabled=b->endpoint.enabled=1;INIT_LIST_HEAD(&b->ep0.urb_list);INIT_LIST_HEAD(&b->endpoint.urb_list);
 b->ep0.desc=(struct usb_endpoint_descriptor){7,5,0,0,64,0};
 b->usb.ep_in[0]=b->usb.ep_out[0]=&b->ep0;b->usb.ep_in[1]=&b->endpoint;
}
static struct LxBinding *binding(void *key)
{
 for(unsigned i=0;i<LX_BINDINGS;i++)if(bindings[i].key==key&&key)return &bindings[i];return 0;
}
int tmc_linux_mouse_boot_layout(const TabletLayout *p)
{
 if(!p)return 0;
 return !p->bytes||(!p->id&&p->relative&&p->x.width==8&&p->y.width==8&&p->x.bit==8&&p->y.bit==16&&
   (!p->wheel.width||(p->wheel.width==8&&p->wheel.bit==24)));
}
int tmc_linux_mouse_attach(void *key,unsigned vid,unsigned pid,unsigned packet,unsigned interval)
{
 u32 flags=tmc_lx_os_lock();int success=0;
 if(!key||packet<3||packet>1024)goto done;
 if(binding(key)){success=binding(key)->kind==2;goto done;}
 struct LxBinding *b=0;for(unsigned i=0;i<LX_BINDINGS;i++)if(!bindings[i].key){b=&bindings[i];break;}
 if(!b)goto done;memset(b,0,sizeof *b);b->key=key;b->kind=2;b->generation=++generation;
 b->hcd.self.bus_name="native-hcd";b->usb=(struct usb_device){.bus=&b->hcd.self,.manufacturer="tinymacOS",.product="Linux usbmouse",.devpath="interrupt-in",.descriptor={(u16)vid,(u16)pid},.maxpacket=packet,.owner=b};
 b->endpoint.desc=(struct usb_endpoint_descriptor){7,5,0x81,3,(u16)packet,interval?interval:10};
 core_endpoints(b);
 b->alt.desc.bNumEndpoints=1;b->alt.endpoint=&b->endpoint;b->intf.usbdev=&b->usb;b->intf.cur_altsetting=&b->alt;
 struct usb_driver *driver=tmc_lx_usbmouse_driver();stats.probes++;
 if(driver->probe(&b->intf,driver->id_table)){memset(b,0,sizeof *b);goto done;}
 if(!b->input||b->input->open(b->input)){
  driver->disconnect(&b->intf);memset(b,0,sizeof *b);goto done;
 }
 b->opened=1;stats.attached++;success=1;
 done:tmc_lx_os_unlock(flags);return success;
}
int tmc_linux_mouse_report(void *key,const u8 *data,unsigned n)
{
 u32 flags=tmc_lx_os_lock();int consumed=0;struct LxBinding *b=binding(key);
 if(!b||b->kind!=2||!b->opened||!b->urb||!b->urb->submitted||!data||n<3||n>(unsigned)b->urb->transfer_buffer_length){stats.rejected++;goto done;}
 struct urb *u=b->urb;
 memset(u->transfer_buffer,0,8);memcpy(u->transfer_buffer,data,n);stats.completions++;tmc_lx_native_giveback(u,0,n);consumed=1;
 done:tmc_lx_os_unlock(flags);return consumed;
}
/* A hardware error is not a successful zero-filled report. Retire through
 * the actual upstream URB core and usbmouse IRQ; its cancellation statuses
 * stop resubmission, while transient failures requeue for native recovery.
 * Release only this device's held buttons, without replaying stale deltas.
 */
int tmc_linux_mouse_error(void *key,int status)
{
 u32 flags=tmc_lx_os_lock();int consumed=0;struct LxBinding *b=binding(key);
 if(status>=0||status< -4095||!b||b->kind!=2||!b->opened||!b->urb||!b->urb->submitted){stats.rejected++;goto done;}
 struct input_dev *d=b->input;
 if(d&&d->buttons){
  for(unsigned button=BTN_LEFT;button<=BTN_EXTRA;button++)input_report_key(d,button,0);
  d->x=d->y=d->wheel=0;input_sync(d);
 }
 stats.errors++;tmc_lx_native_giveback(b->urb,status,0);consumed=1;
 done:tmc_lx_os_unlock(flags);return consumed;
}
void tmc_linux_mouse_detach(void *key)
{
 u32 flags=tmc_lx_os_lock();struct LxBinding *b=binding(key);
 if(b&&b->kind==2){tmc_lx_usbmouse_driver()->disconnect(&b->intf);memset(b,0,sizeof *b);stats.disconnected++;}
 tmc_lx_os_unlock(flags);
}
void tmc_linux_mouse_stats(TmcLinuxUsbStats *out){u32 flags=tmc_lx_os_lock();if(out)*out=stats;tmc_lx_os_unlock(flags);}

void *memscan(void *data,int value,size_t n)
{
 u8 *p=data;while(n&&*p!=(u8)value){p++;n--;}return p;
}
void tmc_lx_spin_init(spinlock_t *s){__atomic_store_n(&s->word,0,__ATOMIC_RELAXED);}
unsigned long tmc_lx_spin_lock_irqsave(spinlock_t *s)
{
 u32 flags=tmc_lx_os_irq_save();
 while(__atomic_exchange_n(&s->word,1,__ATOMIC_ACQUIRE))tmc_lx_os_spin_wait();
 return flags;
}
void tmc_lx_spin_unlock_irqrestore(spinlock_t *s,unsigned long flags)
{
 __atomic_store_n(&s->word,0,__ATOMIC_RELEASE);tmc_lx_os_irq_restore((u32)flags);
}
void tmc_lx_hid_log(void *device,unsigned level,const char *fmt,...)
{
 (void)device;char text[192];va_list ap;va_start(ap,fmt);
 int n=tmc_lx_vsnprintf(text,sizeof text,fmt,ap);va_end(ap);
 if(level>=2)stats.errors++;
 tmc_lx_os_error(n<0?"Linux USB driver: unsupported diagnostic format":text);
}
void usb_fill_control_urb(struct urb *u,struct usb_device *d,unsigned pipe,u8 *setup,void *data,int n,void (*complete)(struct urb *),void *context)
{
 u->type=2;u->dev=d;u->pipe=pipe;u->setup_packet=setup;u->transfer_buffer=data;
 u->transfer_buffer_length=n;u->complete=complete;u->context=context;
 ((struct LxBinding *)d->owner)->control_urb=u;
}
int tmc_linux_keyboard_attach(void *key,unsigned vid,unsigned pid,unsigned packet,unsigned interval,unsigned iface,TmcLinuxUsbControl control,void *context)
{
 u32 flags=tmc_lx_os_lock();int success=0;
 if(!key||packet<8||packet>1024||iface>255||!control)goto done;
 struct LxBinding *b=binding(key);
 if(b){success=b->kind==1;goto done;}
 for(unsigned i=0;i<LX_BINDINGS;i++)if(!bindings[i].key){b=&bindings[i];break;}
 if(!b)goto done;memset(b,0,sizeof *b);b->key=key;b->kind=1;b->generation=++generation;
 b->control=control;b->control_context=context;b->hcd.self.bus_name="native-hcd";
 b->usb=(struct usb_device){.bus=&b->hcd.self,.manufacturer="tinymacOS",.product="Linux usbkbd",.devpath="interrupt-in",.descriptor={(u16)vid,(u16)pid},.maxpacket=packet,.owner=b};
 b->endpoint.desc=(struct usb_endpoint_descriptor){7,5,0x81,3,(u16)packet,interval?interval:10};
 core_endpoints(b);
 b->alt.desc.bNumEndpoints=1;b->alt.desc.bInterfaceNumber=iface;b->alt.endpoint=&b->endpoint;
 b->intf.usbdev=&b->usb;b->intf.cur_altsetting=&b->alt;
 struct usb_driver *driver=tmc_lx_usbkbd_driver();kstats.probes++;
 if(driver->probe(&b->intf,driver->id_table)){memset(b,0,sizeof *b);goto done;}
 if(!b->input||b->input->open(b->input)){driver->disconnect(&b->intf);memset(b,0,sizeof *b);goto done;}
 b->opened=1;kstats.attached++;success=1;
 done:tmc_lx_os_unlock(flags);return success;
}
int tmc_linux_keyboard_report(void *key,const u8 *data,unsigned n)
{
 u32 flags=tmc_lx_os_lock();int consumed=0;struct LxBinding *b=binding(key);
 if(!b||b->kind!=1||!b->opened||!b->urb||!b->urb->submitted||!data||n!=8){kstats.rejected++;goto done;}
 /* A Boot error-rollover report is not a trustworthy set of key releases. */
 for(unsigned i=2;i<8;i++)if(data[i]>=1&&data[i]<=3){kstats.rejected++;consumed=1;goto done;}
 struct urb *u=b->urb;
 memcpy(u->transfer_buffer,data,8);kstats.completions++;tmc_lx_native_giveback(u,0,8);consumed=1;
 done:tmc_lx_os_unlock(flags);return consumed;
}
void tmc_linux_keyboard_detach(void *key)
{
 u32 flags=tmc_lx_os_lock();struct LxBinding *b=binding(key);
 if(b&&b->kind==1){tmc_lx_usbkbd_driver()->disconnect(&b->intf);memset(b,0,sizeof *b);kstats.disconnected++;}
 tmc_lx_os_unlock(flags);
}
void tmc_linux_keyboard_leds(unsigned value)
{
 u32 flags=tmc_lx_os_lock();
 for(unsigned i=0;i<LX_BINDINGS;i++){
  struct LxBinding *b=bindings+i;struct input_dev *d=b->input;
  if(b->kind!=1||!b->opened||!d||!d->event)continue;
  d->led[0]=value&31;d->event(d,EV_LED,LED_CAPSL,!!(value&2));
 }
 tmc_lx_os_unlock(flags);
}
/* One bounded native EP0 transaction per service call. Never hold the Linux
 * compatibility lock across HCD waits: xHCI can dispatch unrelated interrupt
 * completions from inside a control wait. Copy request/payload before unlock;
 * after cancellation/disconnect, touch neither freed URB nor driver context.
 * Generation AND submit serial checks prevent slot/URB-address ABA reuse.
 */
void tmc_linux_usb_service(void)
{
 static unsigned cursor;
 u32 flags=tmc_lx_os_lock();
 if(service_busy){tmc_lx_os_unlock(flags);return;}service_busy=1;
 for(unsigned scan=0;scan<LX_BINDINGS;scan++){
  unsigned index=(cursor+scan)%LX_BINDINGS;struct LxBinding *b=bindings+index;struct urb *u=b->control_urb;
  if(!b->key||!b->opened||!u||!u->submitted||b->servicing)continue;
  u8 setup[8],data[8];unsigned n=u->transfer_buffer_length;
  memcpy(setup,u->setup_packet,8);memcpy(data,u->transfer_buffer,n);
  u64 gen=b->generation,serial=u->serial;TmcLinuxUsbControl control=b->control;void *context=b->control_context;
  b->servicing=1;kstats.controls++;cursor=(index+1)%LX_BINDINGS;
  tmc_lx_os_unlock(flags);
  int status=control(context,setup,data,n);
  flags=tmc_lx_os_lock();b=bindings+index;
  if(b->generation==gen&&b->control_urb==u){
   b->servicing=0;
   if(u->serial==serial&&u->submitted){
    if(status)kstats.control_errors++;
    tmc_lx_native_giveback(u,status,status?0:n);
   }else kstats.stale_controls++;
  }else kstats.stale_controls++;
  break;
 }
 service_busy=0;tmc_lx_os_unlock(flags);
}
void tmc_linux_keyboard_stats(TmcLinuxKeyboardStats *out)
{
 u32 flags=tmc_lx_os_lock();if(out)*out=kstats;tmc_lx_os_unlock(flags);
}

/* Copying native HCD backend. Actual upstream hcd.c owns endpoint queues,
 * submission/unlink references and direct giveback below. Hardware rings/DMA
 * and the physical xHCI/EHCI/UHCI implementations remain native. */
static int tmc_native_hcd_validate(struct usb_hcd *h,struct urb *u,gfp_t flags)
{
 if(!u||!u->dev||!u->ep||!h)return -EINVAL;
 struct LxBinding *b=u->dev->owner;int result=0;
 if(atomic_read(&u->reject))result=-EPERM;
 else if(flags!=GFP_KERNEL&&flags!=GFP_ATOMIC)result=-EINVAL;
 else if(!b||!b->key||&b->usb!=u->dev||&b->hcd!=h||h->driver!=&native_input_hc_driver)result=-ENODEV;
 else if(u->num_sgs||u->sg||u->number_of_packets)result=-EOPNOTSUPP;
 else if(u->transfer_buffer_length<0||u->transfer_buffer_length>8||
         (u->transfer_buffer_length&&!u->transfer_buffer))result=-EINVAL;
 else if(usb_endpoint_type(&u->ep->desc)==USB_ENDPOINT_XFER_INT){
  if(!u->transfer_buffer_length||!(u->pipe&USB_DIR_IN)||b->urb!=u)result=-EOPNOTSUPP;
 }else if(usb_endpoint_type(&u->ep->desc)==USB_ENDPOINT_XFER_CONTROL){
  if(!u->setup_packet||(u->setup_packet[0]&USB_DIR_IN)||b->control_urb!=u||!b->control)result=-EOPNOTSUPP;
 }else result=-EOPNOTSUPP;
 if(!result){
  struct urb *active=usb_endpoint_type(&u->ep->desc)==USB_ENDPOINT_XFER_INT?b->active_input:b->active_control;
  if(active)result=-EBUSY;
 }
 return result;
}
static int native_hcd_enqueue(struct usb_hcd *h,struct urb *u,gfp_t flags)
{
 (void)flags;int result=usb_hcd_link_urb_to_ep(h,u);if(result){ustats.rejected++;return result;}
 struct LxBinding *b=u->dev->owner;
 if(usb_endpoint_type(&u->ep->desc)==USB_ENDPOINT_XFER_INT)b->active_input=u;else b->active_control=u;
 u->hcpriv=b;u->serial++;u->submitted=1;HCOUNT(links);HCOUNT(queued);
 ustats.submits++;ustats.active++;if(ustats.active>ustats.highwater)ustats.highwater=ustats.active;
 return 0;
}
/* Retire native state before original hcd.c giveback: callbacks may resubmit
 * or free their driver reference. The original core retains ownership. */
void tmc_lx_native_giveback(struct urb *u,int status,unsigned length)
{
 if(!u||!u->hcpriv||!u->submitted)return;struct LxBinding *b=u->hcpriv;
 if(b->active_input==u)b->active_input=0;if(b->active_control==u)b->active_control=0;
 u->submitted=0;u->serial++;u->actual_length=length;
 usb_hcd_unlink_urb_from_ep(&b->hcd,u);
 if(!u->unlinked)u->unlinked=status;
 ustats.givebacks++;ustats.active--;HCOUNT(givebacks);
 __usb_hcd_giveback_urb(u);
}
static int native_hcd_dequeue(struct usb_hcd *h,struct urb *u,int status)
{
 int result=usb_hcd_check_unlink_urb(h,u,status);if(result)return result;
 ustats.unlinks++;HCOUNT(unlinks);tmc_lx_native_giveback(u,status,0);return 0;
}
#include "hcd_runtime.inc"
void tmc_lx_spin_lock_irq(spinlock_t *s){(void)tmc_lx_spin_lock_irqsave(s);}
void tmc_lx_spin_unlock_irq(spinlock_t *s){tmc_lx_spin_unlock_irqrestore(s,0x200);}
void tmc_linux_urb_stats(TmcLinuxUrbStats *out)
{
 u32 flags=tmc_lx_os_lock();if(out)*out=ustats;tmc_lx_os_unlock(flags);
}
