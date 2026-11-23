/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <linux/tmc_hid_parser.h>
/* All upstream functions below are byte-exact, individually manifest-checked.
 * No device/bus/input/IRQ API is replaced with a fake-success implementation. */
static TmcHidParserStats tmc_linux_hid_parser_counters;
#define stats tmc_linux_hid_parser_counters
static unsigned parsing,allocation_failed;
#define HEAP_LIMIT (2u*1024*1024)
typedef struct {size_t bytes;u64 reserved;} Allocation;
void *tmc_hid_alloc(size_t n,int zero)
{
 if(!n||n>512*1024){allocation_failed=1;return (void *)0;}
 size_t bytes=n+sizeof(Allocation);
 u64 live=__atomic_add_fetch(&stats.live_bytes,bytes,__ATOMIC_RELAXED);
 if(live>HEAP_LIMIT){__atomic_sub_fetch(&stats.live_bytes,bytes,__ATOMIC_RELAXED);allocation_failed=1;return (void *)0;}
 Allocation *a=tmc_hid_os_alloc(bytes);
 if(!a){__atomic_sub_fetch(&stats.live_bytes,bytes,__ATOMIC_RELAXED);allocation_failed=1;return (void *)0;}
 a->bytes=bytes;a->reserved=0;
 __atomic_add_fetch(&stats.allocations,1,__ATOMIC_RELAXED);
 u64 peak=__atomic_load_n(&stats.peak_bytes,__ATOMIC_RELAXED);
 while(live>peak&&!__atomic_compare_exchange_n(&stats.peak_bytes,&peak,live,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED)){}
 if(zero)memset(a+1,0,n);return a+1;
}
void tmc_hid_free(void *p)
{
 if(!p)return;Allocation *a=(Allocation *)p-1;size_t bytes=a->bytes;
 tmc_hid_os_free(a);__atomic_sub_fetch(&stats.live_bytes,bytes,__ATOMIC_RELAXED);
 __atomic_add_fetch(&stats.frees,1,__ATOMIC_RELAXED);
}
void *tmc_hid_realloc(void *p,size_t n)
{
 if(!p)return tmc_hid_alloc(n,0);
 if(!n){tmc_hid_free(p);return (void *)0;}
 void *q=tmc_hid_alloc(n,0);if(!q)return (void *)0;
 memcpy(q,p,MIN(n,((Allocation *)p-1)->bytes-sizeof(Allocation)));tmc_hid_free(p);return q;
}
void tmc_hid_notice(void){__atomic_add_fetch(&stats.notices,1,__ATOMIC_RELAXED);}
#include "../../vendor/linux-v6.12/hid-parser-signed.inc"
#include "../../vendor/linux-v6.12/hid-parser-fields.inc"
#include "../../vendor/linux-v6.12/hid-parser-items.inc"
#include "../../vendor/linux-v6.12/hid-parser-multipliers.inc"
static unsigned invalid_multiplier;
static void checked_resolution_multiplier(struct hid_device *d)
{
 /* hid_open_report initializes feature values to zero. Validate the exact
  * signed arithmetic BEFORE upstream multiplier evaluation: untrusted ranges
  * must not overflow or trigger INT_MIN/-1. No upstream source edits. */
 for(unsigned id=0;id<HID_MAX_IDS;id++){
  struct hid_report *r=d->report_enum[HID_FEATURE_REPORT].report_id_hash[id];if(!r)continue;
  for(unsigned i=0;i<r->maxfield;i++){
   struct hid_field *f=r->field[i];if(!f->report_count)continue;
   int multiplier=0;for(unsigned u=0;u<f->maxusage;u++)if(f->usage[u].hid==HID_GD_RESOLUTION_MULTIPLIER)multiplier=1;
   if(!multiplier)continue;
   i64 den=(i64)f->logical_maximum-f->logical_minimum;
   if(!den)continue;
   i64 num=(i64)*f->value-f->logical_minimum,span=(i64)f->physical_maximum-f->physical_minimum;
   if(den<INT32_MIN||den>INT32_MAX||num<INT32_MIN||num>INT32_MAX||span<INT32_MIN||span>INT32_MAX||(num==INT32_MIN&&den==-1)){
    invalid_multiplier=1;return;
   }
   i64 product=(num/den)*span,result=product+f->physical_minimum;
   if(product<INT32_MIN||product>INT32_MAX||result<INT32_MIN||result>INT32_MAX){invalid_multiplier=1;return;}
  }
 }
 hid_setup_resolution_multiplier(d);
}
#define hid_setup_resolution_multiplier checked_resolution_multiplier
#include "../../vendor/linux-v6.12/hid-parser-open.inc"
#undef hid_setup_resolution_multiplier
struct TmcLinuxHidParser {
 struct hid_device device;
 struct hid_driver driver;
 struct hid_ll_driver low;
};
TmcLinuxHidParser *tmc_linux_hid_parse(const u8 *desc,unsigned n)
{
 if(!desc||!n||n>1024){__atomic_add_fetch(&stats.failures,1,__ATOMIC_RELAXED);return (void *)0;}
 /* Enumeration is normally serialized. A concurrent caller is rejected, never
  * spun on with interrupts disabled. Readers/close of distinct owned objects
  * do not take this lock. Allocation-failure state is scoped to this parse. */
 if(__atomic_test_and_set(&parsing,__ATOMIC_ACQUIRE)){
  __atomic_add_fetch(&stats.busy,1,__ATOMIC_RELAXED);return (void *)0;
 }
 allocation_failed=invalid_multiplier=0;
 TmcLinuxHidParser *p=tmc_hid_alloc(sizeof(*p),1);
 if(p){
  p->device.driver=&p->driver;p->device.ll_driver=&p->low;p->device.dev.named=1;
  p->device.dev_rdesc=desc;p->device.dev_rsize=n;
  for(unsigned i=0;i<HID_REPORT_TYPES;i++)INIT_LIST_HEAD(&p->device.report_enum[i].report_list);
  int result=hid_open_report(&p->device);
  /* Upstream hid_add_field can ignore failed field allocation. A bounded OS
   * adapter must not claim that this is a complete successful descriptor. */
  int incomplete=allocation_failed||invalid_multiplier;
  for(unsigned t=0;t<HID_REPORT_TYPES;t++)for(unsigned id=0;id<HID_MAX_IDS;id++){
   struct hid_report *r=p->device.report_enum[t].report_id_hash[id];
   if(r&&r->maxfield==HID_MAX_FIELDS)incomplete=1;
  }
  if(result||incomplete){hid_close_report(&p->device);tmc_hid_free(p);p=(void *)0;}
  else {p->device.dev_rdesc=(void *)0;p->device.dev_rsize=0;}
 }
 if(p)__atomic_add_fetch(&stats.opens,1,__ATOMIC_RELAXED);
 else __atomic_add_fetch(&stats.failures,1,__ATOMIC_RELAXED);
 __atomic_clear(&parsing,__ATOMIC_RELEASE);return p;
}
void tmc_linux_hid_close(TmcLinuxHidParser *p)
{
 if(!p)return;hid_close_report(&p->device);tmc_hid_free(p);__atomic_add_fetch(&stats.closes,1,__ATOMIC_RELAXED);
}
static const struct hid_report *lookup(const TmcLinuxHidParser *p,unsigned type,unsigned id)
{
 return p&&type<HID_REPORT_TYPES&&id<HID_MAX_IDS?p->device.report_enum[type].report_id_hash[id]:(void *)0;
}
int tmc_linux_hid_report(const TmcLinuxHidParser *p,unsigned type,unsigned id,TmcHidReportInfo *out)
{
 const struct hid_report *r=lookup(p,type,id);if(!r||!out)return 0;
 unsigned numbered=p->device.report_enum[type].numbered;
 *out=(TmcHidReportInfo){r->id,r->type,r->size,(r->size+7)/8+numbered,r->maxfield,r->application,numbered};return 1;
}
int tmc_linux_hid_field(const TmcLinuxHidParser *p,unsigned type,unsigned id,unsigned index,TmcHidFieldInfo *out)
{
 const struct hid_report *r=lookup(p,type,id);if(!r||!out||index>=r->maxfield)return 0;
 const struct hid_field *f=r->field[index];
 *out=(TmcHidFieldInfo){f->application,f->physical,f->logical,f->flags,f->report_offset,f->report_size,f->report_count,f->maxusage,f->logical_minimum,f->logical_maximum,f->physical_minimum,f->physical_maximum,f->unit_exponent,f->unit};return 1;
}
int tmc_linux_hid_usage(const TmcLinuxHidParser *p,unsigned type,unsigned id,unsigned field,unsigned index,u32 *out)
{
 const struct hid_report *r=lookup(p,type,id);if(!r||!out||field>=r->maxfield||index>=r->field[field]->maxusage)return 0;
 *out=r->field[field]->usage[index].hid;return 1;
}
int tmc_linux_hid_frame(const TmcLinuxHidParser *p,unsigned type,const u8 *bytes,unsigned n,TmcHidReportInfo *out)
{
 if(!p||!bytes||!n||!out||type>=HID_REPORT_TYPES)return 0;
 unsigned numbered=p->device.report_enum[type].numbered,id=numbered?bytes[0]:0;
 TmcHidReportInfo r;
 /* Mixed unnumbered/numbered IDs are ambiguous at this transport boundary. */
 if((numbered&&!id)||!tmc_linux_hid_report(p,type,id,&r)||n<r.bytes||n>HID_MAX_BUFFER_SIZE)return 0;
 *out=r;return 1;
}
static int bind(TabletField *out,const struct hid_field *f,unsigned index,unsigned numbered)
{
 if(out->width||!f->report_size||f->report_size>16||f->logical_minimum>=f->logical_maximum)return 0;
 int limit=1<<(f->report_size-(f->logical_minimum<0));
 if(f->logical_minimum < -limit||f->logical_maximum>=limit)return 0;
 *out=(TabletField){f->report_offset+index*f->report_size+8*numbered,f->report_size,f->logical_minimum,f->logical_maximum};return 1;
}
int tmc_linux_hid_pointer_layout(const u8 *desc,unsigned n,unsigned packet,TabletLayout *out)
{
 if(!out||!packet||packet>64)return 0;
 TmcLinuxHidParser *p=tmc_linux_hid_parse(desc,n);if(!p)return 0;
 int found=0;struct hid_report_enum *e=&p->device.report_enum[HID_INPUT_REPORT];
 struct hid_report *r;
 list_for_each_entry(r,&e->report_list,list){
  TabletLayout t={.id=r->id,.bytes=(r->size+7)/8+e->numbered};int mode=-1,valid=1;
  if((e->numbered&&!r->id)||!t.bytes||t.bytes>packet)continue;
  for(unsigned i=0;i<r->maxfield&&valid;i++){
   const struct hid_field *f=r->field[i];
   if((f->application!=HID_GD_MOUSE&&f->application!=HID_GD_POINTER)||
       (f->flags&HID_MAIN_ITEM_CONSTANT)||!(f->flags&HID_MAIN_ITEM_VARIABLE))continue;
   for(unsigned j=0;j<f->report_count&&j<f->maxusage;j++){
    u32 usage=f->usage[j].hid;int relative=!!(f->flags&HID_MAIN_ITEM_RELATIVE);TabletField *dest=(void *)0;
    if(usage==HID_GD_X||usage==HID_GD_Y){
     if((mode>=0&&mode!=relative)||(relative&&f->logical_minimum>=0)){valid=0;break;}
     mode=relative;t.relative=relative;dest=usage==HID_GD_X?&t.x:&t.y;
    }else if(usage==HID_GD_WHEEL&&relative)dest=&t.wheel;
    else if((usage&HID_USAGE_PAGE)==HID_UP_BUTTON&&!relative&&(usage&65535)>=1&&(usage&65535)<=3&&f->report_size==1)dest=&t.button[(usage&65535)-1];
    if(dest&&!bind(dest,f,j,e->numbered)){valid=0;break;}
   }
  }
  if(valid&&t.x.width&&t.y.width&&t.button[0].width){*out=t;found=1;break;}
 }
 tmc_linux_hid_close(p);return found;
}
void tmc_linux_hid_parser_stats(TmcHidParserStats *out)
{
 if(!out)return;
 #define READ(f) out->f=__atomic_load_n(&stats.f,__ATOMIC_RELAXED)
 READ(opens);READ(failures);READ(closes);READ(allocations);READ(frees);READ(live_bytes);READ(peak_bytes);READ(notices);READ(busy);
 #undef READ
}

#include "hid_generic_runtime.inc"
