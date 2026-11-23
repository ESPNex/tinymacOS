/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* Private executable input/bus adaptation for original hid-input/generic.
 * Not the general Linux input core or a USB controller implementation. */
#include <linux/tmc_hid_parser.h>
#include "../../../../vendor/linux-v6.12/input-event-codes.h"
#include "linux_hid_generic.h"
#define ARRAY_SIZE COUNT
#define BITS_TO_LONGS(n) (((n)+63)/64)
#define DIV_ROUND_CLOSEST(a,b) (((a)+(b)/2)/(b))
#define clamp(x,a,b) MIN(MAX(x,a),b)
#define fallthrough __attribute__((fallthrough))
#define ENOSYS 38
#define EOPNOTSUPP 95
#define GFP_ATOMIC 1
typedef unsigned gfp_t;
#define HID_BUS_ANY 0xffff
#define HID_GROUP_ANY 0
#define HID_ANY_ID (~0u)
#define HID_TERMINATOR (HID_ANY_ID-1)
#define HID_REQ_GET_REPORT 1
#define HID_REQ_SET_REPORT 9
#define HID_DEVICE(b,g,v,p) .bus=(b),.group=(g),.vendor=(v),.product=(p)
#define MODULE_DEVICE_TABLE(t,x)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_LICENSE(x)
#define module_hid_driver(x) static struct hid_driver *tmc_generic_driver(void){return &(x);}
#define to_hid_driver(p) container_of(p,struct hid_driver,driver)
#define pr_warn_ratelimited(...) tmc_hid_notice()
#define hid_dbg(d,...) ((void)0)
#define hid_warn_once(d,...) tmc_hid_notice()
#define hid_dump_input(d,u,v) ((void)0)
#define IS_INPUT_APPLICATION(a) (((a>=HID_UP_GENDESK)&&(a<=HID_GD_MULTIAXIS))||((a>=HID_DG_DIGITIZER)&&(a<=HID_DG_WHITEBOARD))||(a==HID_GD_SYSTEM_CONTROL)||(a==HID_CP_CONSUMER_CONTROL)||(a==HID_GD_WIRELESS_RADIO_CTLS))
#define FF_CNT 128
#define input_allocate_device tmc_hid_input_allocate_device
#define input_free_device tmc_hid_input_free_device
#define input_register_device tmc_hid_input_register_device
#define input_unregister_device tmc_hid_input_unregister_device
#define input_event tmc_hid_input_event
#define input_sync tmc_hid_input_sync
#define strcmp tmc_hid_strcmp
#define abs tmc_hid_abs
struct hid_report_id {u32 report_type;};
struct hid_usage_id {u32 usage_hid,usage_type,usage_code;};
struct input_absinfo {int value,minimum,maximum,fuzz,flat,resolution;};
struct input_keymap_entry {u8 flags,len;u16 index;u32 keycode;u8 scancode[32];};
#define INPUT_KEYMAP_BY_INDEX 1
struct input_dev {
 const char *name,*phys,*uniq;struct device dev;
 struct {unsigned bustype,vendor,product,version;} id;
 unsigned long evbit[BITS_TO_LONGS(EV_CNT)],keybit[BITS_TO_LONGS(KEY_CNT)],key[BITS_TO_LONGS(KEY_CNT)];
 unsigned long relbit[BITS_TO_LONGS(REL_CNT)],absbit[BITS_TO_LONGS(ABS_CNT)],mscbit[BITS_TO_LONGS(MSC_CNT)],ledbit[BITS_TO_LONGS(LED_CNT)],led[BITS_TO_LONGS(LED_CNT)];
 unsigned long sndbit[BITS_TO_LONGS(SND_CNT)],ffbit[BITS_TO_LONGS(FF_CNT)],swbit[BITS_TO_LONGS(SW_CNT)],propbit[BITS_TO_LONGS(INPUT_PROP_CNT)];
 struct input_absinfo absinfo[ABS_CNT];
 int (*event)(struct input_dev *,unsigned,unsigned,int);
 int (*open)(struct input_dev *);void (*close)(struct input_dev *);
 int (*setkeycode)(struct input_dev *,const struct input_keymap_entry *,unsigned *);
 int (*getkeycode)(struct input_dev *,struct input_keymap_entry *);
 unsigned hint_events_per_packet;void *drvdata;unsigned registered,token;
};
static inline int test_bit(unsigned n,const unsigned long *b){return !!(b[n/64]&(1ul<<(n%64)));}
static inline void set_bit(unsigned n,unsigned long *b){b[n/64]|=1ul<<(n%64);}
#define __set_bit set_bit
static inline void clear_bit(unsigned n,unsigned long *b){b[n/64]&=~(1ul<<(n%64));}
static inline int test_and_set_bit(unsigned n,unsigned long *b){int v=test_bit(n,b);set_bit(n,b);return v;}
static inline unsigned find_next_zero_bit(const unsigned long *b,unsigned end,unsigned n){while(n<end&&test_bit(n,b))n++;return n;}
static inline int list_empty(const struct list_head *h){return h->next==h;}
static inline void list_del(struct list_head *n){n->next->prev=n->prev;n->prev->next=n->next;n->next=n->prev=n;}
#define list_for_each_entry_safe(p,n,h,m) for(p=container_of((h)->next,__typeof__(*p),m),n=container_of(p->m.next,__typeof__(*p),m);&p->m!=(h);p=n,n=container_of(n->m.next,__typeof__(*n),m))
static inline void input_set_drvdata(struct input_dev *i,void *p){i->drvdata=p;}
static inline void *input_get_drvdata(struct input_dev *i){return i->drvdata;}
static inline void input_set_events_per_packet(struct input_dev *i,unsigned n){i->hint_events_per_packet=n;}
static inline void input_set_abs_params(struct input_dev *i,unsigned c,int a,int b,int f,int flat){if(c<ABS_CNT){i->absinfo[c]=(struct input_absinfo){0,a,b,f,flat,0};set_bit(c,i->absbit);}}
static inline void input_abs_set_res(struct input_dev *i,unsigned c,int v){if(c<ABS_CNT)i->absinfo[c].resolution=v;}
static inline int input_scancode_to_scalar(const struct input_keymap_entry *k,unsigned *out){if(k->len!=1&&k->len!=2&&k->len!=4)return -EINVAL;*out=0;for(unsigned n=0;n<k->len;n++)*out|=(u32)k->scancode[n]<<(8*n);return 0;}
struct input_dev *input_allocate_device(void);
void input_free_device(struct input_dev *);
int input_register_device(struct input_dev *);
void input_unregister_device(struct input_dev *);
void input_event(struct input_dev *,unsigned,unsigned,int);
static inline void input_sync(struct input_dev *i){input_event(i,EV_SYN,SYN_REPORT,0);}
static inline int input_ff_event(struct input_dev *i,unsigned t,unsigned c,int v){return -EOPNOTSUPP;}
int strcmp(const char *,const char *);
int abs(int);
char *kasprintf(unsigned,const char *,...);
#define INIT_WORK(w,f) do{(w)->function=f;(w)->queued=0;}while(0)
static inline int schedule_work(struct work_struct *w){int old=w->queued;w->queued=1;return !old;}
static inline int cancel_work_sync(struct work_struct *w){int old=w->queued;w->queued=0;return old;}
static inline unsigned hid_report_len(struct hid_report *r){return (r->size+7)/8+(r->id>0);}
void hidinput_hid_event(struct hid_device *,struct hid_field *,struct hid_usage *,s32);
void hidinput_report_event(struct hid_device *,struct hid_report *);
int hidinput_connect(struct hid_device *,unsigned);
void hidinput_disconnect(struct hid_device *);
int hid_set_field(struct hid_field *,unsigned,s32);
u8 *hid_alloc_report_buf(struct hid_report *,unsigned);
void hid_output_report(struct hid_report *,u8 *);
int hid_hw_start(struct hid_device *,unsigned);
int hid_hw_open(struct hid_device *);
void hid_hw_close(struct hid_device *);
int hid_hw_output_report(struct hid_device *,u8 *,size_t);
int hid_hw_raw_request(struct hid_device *,unsigned,u8 *,size_t,unsigned,unsigned);
int __hid_request(struct hid_device *,struct hid_report *,int);
void hid_hw_request(struct hid_device *,struct hid_report *,int);
void hid_hw_wait(struct hid_device *);
static inline int hid_parse(struct hid_device *d){return hid_open_report(d);}
struct bus_type {struct hid_driver *drivers[8];unsigned count;};
extern struct bus_type hid_bus_type;
int bus_for_each_drv(struct bus_type *,struct device_driver *,void *,int (*)(struct device_driver *,void *));
const struct hid_device_id *hid_match_device(struct hid_device *,struct hid_driver *);

