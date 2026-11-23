/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* The implemented Linux USB/input API subset for usbmouse.c, NOT a set of
 * empty replacements for general Linux kernel services. Runtime definitions
 * are in usb_input.c; the native kernel supplies allocation, locking/events.
 */
#include "base.h"
#include <stdarg.h>
typedef u64 dma_addr_t;
typedef unsigned gfp_t;
#define GFP_KERNEL 0
#define GFP_ATOMIC 1
#define ENOENT 2
#define EIO 5
#define ENOMEM 12
#define EBUSY 16
#define ENODEV 19
#define EINVAL 22
#define EPIPE 32
#define ENOSPC 28
#define ESHUTDOWN 108
#define ECONNRESET 104
#define URB_NO_TRANSFER_DMA_MAP 4
#define EV_KEY 1
#define EV_REL 2
#define BTN_MOUSE 0x110
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112
#define BTN_SIDE 0x113
#define BTN_EXTRA 0x114
#define REL_X 0
#define REL_Y 1
#define REL_WHEEL 8
#define BIT_WORD(x) ((unsigned)(x)/64)
#define BIT_MASK(x) (1UL<<((unsigned)(x)%64))
#define USB_INTERFACE_CLASS_HID 3
#define USB_INTERFACE_SUBCLASS_BOOT 1
#define USB_INTERFACE_PROTOCOL_MOUSE 2
#define USB_INTERFACE_INFO(c,s,p) .cls=(c),.sub=(s),.proto=(p)
/* These are static module metadata, not unimplemented runtime operations. */
#define MODULE_AUTHOR(x) static const char tmc_lx_author[] __attribute__((used))=(x)
#define MODULE_DESCRIPTION(x) static const char tmc_lx_description[] __attribute__((used))=(x)
#define MODULE_LICENSE(x) static const char tmc_lx_license[] __attribute__((used))=(x)
#define MODULE_DEVICE_TABLE(kind,table)
#define module_usb_driver(driver) struct usb_driver *tmc_lx_usbmouse_driver(void){return &(driver);}
#define le16_to_cpu(x) (x) /* supported kernel target is little-endian x86-64 */
struct device {struct device *parent;};
struct input_id {u16 bustype,vendor,product,version;};
struct input_dev {
 const char *name,*phys;struct input_id id;struct device dev;
 unsigned long evbit[1],keybit[16],relbit[1];
 void *driver_data;int (*open)(struct input_dev *);void (*close)(struct input_dev *);
 int registered,x,y,wheel,buttons;void *owner;
};
struct usb_bus {const char *bus_name;};
struct usb_device {
 struct device dev;struct usb_bus *bus;const char *manufacturer,*product,*devpath;
 struct {u16 idVendor,idProduct;} descriptor;unsigned maxpacket;void *owner;
};
struct usb_endpoint_descriptor {u8 bLength,bDescriptorType,bEndpointAddress,bmAttributes;u16 wMaxPacketSize;u8 bInterval;} PACK;
struct usb_host_endpoint {struct usb_endpoint_descriptor desc;};
struct usb_host_interface {struct {u8 bNumEndpoints;} desc;struct usb_host_endpoint *endpoint;};
struct usb_interface {struct device dev;struct usb_device *usbdev;struct usb_host_interface *cur_altsetting;void *driver_data;};
struct usb_device_id {u8 cls,sub,proto;};
struct urb {
 struct usb_device *dev;void *context,*transfer_buffer;int status,actual_length,transfer_buffer_length;
 dma_addr_t transfer_dma;unsigned transfer_flags,pipe,interval;void (*complete)(struct urb *);
 int submitted,killed;
};
struct usb_driver {const char *name;int (*probe)(struct usb_interface *,const struct usb_device_id *);void (*disconnect)(struct usb_interface *);const struct usb_device_id *id_table;};
void *tmc_lx_zalloc(size_t,gfp_t);
void tmc_lx_free(void *);
#define kzalloc tmc_lx_zalloc
#define kfree tmc_lx_free
long tmc_lx_strscpy(char *,const char *,size_t);
size_t tmc_lx_strlcat(char *,const char *,size_t);
int tmc_lx_snprintf(char *,size_t,const char *,...);
#define strscpy tmc_lx_strscpy
#define strlcat tmc_lx_strlcat
#define snprintf tmc_lx_snprintf
void tmc_lx_dev_err(struct device *,const char *,...);
#define dev_err tmc_lx_dev_err
struct input_dev *input_allocate_device(void);
void input_free_device(struct input_dev *);
int input_register_device(struct input_dev *);
void input_unregister_device(struct input_dev *);
void input_report_key(struct input_dev *,unsigned,int);
void input_report_rel(struct input_dev *,unsigned,int);
void input_sync(struct input_dev *);
static inline void input_set_drvdata(struct input_dev *d,void *p){d->driver_data=p;}
static inline void *input_get_drvdata(struct input_dev *d){return d->driver_data;}
static inline struct usb_device *interface_to_usbdev(struct usb_interface *i){return i->usbdev;}
static inline void usb_set_intfdata(struct usb_interface *i,void *p){i->driver_data=p;}
static inline void *usb_get_intfdata(struct usb_interface *i){return i->driver_data;}
static inline int usb_endpoint_is_int_in(struct usb_endpoint_descriptor *e){return (e->bmAttributes&3)==3&&(e->bEndpointAddress&128);}
static inline unsigned usb_rcvintpipe(struct usb_device *d,unsigned ep){(void)d;return ep;}
static inline unsigned usb_maxpacket(struct usb_device *d,unsigned pipe){(void)pipe;return d->maxpacket;}
void usb_to_input_id(struct usb_device *,struct input_id *);
int usb_make_path(struct usb_device *,char *,size_t);
void *usb_alloc_coherent(struct usb_device *,size_t,gfp_t,dma_addr_t *);
void usb_free_coherent(struct usb_device *,size_t,void *,dma_addr_t);
struct urb *usb_alloc_urb(int,gfp_t);
void usb_free_urb(struct urb *);
int usb_submit_urb(struct urb *,gfp_t);
void usb_kill_urb(struct urb *);
void usb_fill_int_urb(struct urb *,struct usb_device *,unsigned,void *,int,void (*)(struct urb *),void *,unsigned);
struct usb_driver *tmc_lx_usbmouse_driver(void);
