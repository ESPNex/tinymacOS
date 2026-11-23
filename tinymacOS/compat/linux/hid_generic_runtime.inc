/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Whole hid-input.c and hid-generic.c, with real bounded event/transport
 * services. USB HCD/core/hub/usbhid remain OUTSIDE this runtime. */
#define TMC_LINUX_HID_UNIT 1
#include <linux/tmc_hid_input.h>
#include <stdarg.h>
static TmcHidGenericStats tmc_hid_generic_counters;
static int generic_model_supported(struct hid_device *);
struct TmcHidGeneric {
 TmcLinuxHidParser *parser;TmcHidGenericOps ops;void *context;
 u8 *buffer;unsigned started,users,inputs,token;
};
static TmcHidGeneric *generic_device(struct hid_device *d){return d->tmc_generic;}
int tmc_hid_strcmp(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return (u8)*a-(u8)*b;}
int tmc_hid_abs(int v){return v<0?-v:v;}
char *kasprintf(unsigned flags,const char *fmt,...)
{
 if(tmc_hid_strcmp(fmt,"%s %s"))return (void *)0;
 va_list ap;va_start(ap,fmt);const char *a=va_arg(ap,const char *),*b=va_arg(ap,const char *);va_end(ap);
 size_t an=strlen(a),bn=strlen(b);if(an>256||bn>256)return (void *)0;
 char *out=tmc_hid_alloc(an+bn+2,0);if(out){memcpy(out,a,an);out[an]=' ';memcpy(out+an+1,b,bn+1);}return out;
}
struct input_dev *input_allocate_device(void){return tmc_hid_alloc(sizeof(struct input_dev),1);}
void input_free_device(struct input_dev *i){tmc_hid_free(i);}
void input_event(struct input_dev *i,unsigned type,unsigned code,int value)
{
 if(!i||!i->registered||type>=EV_CNT)return;
 if(type==EV_KEY){if(code>=KEY_CNT||!test_bit(code,i->keybit))return;value=!!value;if(test_bit(code,i->key)==value)return;if(value)set_bit(code,i->key);else clear_bit(code,i->key);}
 else if(type==EV_REL){if(code>=REL_CNT||!test_bit(code,i->relbit)||!value)return;}
 else if(type==EV_ABS){if(code>=ABS_CNT||!test_bit(code,i->absbit))return;if(i->absinfo[code].value==value)return;i->absinfo[code].value=value;}
 else if(type!=EV_SYN&&!test_bit(type,i->evbit))return;
 TmcHidGeneric *g=generic_device(i->drvdata);g->ops.event(g->context,i->token,type,code,value);
 __atomic_add_fetch(&tmc_hid_generic_counters.events,1,__ATOMIC_RELAXED);
}
int input_register_device(struct input_dev *i)
{
 struct hid_device *d=i?i->drvdata:(void *)0;TmcHidGeneric *g=d?generic_device(d):(void *)0;
 if(!g||i->registered||g->inputs>=16)return -EINVAL;
 struct hid_input *h;unsigned app=0;list_for_each_entry(h,&d->inputs,list)if(h->input==i){app=h->application;break;}
 i->token=++g->token;int result=g->ops.input(g->context,i->token,app,1);if(result)return result;
 if(g->ops.axis)for(unsigned c=0;c<ABS_CNT;c++)if(test_bit(c,i->absbit))g->ops.axis(g->context,i->token,c,i->absinfo[c].minimum,i->absinfo[c].maximum,i->absinfo[c].resolution);
 result=i->open?i->open(i):0;
 if(result){g->ops.input(g->context,i->token,app,0);return result;}
 i->registered=1;g->inputs++;__atomic_add_fetch(&tmc_hid_generic_counters.inputs,1,__ATOMIC_RELAXED);return 0;
}
void input_unregister_device(struct input_dev *i)
{
 if(!i)return;
 if(i->registered){
  TmcHidGeneric *g=generic_device(i->drvdata);
  for(unsigned k=0;k<KEY_CNT;k++)if(test_bit(k,i->key))input_event(i,EV_KEY,k,0);
  input_sync(i);if(i->close)i->close(i);
  g->ops.input(g->context,i->token,0,0);g->inputs--;i->registered=0;
 }
 input_free_device(i);
}
int hid_hw_open(struct hid_device *d)
{
 TmcHidGeneric *g=generic_device(d);if(!g||!g->started)return -ENODEV;
 if(!g->users){int result=g->ops.open(g->context);if(result)return result;}
 g->users++;return 0;
}
void hid_hw_close(struct hid_device *d)
{
 TmcHidGeneric *g=generic_device(d);if(g&&g->users&&!--g->users)g->ops.close(g->context);
}
int hid_hw_output_report(struct hid_device *d,u8 *buf,size_t n)
{
 TmcHidGeneric *g=generic_device(d);if(!g||!buf||!n||n>HID_MAX_BUFFER_SIZE)return -EINVAL;
 if(!g->ops.output)return -ENOSYS;
 int r=g->ops.output(g->context,buf,n);__atomic_add_fetch(&tmc_hid_generic_counters.outputs,1,__ATOMIC_RELAXED);
 if(r!=(int)n){__atomic_add_fetch(&tmc_hid_generic_counters.output_errors,1,__ATOMIC_RELAXED);return r<0?r:-EINVAL;}return r;
}
int hid_hw_raw_request(struct hid_device *d,unsigned id,u8 *buf,size_t n,unsigned type,unsigned request)
{
 TmcHidGeneric *g=generic_device(d);if(!g||!buf||!n||n>HID_MAX_BUFFER_SIZE||type>=HID_REPORT_TYPES)return -EINVAL;
 if(!g->ops.raw)return -EOPNOTSUPP;
 int r=g->ops.raw(g->context,id,buf,n,type,request);__atomic_add_fetch(&tmc_hid_generic_counters.outputs,1,__ATOMIC_RELAXED);
 if(r<0||(unsigned)r>n||(request==HID_REQ_SET_REPORT&&(unsigned)r!=n)){__atomic_add_fetch(&tmc_hid_generic_counters.output_errors,1,__ATOMIC_RELAXED);return r<0?r:-EINVAL;}return r;
}
void hid_hw_request(struct hid_device *d,struct hid_report *r,int request){__hid_request(d,r,request);}
void hid_hw_wait(struct hid_device *d)
{
 /* This transport contract is synchronous: raw/request has actually completed
  * before returning. There is no pending asynchronous request to fake-wait. */
 (void)d;
}
#include "../../vendor/linux-v6.12/hid-event-bits.inc"
#include "../../vendor/linux-v6.12/hid-event-process.inc"
#include "../../vendor/linux-v6.12/hid-event-output.inc"
int __hid_request(struct hid_device *d,struct hid_report *r,int request)
{
 u8 *buffer=hid_alloc_report_buf(r,GFP_KERNEL);if(!buffer)return -ENOMEM;
 unsigned n=hid_report_len(r);if(request==HID_REQ_SET_REPORT)hid_output_report(r,buffer);
 int result=hid_hw_raw_request(d,r->id,buffer,n,r->type,request);
 if(result>=0&&request==HID_REQ_GET_REPORT){
  if((unsigned)result<n||(r->id&&buffer[0]!=r->id))result=-EINVAL;
  else hid_process_report(d,r,buffer+(r->id>0),0);
 }
 tmc_hid_free(buffer);return result<0?result:0;
}
/* Battery support is CONFIG-disabled, as are force feedback and HIDRAW/HIDDEV.
 * Original upstream disabled-config paths are compiled without replacement. */
