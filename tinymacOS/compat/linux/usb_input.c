/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "linux_usb.h"
#include "linux/tmc_compat.h"
/* One CPU0-polled native input pipe supplies each logical interrupt URB.
 * Serialized entry points share the native IRQ-safe lock. There is no claim
 * that these interface/URB objects implement all of Linux USB core or HCDs.
 */
#define LX_BINDINGS 128
struct LxBinding {
 void *key;struct usb_bus bus;struct usb_device usb;struct usb_interface intf;
 struct usb_host_interface alt;struct usb_host_endpoint endpoint;
 struct input_dev *input;struct urb *urb;int opened;
};
static struct LxBinding bindings[LX_BINDINGS];
static TmcLinuxUsbStats tmc_linux_usb_stats;
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
int tmc_lx_snprintf(char *out,size_t n,const char *fmt,...)
{
 va_list ap;va_start(ap,fmt);size_t at=0;int error=0;
 for(;*fmt;fmt++){
  if(*fmt!='%'){charout(out,n,&at,*fmt);continue;}
  fmt++;char pad=' ';unsigned width=0;if(*fmt=='0'){pad='0';fmt++;}
  while(*fmt>='0'&&*fmt<='9'){width=width*10+*fmt-'0';fmt++;if(width>128){error=1;break;}}
  if(error)break;
  if(*fmt=='%'){charout(out,n,&at,'%');continue;}
  if(*fmt=='s'){const char *s=va_arg(ap,const char *);if(!s)s="(null)";while(*s)charout(out,n,&at,*s++);continue;}
  if(*fmt=='d'||*fmt=='u'||*fmt=='x'){
   i64 v=*fmt=='d'?va_arg(ap,int):(i64)va_arg(ap,unsigned);u64 u=v<0?(u64)-v:(u64)v;char digits[32];unsigned len=0,base=*fmt=='x'?16:10;
   do{digits[len++]="0123456789abcdef"[u%base];u/=base;}while(u);
   if(v<0){charout(out,n,&at,'-');if(width)width--;}
   while(width>len){charout(out,n,&at,pad);width--;}
   while(len)charout(out,n,&at,digits[--len]);continue;
  }
  error=1;break;
 }
 va_end(ap);if(n)out[at<n?at:n-1]=0;return error?-EINVAL:(int)at;
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
 if(d->registered&&d->buttons){d->buttons=d->x=d->y=d->wheel=0;input_sync(d);}
 if(b)b->input=0;d->registered=0;input_free_device(d);
}
void input_report_key(struct input_dev *d,unsigned code,int on)
{
 if(!d||!d->registered||code<BTN_LEFT||code>BTN_EXTRA)return;
 unsigned bit=1u<<(code-BTN_LEFT);if(on)d->buttons|=bit;else d->buttons&=~bit;
}
void input_report_rel(struct input_dev *d,unsigned axis,int delta)
{
 if(!d||!d->registered)return;
 if(axis==REL_X)d->x=delta;else if(axis==REL_Y)d->y=delta;else if(axis==REL_WHEEL)d->wheel=delta;
}
void input_sync(struct input_dev *d)
{
 if(!d||!d->registered)return;
 tmc_lx_os_pointer(d->x,d->y,d->buttons,d->wheel);d->x=d->y=d->wheel=0;stats.frames++;
}
void usb_to_input_id(struct usb_device *d,struct input_id *id){*id=(struct input_id){3,d->descriptor.idVendor,d->descriptor.idProduct,0};}
int usb_make_path(struct usb_device *d,char *p,size_t n){return tmc_lx_snprintf(p,n,"%s/%s",d->bus->bus_name,d->devpath);}
void *usb_alloc_coherent(struct usb_device *d,size_t n,gfp_t flags,dma_addr_t *dma)
{
 (void)d;if(!dma||!n||n>4096||(flags!=GFP_KERNEL&&flags!=GFP_ATOMIC))return 0;
 void *p=tmc_lx_os_dma_alloc(n,dma);if(p){memset(p,0,n);stats.allocations++;}return p;
}
void usb_free_coherent(struct usb_device *d,size_t n,void *p,dma_addr_t dma){(void)d;(void)n;if(p){stats.frees++;tmc_lx_os_dma_free(p,dma);}}
struct urb *usb_alloc_urb(int iso,gfp_t flags){return iso?0:tmc_lx_zalloc(sizeof(struct urb),flags);}
void usb_free_urb(struct urb *u){if(u){usb_kill_urb(u);tmc_lx_free(u);}}
int usb_submit_urb(struct urb *u,gfp_t flags)
{
 if(!u||!u->dev||!u->complete||!u->transfer_buffer||u->transfer_buffer_length<1||u->transfer_buffer_length>8||!(u->pipe&128))return -EINVAL;
 if(flags!=GFP_KERNEL&&flags!=GFP_ATOMIC)return -EINVAL;
 if(u->killed)return -ESHUTDOWN;if(u->submitted)return -EBUSY;
 u->submitted=1;return 0;
}
void usb_kill_urb(struct urb *u)
{
 if(!u)return;int pending=u->submitted;u->killed=1;u->submitted=0;
 if(pending&&u->complete){u->status=-ENOENT;u->actual_length=0;u->complete(u);}
 u->killed=0; /* synchronous cancellation is not permanent URB poisoning */
}
void usb_fill_int_urb(struct urb *u,struct usb_device *d,unsigned pipe,void *buf,int n,void (*complete)(struct urb *),void *ctx,unsigned interval)
{
 u->dev=d;u->pipe=pipe;u->transfer_buffer=buf;u->transfer_buffer_length=n;u->complete=complete;u->context=ctx;u->interval=interval;
 ((struct LxBinding *)d->owner)->urb=u;
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
 if(binding(key)){success=1;goto done;}
 struct LxBinding *b=0;for(unsigned i=0;i<LX_BINDINGS;i++)if(!bindings[i].key){b=&bindings[i];break;}
 if(!b)goto done;memset(b,0,sizeof *b);b->key=key;
 b->bus.bus_name="native-hcd";b->usb=(struct usb_device){.bus=&b->bus,.manufacturer="tinymacOS",.product="Linux usbmouse",.devpath="interrupt-in",.descriptor={(u16)vid,(u16)pid},.maxpacket=packet,.owner=b};
 b->endpoint.desc=(struct usb_endpoint_descriptor){7,5,0x81,3,(u16)packet,interval?interval:10};
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
 if(!b||!b->opened||!b->urb||!b->urb->submitted||!data||n<3||n>(unsigned)b->urb->transfer_buffer_length){stats.rejected++;goto done;}
 struct urb *u=b->urb;u->submitted=0;u->status=0;u->actual_length=n;
 memset(u->transfer_buffer,0,8);memcpy(u->transfer_buffer,data,n);stats.completions++;u->complete(u);consumed=1;
 done:tmc_lx_os_unlock(flags);return consumed;
}
void tmc_linux_mouse_detach(void *key)
{
 u32 flags=tmc_lx_os_lock();struct LxBinding *b=binding(key);
 if(b){tmc_lx_usbmouse_driver()->disconnect(&b->intf);memset(b,0,sizeof *b);stats.disconnected++;}
 tmc_lx_os_unlock(flags);
}
void tmc_linux_mouse_stats(TmcLinuxUsbStats *out){u32 flags=tmc_lx_os_lock();if(out)*out=stats;tmc_lx_os_unlock(flags);}
