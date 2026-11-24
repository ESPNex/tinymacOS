/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <tmc_kernel.h>
#include "/home/user/tinymacOS/vendor/linux-v6.12/input-event-codes.h"
#define BUS_USB 3
#define BIT_MASK(n) BIT((n)%BITS_PER_LONG)
#define BIT_WORD(n) ((n)/BITS_PER_LONG)
#define USB_INTERFACE_CLASS_HID 3
#define USB_INTERFACE_SUBCLASS_BOOT 1
#define USB_INTERFACE_PROTOCOL_KEYBOARD 1
#define USB_INTERFACE_PROTOCOL_MOUSE 2
struct input_id {u16 bustype,vendor,product,version;};
#define BUS_BLUETOOTH 5
#define BUS_VIRTUAL 6
#define BUS_I2C 0x18
#define BUS_INTEL_ISHTP 0x1f
#define BUS_AMD_SFH 0x20
#define FF_CNT 128
#define INPUT_KEYMAP_BY_INDEX 1
struct input_keymap_entry {u8 flags,len;u16 index;u32 keycode;u8 scancode[32];};
struct input_absinfo {int value,minimum,maximum,fuzz,flat,resolution;};
struct input_dev {unsigned long propbit[BITS_TO_LONGS(INPUT_PROP_CNT)],sndbit[BITS_TO_LONGS(SND_CNT)],ffbit[BITS_TO_LONGS(FF_CNT)],swbit[BITS_TO_LONGS(SW_CNT)];struct input_absinfo absinfo[ABS_CNT];const char *uniq;unsigned hint_events_per_packet;int (*setkeycode)(struct input_dev *,const struct input_keymap_entry *,unsigned *);int (*getkeycode)(struct input_dev *,struct input_keymap_entry *);unsigned long absbit[BITS_TO_LONGS(ABS_CNT)],mscbit[BITS_TO_LONGS(MSC_CNT)];const char *name,*phys;struct input_id id;struct device dev;unsigned long evbit[BITS_TO_LONGS(EV_CNT)],keybit[BITS_TO_LONGS(KEY_CNT)],relbit[BITS_TO_LONGS(REL_CNT)],ledbit[BITS_TO_LONGS(LED_CNT)],led[BITS_TO_LONGS(LED_CNT)],key[BITS_TO_LONGS(KEY_CNT)];void *driver_data;int (*open)(struct input_dev *);void (*close)(struct input_dev *);int (*event)(struct input_dev *,unsigned,unsigned,int);struct list_head registered_entry;unsigned registered;int x,y,wheel,hwheel;unsigned buttons;};
struct input_dev *input_allocate_device(void);void input_free_device(struct input_dev *);
int input_register_device(struct input_dev *);void input_unregister_device(struct input_dev *);
void input_report_key(struct input_dev *,unsigned,int);void input_report_rel(struct input_dev *,unsigned,int);void input_sync(struct input_dev *);
static inline void input_set_drvdata(struct input_dev *d,void *p){d->driver_data=p;}
static inline void *input_get_drvdata(struct input_dev *d){return d->driver_data;}
void *memscan(void *,int,size_t);
#define __set_bit set_bit
static inline void input_set_events_per_packet(struct input_dev *i,unsigned n){i->hint_events_per_packet=n;}
static inline void input_set_abs_params(struct input_dev *i,unsigned c,int a,int b,int f,int flat){if(c<ABS_CNT){i->absinfo[c]=(struct input_absinfo){0,a,b,f,flat,0};set_bit(c,i->absbit);}}
static inline void input_abs_set_res(struct input_dev *i,unsigned c,int v){if(c<ABS_CNT)i->absinfo[c].resolution=v;}
static inline int input_scancode_to_scalar(const struct input_keymap_entry *k,unsigned *out){if(k->len!=1&&k->len!=2&&k->len!=4)return -EINVAL;*out=0;for(unsigned n=0;n<k->len;n++)*out|=(u32)k->scancode[n]<<(8*n);return 0;}
int abs(int);
void input_event(struct input_dev *,unsigned,unsigned,int);
static inline int input_ff_event(struct input_dev *i,unsigned t,unsigned c,int v){return -EOPNOTSUPP;}