#pragma GCC push_options
#pragma GCC optimize ("-fwrapv")
#define hid_setup_resolution_multiplier checked_resolution_multiplier
#include "../../vendor/linux-v6.12/hid-input.c"
#undef hid_setup_resolution_multiplier
#pragma GCC pop_options
int hid_hw_start(struct hid_device *d,unsigned mask)
{
 TmcHidGeneric *g=generic_device(d);if(!g||g->started||!(mask&HID_CONNECT_HIDINPUT)||!generic_model_supported(d)||allocation_failed||invalid_multiplier)return -EINVAL;
 int result=g->ops.start(g->context);if(result)return result;g->started=1;
 result=hidinput_connect(d,0);
 if(result){g->ops.stop(g->context);g->started=0;return result;}
 d->claimed=HID_CLAIMED_INPUT;hid_process_ordering(d);return 0;
}
struct bus_type hid_bus_type;
int bus_for_each_drv(struct bus_type *bus,struct device_driver *start,void *data,int (*fn)(struct device_driver *,void *))
{
 if(!bus||!fn)return -EINVAL;unsigned at=0;
 if(start){while(at<bus->count&&&bus->drivers[at]->driver!=start)at++;if(at==bus->count)return -EINVAL;at++;}
 for(;at<bus->count;at++){int r=fn(&bus->drivers[at]->driver,data);if(r)return r;}return 0;
}
const struct hid_device_id *hid_match_device(struct hid_device *d,struct hid_driver *driver)
{
 if(!d||!driver||!driver->id_table)return (void *)0;
 const struct hid_device_id *id=driver->id_table;
 for(;id->bus||id->group||id->vendor||id->product;id++)if((id->bus==HID_BUS_ANY||id->bus==d->bus)&&(id->group==HID_GROUP_ANY||id->group==d->group)&&(id->vendor==HID_ANY_ID||id->vendor==d->vendor)&&(id->product==HID_ANY_ID||id->product==d->product))return id;
 return (void *)0;
}
#include "../../vendor/linux-v6.12/hid-generic.c"
static int generic_lock(void)
{
 if(__atomic_test_and_set(&parsing,__ATOMIC_ACQUIRE)){__atomic_add_fetch(&tmc_hid_generic_counters.busy,1,__ATOMIC_RELAXED);return 0;}return 1;
}
static void generic_unlock(void){__atomic_clear(&parsing,__ATOMIC_RELEASE);}
static int generic_model_supported(struct hid_device *d)
{
 for(unsigned t=0;t<HID_REPORT_TYPES;t++)for(unsigned id=0;id<HID_MAX_IDS;id++){
  struct hid_report *r=d->report_enum[t].report_id_hash[id];if(!r)continue;
  if((d->report_enum[t].numbered&&!id)||r->maxfield==HID_MAX_FIELDS)return 0;
  for(unsigned i=0;i<r->maxfield;i++){
   struct hid_field *f=r->field[i];
   if(!f->report_size||f->report_size>16||f->logical_minimum==INT32_MIN||f->logical_maximum<f->logical_minimum||
      (i64)f->logical_maximum-f->logical_minimum>INT32_MAX||
      (i64)f->physical_maximum-f->physical_minimum>INT32_MAX||
      (i64)f->physical_maximum-f->physical_minimum<INT32_MIN||
      f->unit_exponent< -8||f->unit_exponent>8)return 0;
  }
 }
 return 1;
}
TmcHidGeneric *tmc_hid_generic_attach(const u8 *desc,unsigned n,const TmcHidGenericOps *ops,void *context)
{
 if(!desc||!n||n>1024||!ops||!ops->start||!ops->stop||!ops->open||!ops->close||!ops->input||!ops->event)return (void *)0;
 if(!generic_lock())return (void *)0;
 __atomic_add_fetch(&tmc_hid_generic_counters.probes,1,__ATOMIC_RELAXED);allocation_failed=invalid_multiplier=0;
 TmcHidGeneric *g=tmc_hid_alloc(sizeof(*g),1);TmcLinuxHidParser *p=(void *)0;
 if(g){g->ops=*ops;g->context=context;g->parser=p=tmc_hid_alloc(sizeof(*p),1);}
 if(p){
  struct hid_device *d=&p->device;d->ll_driver=&p->low;d->driver=tmc_generic_driver();d->tmc_generic=g;d->dev.named=1;
  d->dev_rdesc=desc;d->dev_rsize=n;d->bus=3;d->group=HID_GROUP_GENERIC;
  memcpy(d->name,"USB generic HID",16);INIT_LIST_HEAD(&d->inputs);INIT_LIST_HEAD(&d->debug_list);
  for(unsigned t=0;t<HID_REPORT_TYPES;t++)INIT_LIST_HEAD(&d->report_enum[t].report_list);
  if(!hid_bus_type.count)hid_bus_type.drivers[hid_bus_type.count++]=d->driver;
  const struct hid_device_id *id=hid_match_device(d,d->driver);
  int result=!id||!d->driver->match(d,0)?-ENODEV:d->driver->probe(d,id);
  /* Registration is rolled back if upstream ignored a failed field/order
   * allocation, the checked multiplier failed, or this bounded sink cannot
   * represent the descriptor. Never return a partial successful device. */
  if(!result&&(!generic_model_supported(d)||allocation_failed||invalid_multiplier))result=-EINVAL;
  if(!result){g->buffer=tmc_hid_alloc(HID_MAX_BUFFER_SIZE+8,1);if(!g->buffer)result=-ENOMEM;}
  if(!result){d->dev_rdesc=(void *)0;d->dev_rsize=0;__atomic_add_fetch(&tmc_hid_generic_counters.attached,1,__ATOMIC_RELAXED);__atomic_add_fetch(&stats.opens,1,__ATOMIC_RELAXED);generic_unlock();return g;}
  if(g->started){hidinput_disconnect(d);g->ops.stop(context);g->started=0;}
  hid_close_report(d);tmc_hid_free(p);
 }
 tmc_hid_free(g);__atomic_add_fetch(&tmc_hid_generic_counters.failures,1,__ATOMIC_RELAXED);__atomic_add_fetch(&stats.failures,1,__ATOMIC_RELAXED);generic_unlock();return (void *)0;
}
int tmc_hid_generic_report(TmcHidGeneric *g,const u8 *bytes,unsigned n)
{
 if(!g||!generic_lock())return -EBUSY;TmcHidReportInfo info;int result=-EINVAL;
 __atomic_add_fetch(&tmc_hid_generic_counters.reports,1,__ATOMIC_RELAXED);
 if(tmc_linux_hid_frame(g->parser,0,bytes,n,&info)){
  struct hid_device *d=&g->parser->device;struct hid_report *r=d->report_enum[0].report_id_hash[info.id];
  if(hid_match_report(d,r)){memcpy(g->buffer,bytes,n);hid_process_report(d,r,g->buffer+info.numbered,1);hidinput_report_event(d,r);result=0;}
 }
 if(result)__atomic_add_fetch(&tmc_hid_generic_counters.rejected,1,__ATOMIC_RELAXED);generic_unlock();return result;
}
int tmc_hid_generic_set_led(TmcHidGeneric *g,unsigned code,int on)
{
 if(!g||code>=LED_CNT)return -EINVAL;if(!generic_lock())return -EBUSY;
 struct hid_device *d=&g->parser->device;struct hid_input *h;int result=-EINVAL;
 list_for_each_entry(h,&d->inputs,list)if(test_bit(code,h->input->ledbit)){
  if(on)set_bit(code,h->input->led);else clear_bit(code,h->input->led);
  result=h->input->event(h->input,EV_LED,code,!!on);if(result)break;
 }
 generic_unlock();return result;
}
int tmc_hid_generic_service(TmcHidGeneric *g)
{
 if(!g||!generic_lock())return -EBUSY;struct work_struct *w=&g->parser->device.led_work;
 if(w->queued){w->queued=0;w->function(w);}generic_unlock();return 0;
}
int tmc_hid_generic_detach(TmcHidGeneric *g)
{
 if(!g||!generic_lock())return -EBUSY;
 hidinput_disconnect(&g->parser->device);if(g->started)g->ops.stop(g->context);
 tmc_hid_free(g->buffer);hid_close_report(&g->parser->device);tmc_hid_free(g->parser);tmc_hid_free(g);
 __atomic_add_fetch(&tmc_hid_generic_counters.detached,1,__ATOMIC_RELAXED);__atomic_add_fetch(&stats.closes,1,__ATOMIC_RELAXED);generic_unlock();return 0;
}
void tmc_hid_generic_stats(TmcHidGenericStats *out)
{
 if(!out)return;
 #define READ(f) out->f=__atomic_load_n(&tmc_hid_generic_counters.f,__ATOMIC_RELAXED)
 READ(probes);READ(attached);READ(detached);READ(failures);READ(reports);READ(rejected);READ(events);READ(inputs);READ(outputs);READ(output_errors);READ(busy);
 #undef READ
}

