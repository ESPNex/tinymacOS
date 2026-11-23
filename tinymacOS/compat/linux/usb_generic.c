/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "linux_usb.h"
#include "linux_hid_generic.h"
#include <linux/tmc_compat.h>
/* CPU0 native-HCD report subscription. Controllers own static DMA/rings and
 * configure/arm/cancel them; this adapter never pretends to be a Linux HCD.
 * Generic attach precedes native IN arming. stop removes the subscription,
 * not controller-owned memory. Every start re-reads the real descriptor prefix.
 * Native raw requests are bounded synchronous EP0 transfers, not sleeping USB
 * Linux requests. Reports arriving reentrantly during EP0 are copied/queued. */
#define NG 32
#define NINPUT 16
#define QUEUE 8
struct NativeInput {struct input_dev *input;unsigned kind,axes;int value[2],low[2],high[2];};
struct GenericBinding {
 void *key;TmcHidGeneric *hid;TmcLinuxHidControl control;void *context;
 unsigned packet,iface,width,height,kinds,started,opened,busy,fatal,quiesce,led_pending,leds;
 unsigned head,count;u8 queue[QUEUE][64],length[QUEUE],prefix[8];unsigned prefix_n;
 TmcLinuxAbsolute absolute;struct NativeInput input[NINPUT];
};
static struct GenericBinding *generic_bindings[NG];
static struct {u64 attached,failed,received,dispatched,rejected,queued,overflows,errors,detached,controls,control_errors;} tmc_native_generic_counters;
#define GCOUNT(f) __atomic_add_fetch(&tmc_native_generic_counters.f,1,__ATOMIC_RELAXED)
static struct GenericBinding *find_generic(void *key)
{
 for(unsigned i=0;i<NG;i++)if(generic_bindings[i]&&generic_bindings[i]->key==key)return generic_bindings[i];return 0;
}
static int binding_start(void *arg)
{
 struct GenericBinding *b=arg;u8 actual[8];
 if(!b||b->started||!b->control||!b->key)return -EINVAL;
 GCOUNT(controls);int n=b->control(b->context,0x81,6,0x2200,b->iface,actual,b->prefix_n);
 if(n!=(int)b->prefix_n||memcmp(actual,b->prefix,b->prefix_n)){GCOUNT(control_errors);return n<0?n:-EIO;}
 b->started=1;return 0;
}
static void binding_stop(void *arg){struct GenericBinding *b=arg;b->opened=b->started=0;}
static int binding_open(void *arg)
{
 struct GenericBinding *b=arg;if(!b->started||b->opened)return -EINVAL;b->opened=1;return 0;
}
static void binding_close(void *arg){((struct GenericBinding *)arg)->opened=0;}
static int binding_input(void *arg,unsigned token,unsigned app,int add)
{
 struct GenericBinding *b=arg;if(!token||token>NINPUT)return -EINVAL;struct NativeInput *n=b->input+token-1;
 u32 flags=tmc_lx_os_lock();
 if(!add){
  if(n->input){if(n->kind==1)tmc_lx_input_unregister_keyboard(n->input);else tmc_lx_input_unregister_pointer(n->input);input_free_device(n->input);n->input=0;}
  tmc_lx_os_unlock(flags);return 0;
 }
 unsigned kind=app==0x10006||app==0x10007?1:app==0x10001||app==0x10002?2:0;
 if(!kind||n->input){tmc_lx_os_unlock(flags);return -EOPNOTSUPP;}
 n->input=input_allocate_device();n->kind=kind;
 int result=!n->input?-ENOMEM:kind==1?tmc_lx_input_register_keyboard(n->input):tmc_lx_input_register_pointer(n->input);
 if(result){input_free_device(n->input);n->input=0;}else b->kinds|=kind;
 tmc_lx_os_unlock(flags);return result;
}
static void binding_axis(void *arg,unsigned token,unsigned code,int low,int high,int resolution)
{
 struct GenericBinding *b=arg;(void)resolution;
 if(!token||token>NINPUT||code>1)return;struct NativeInput *n=b->input+token-1;
 n->axes|=1u<<code;n->low[code]=low;n->high[code]=high;n->value[code]=low;
}
static void binding_event(void *arg,unsigned token,unsigned type,unsigned code,int value)
{
 struct GenericBinding *b=arg;if(!token||token>NINPUT)return;struct NativeInput *n=b->input+token-1;if(!n->input)return;
 u32 flags=tmc_lx_os_lock();
 if(type==EV_KEY)input_report_key(n->input,code,value);
 else if(type==EV_REL)input_report_rel(n->input,code,value);
 else if(type==3&&code<2)n->value[code]=value; /* EV_ABS */
 else if(type==0&&code==0){ /* EV_SYN / SYN_REPORT */
  if(n->kind==2&&n->axes==3){
   int x=pointer_pixel(n->value[0]<=n->low[0]?0:(u64)((i64)n->value[0]-n->low[0]),0,(u32)((i64)n->high[0]-n->low[0]),b->width);
   int y=pointer_pixel(n->value[1]<=n->low[1]?0:(u64)((i64)n->value[1]-n->low[1]),0,(u32)((i64)n->high[1]-n->low[1]),b->height);
   tmc_lx_input_absolute(n->input,x,y,n->input->wheel,b->absolute);
  }else input_sync(n->input);
 }
 tmc_lx_os_unlock(flags);
}
static int binding_raw(void *arg,unsigned id,u8 *bytes,unsigned n,unsigned type,unsigned request)
{
 struct GenericBinding *b=arg;if(!b->started||type>2||!n||n>1024||(request!=1&&request!=9))return -EINVAL;
 GCOUNT(controls);int result=b->control(b->context,request==1?0xa1:0x21,request,((type+1)<<8)|id,b->iface,bytes,n);
 if(result<0)GCOUNT(control_errors);return result;
}
static const TmcHidGenericOps generic_ops={binding_start,binding_stop,binding_open,binding_close,binding_input,binding_event,binding_raw,0,binding_axis};
int tmc_linux_generic_attach(void *key,const u8 *desc,unsigned length,unsigned packet,unsigned iface,TmcLinuxHidControl control,void *context,unsigned width,unsigned height,TmcLinuxAbsolute absolute)
{
 if(!key||!desc||!length||length>1024||!packet||packet>64||!control||!width||!height||!absolute||find_generic(key))return 0;
 unsigned slot=0;while(slot<NG&&generic_bindings[slot])slot++;if(slot==NG)return 0;
 struct GenericBinding *b=tmc_lx_zalloc(sizeof(*b),GFP_KERNEL);if(!b)return 0;
 *b=(struct GenericBinding){.key=key,.control=control,.context=context,.packet=packet,.iface=iface,.width=width,.height=height,.absolute=absolute,.prefix_n=MIN(length,8u),.busy=1};memcpy(b->prefix,desc,b->prefix_n);
 generic_bindings[slot]=b;b->hid=tmc_hid_generic_attach(desc,length,&generic_ops,b);
 int input_bytes=b->hid?tmc_hid_generic_input_bytes(b->hid):0;
 int valid=input_bytes>0&&(unsigned)input_bytes<=packet;
 for(unsigned i=0;i<NINPUT;i++)if(b->input[i].input&&b->input[i].axes&&
  (b->input[i].axes!=3||b->input[i].low[0]>=b->input[i].high[0]||b->input[i].low[1]>=b->input[i].high[1]))valid=0;
 if(!valid){if(b->hid)(void)tmc_hid_generic_detach(b->hid);generic_bindings[slot]=0;tmc_lx_free(b);GCOUNT(failed);return 0;}
 b->busy=0;GCOUNT(attached);return b->kinds;
}
static void drain_generic(struct GenericBinding *b,int do_output)
{
 if(!b||b->busy||!b->hid)return;b->busy=1;
 if(b->quiesce){if(tmc_hid_generic_quiesce(b->hid)){b->busy=0;return;}b->quiesce=0;}
 if(do_output&&!b->fatal&&b->led_pending){
  unsigned on=b->leds;b->led_pending=0;
  for(unsigned i=0;i<5;i++)if(tmc_hid_generic_set_led(b->hid,i,!!(on&(1u<<i)))==-EBUSY)b->led_pending=1;
 }
 if(do_output&&!b->fatal)(void)tmc_hid_generic_service(b->hid);
 for(unsigned i=0;i<QUEUE&&b->count&&!b->fatal;i++){
  unsigned at=b->head;int result=tmc_hid_generic_report(b->hid,b->queue[at],b->length[at]);
  if(result==-EBUSY)break;
  b->head=(at+1)%QUEUE;b->count--;if(result)GCOUNT(rejected);else GCOUNT(dispatched);
 }
 b->busy=0;
}
int tmc_linux_generic_report(void *key,const u8 *bytes,unsigned n)
{
 struct GenericBinding *b=find_generic(key);if(!b||!b->opened||b->fatal)return -ENODEV;
 GCOUNT(received);if(!bytes||!n||n>b->packet){GCOUNT(rejected);return -EINVAL;}
 if(b->count==QUEUE){b->head=b->count=0;b->quiesce=1;GCOUNT(overflows);return -ENOSPC;}
 unsigned at=(b->head+b->count)%QUEUE;memcpy(b->queue[at],bytes,n);b->length[at]=n;b->count++;if(b->busy)GCOUNT(queued);
 drain_generic(b,0);return 0; /* transport consumed/copied, not a fabricated input frame */
}
void tmc_linux_generic_error(void *key,int fatal)
{
 struct GenericBinding *b=find_generic(key);if(!b)return;b->head=b->count=0;b->quiesce=1;b->fatal=!!fatal;GCOUNT(errors);drain_generic(b,0);
}
int tmc_linux_generic_detach(void *key)
{
 struct GenericBinding *b=find_generic(key);if(!b)return 0;if(b->busy)return -EBUSY;b->busy=1;
 int result=tmc_hid_generic_detach(b->hid);if(result){b->busy=0;return result;}
 for(unsigned i=0;i<NG;i++)if(generic_bindings[i]==b){generic_bindings[i]=0;break;}
 tmc_lx_free(b);GCOUNT(detached);return 0;
}
void tmc_linux_generic_service(void)
{
 for(unsigned i=0;i<NG;i++)if(generic_bindings[i])drain_generic(generic_bindings[i],1);
}
void tmc_linux_generic_leds(unsigned on)
{
 for(unsigned i=0;i<NG;i++)if(generic_bindings[i]){generic_bindings[i]->leds=on;generic_bindings[i]->led_pending=1;}
}