void hid_setup_resolution_multiplier(struct hid_device *);
static inline void hid_map_usage(struct hid_input *hidinput,
		struct hid_usage *usage, unsigned long **bit, int *max,
		__u8 type, unsigned int c)
{
	struct input_dev *input = hidinput->input;
	unsigned long *bmap = NULL;
	unsigned int limit = 0;

	switch (type) {
	case EV_ABS:
		bmap = input->absbit;
		limit = ABS_MAX;
		break;
	case EV_REL:
		bmap = input->relbit;
		limit = REL_MAX;
		break;
	case EV_KEY:
		bmap = input->keybit;
		limit = KEY_MAX;
		break;
	case EV_LED:
		bmap = input->ledbit;
		limit = LED_MAX;
		break;
	case EV_MSC:
		bmap = input->mscbit;
		limit = MSC_MAX;
		break;
	}

	if (unlikely(c > limit || !bmap)) {
		pr_warn_ratelimited("%s: Invalid code %d type %d\n",
				    input->name, c, type);
		*bit = NULL;
		return;
	}

	usage->type = type;
	usage->code = c;
	*max = limit;
	*bit = bmap;
}

/**
 * hid_map_usage_clear - map usage input bits and clear the input bit
 *
 * @hidinput: hidinput which we are interested in
 * @usage: usage to fill in
 * @bit: pointer to input->{}bit (out parameter)
 * @max: maximal valid usage->code to consider later (out parameter)
 * @type: input event type (EV_KEY, EV_REL, ...)
 * @c: code which corresponds to this usage and type
 *
 * The same as hid_map_usage, except the @c bit is also cleared in supported
 * bits (@bit).
 */
static inline void hid_map_usage_clear(struct hid_input *hidinput,
		struct hid_usage *usage, unsigned long **bit, int *max,
		__u8 type, __u16 c)
{
	hid_map_usage(hidinput, usage, bit, max, type, c);
	if (*bit)
		clear_bit(usage->code, *bit);
}
