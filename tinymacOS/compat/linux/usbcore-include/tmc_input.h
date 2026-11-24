/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <tmc_kernel.h>
#include "../../../vendor/linux-v6.12/input-event-codes.h"
#define BUS_USB 3
#define BIT_MASK(n) BIT((n)%BITS_PER_LONG)
#define BIT_WORD(n) ((n)/BITS_PER_LONG)
#define USB_INTERFACE_CLASS_HID 3
#define USB_INTERFACE_SUBCLASS_BOOT 1
#define USB_INTERFACE_PROTOCOL_KEYBOARD 1
#define USB_INTERFACE_PROTOCOL_MOUSE 2
struct input_id {u16 bustype,vendor,product,version;};
struct input_dev {const char *name,*phys;struct input_id id;struct device dev;unsigned long evbit[BITS_TO_LONGS(EV_CNT)],keybit[BITS_TO_LONGS(KEY_CNT)],relbit[BITS_TO_LONGS(REL_CNT)],ledbit[BITS_TO_LONGS(LED_CNT)],led[BITS_TO_LONGS(LED_CNT)],key[BITS_TO_LONGS(KEY_CNT)];void *driver_data;int (*open)(struct input_dev *);void (*close)(struct input_dev *);int (*event)(struct input_dev *,unsigned,unsigned,int);struct list_head registered_entry;unsigned registered;int x,y,wheel,hwheel;unsigned buttons;};
struct input_dev *input_allocate_device(void);void input_free_device(struct input_dev *);
int input_register_device(struct input_dev *);void input_unregister_device(struct input_dev *);
void input_report_key(struct input_dev *,unsigned,int);void input_report_rel(struct input_dev *,unsigned,int);void input_sync(struct input_dev *);
static inline void input_set_drvdata(struct input_dev *d,void *p){d->driver_data=p;}
static inline void *input_get_drvdata(struct input_dev *d){return d->driver_data;}
#define hid_info(d,...) dev_info(d,__VA_ARGS__)
#define hid_err(d,...) dev_err(d,__VA_ARGS__)
void *memscan(void *,int,size_t);
#define hid_warn(d,...) dev_warn(d,__VA_ARGS__)