int tmc_hid_generic_quiesce(TmcHidGeneric *g)
{
 if(!g||!generic_lock())return -EBUSY;
 struct hid_device *d=&g->parser->device;struct hid_input *h;
 list_for_each_entry(h,&d->inputs,list){
  for(unsigned k=0;k<KEY_CNT;k++)if(test_bit(k,h->input->key))input_event(h->input,EV_KEY,k,0);
  input_sync(h->input);
 }
 for(unsigned id=0;id<HID_MAX_IDS;id++){
  struct hid_report *r=d->report_enum[HID_INPUT_REPORT].report_id_hash[id];if(!r)continue;
  for(unsigned i=0;i<r->maxfield;i++){struct hid_field *f=r->field[i];memset(f->value,0,f->report_count*sizeof(s32));memset(f->new_value,0,f->report_count*sizeof(s32));}
 }
 generic_unlock();return 0;
}

int tmc_hid_generic_input_bytes(TmcHidGeneric *g)
{
 if(!g||!generic_lock())return -EBUSY;
 struct hid_report_enum *e=&g->parser->device.report_enum[HID_INPUT_REPORT];unsigned maximum=0;
 for(unsigned id=0;id<HID_MAX_IDS;id++){
  struct hid_report *r=e->report_id_hash[id];if(r)maximum=MAX(maximum,(r->size+7)/8+!!e->numbered);
 }
 generic_unlock();return maximum;
}
