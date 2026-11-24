/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* The implemented Linux USB/input API subset for usbmouse.c/usbkbd.c, NOT a set of
 * empty replacements for general Linux kernel services. Runtime definitions
 * are in usb_input.c; the native kernel supplies allocation, locking/events.
 */
#include "base.h"
#include <stdarg.h>
#include <stdbool.h>
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
#define EV_LED 17
#define EV_REP 20
#define LED_NUML 0
#define LED_CAPSL 1
#define LED_SCROLLL 2
#define LED_COMPOSE 3
#define LED_KANA 4
#define USB_TYPE_CLASS 0x20
#define USB_RECIP_INTERFACE 1
#ifndef KBUILD_MODNAME
#define KBUILD_MODNAME "linux-usb"
#endif
#define BTN_MOUSE 0x110
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112
#define BTN_SIDE 0x113
#define BTN_EXTRA 0x114
#define REL_X 0
#define REL_Y 1
#define REL_WHEEL 8
#define REL_HWHEEL 6
#define BIT_WORD(x) ((unsigned)(x)/64)
#define BIT_MASK(x) (1UL<<((unsigned)(x)%64))
#define USB_INTERFACE_CLASS_HID 3
#define USB_INTERFACE_SUBCLASS_BOOT 1
#define USB_INTERFACE_PROTOCOL_MOUSE 2
#define USB_INTERFACE_PROTOCOL_KEYBOARD 1
#define USB_INTERFACE_INFO(c,s,p) .cls=(c),.sub=(s),.proto=(p)
/* These are static module metadata, not unimplemented runtime operations. */
#define MODULE_AUTHOR(x) static const char tmc_lx_author[] __attribute__((used))=(x)
#define MODULE_DESCRIPTION(x) static const char tmc_lx_description[] __attribute__((used))=(x)
#define MODULE_LICENSE(x) static const char tmc_lx_license[] __attribute__((used))=(x)
#define MODULE_DEVICE_TABLE(kind,table)
#define tmc_lx_usb_mouse_driver tmc_lx_usbmouse_driver
#define tmc_lx_usb_kbd_driver tmc_lx_usbkbd_driver
#define module_usb_driver(driver) struct usb_driver *tmc_lx_##driver(void){return &(driver);}
#define le16_to_cpu(x) (x) /* supported kernel target is little-endian x86-64 */
/* Core endpoint queues/reference types precede USB device representations. */
#include "tmc_urb_types.h"
struct device {struct device *parent;};
static inline int device_set_wakeup_enable(struct device *d,int on){(void)d;return on?-95:0;} /* no platform suspend/wake implementation */
struct input_id {u16 bustype,vendor,product,version;};
struct input_dev {
 const char *name,*phys;struct input_id id;struct device dev;
 unsigned long evbit[1],keybit[16],relbit[1],ledbit[1],led[1],key[4];
 void *driver_data;int (*open)(struct input_dev *);void (*close)(struct input_dev *);
 int registered,x,y,wheel,hwheel,buttons;void *owner;
 int (*event)(struct input_dev *,unsigned,unsigned,int);
};
struct usb_bus {const char *bus_name;int no_sg_constraint,busnum;struct usb_device *root_hub;};
struct usb_host_endpoint;
struct usb_device {
 struct device dev;struct usb_bus *bus;const char *manufacturer,*product,*devpath;
 struct {u16 idVendor,idProduct;} descriptor;unsigned maxpacket;void *owner;
 unsigned long quirks;int state,speed,can_submit;atomic_t urbnum,references;struct usb_host_endpoint *ep_in[16],*ep_out[16];
};
struct usb_endpoint_descriptor {u8 bLength,bDescriptorType,bEndpointAddress,bmAttributes;u16 wMaxPacketSize;u8 bInterval;} PACK;
struct usb_ssp_isoc_ep_comp_descriptor {u32 dwBytesPerInterval;};
struct usb_host_endpoint {struct usb_endpoint_descriptor desc;
 struct {u8 bMaxBurst,bmAttributes;} ss_ep_comp;
 struct usb_ssp_isoc_ep_comp_descriptor ssp_isoc_ep_comp;
 struct list_head urb_list;int enabled;
};
struct usb_host_interface {struct {u8 bNumEndpoints,bInterfaceNumber;} desc;struct usb_host_endpoint *endpoint;};
struct usb_interface {struct device dev;struct usb_device *usbdev;struct usb_host_interface *cur_altsetting;void *driver_data;};
struct usb_device_id {u8 cls,sub,proto;};
/* The core ABI and real atomic/list/wait services used by upstream urb.c. */
struct usb_driver {const char *name;int (*probe)(struct usb_interface *,const struct usb_device_id *);void (*disconnect)(struct usb_interface *);const struct usb_device_id *id_table;};
void *tmc_lx_zalloc(size_t,gfp_t);
void tmc_lx_free(void *);
#define kzalloc tmc_lx_zalloc
#define kmalloc tmc_lx_zalloc
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
static inline unsigned usb_rcvintpipe(struct usb_device *d,unsigned ep){(void)d;return (1u<<30)|(ep&15)<<15|128;}
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

struct usb_ctrlrequest {u8 bRequestType,bRequest;u16 wValue,wIndex,wLength;} PACK;
#define cpu_to_le16(x) ((u16)(x))
void tmc_lx_spin_init(spinlock_t *);
unsigned long tmc_lx_spin_lock_irqsave(spinlock_t *);
void tmc_lx_spin_unlock_irqrestore(spinlock_t *,unsigned long);
#define spin_lock_init(l) tmc_lx_spin_init(l)
#define spin_lock_irqsave(l,f) do {(f)=tmc_lx_spin_lock_irqsave(l);} while(0)
#define spin_unlock_irqrestore(l,f) tmc_lx_spin_unlock_irqrestore((l),(f))
static inline void set_bit(unsigned n,unsigned long *b){b[BIT_WORD(n)]|=BIT_MASK(n);}
static inline void clear_bit(unsigned n,unsigned long *b){b[BIT_WORD(n)]&=~BIT_MASK(n);}
static inline int test_bit(unsigned n,const unsigned long *b){return !!(b[BIT_WORD(n)]&BIT_MASK(n));}
void *memscan(void *,int,size_t);
void tmc_lx_hid_log(void *,unsigned,const char *,...);
#define hid_info(d,fmt,...) tmc_lx_hid_log((d),0,(fmt),##__VA_ARGS__)
#define hid_warn(d,fmt,...) tmc_lx_hid_log((d),1,(fmt),##__VA_ARGS__)
#define hid_err(d,fmt,...) tmc_lx_hid_log((d),2,(fmt),##__VA_ARGS__)
#define pr_err(fmt,...) tmc_lx_hid_log(0,2,pr_fmt(fmt),##__VA_ARGS__)
static inline unsigned usb_sndctrlpipe(struct usb_device *d,unsigned ep){(void)d;return (2u<<30)|(ep&15)<<15;}
void usb_fill_control_urb(struct urb *,struct usb_device *,unsigned,u8 *,void *,int,void (*)(struct urb *),void *);
struct usb_driver *tmc_lx_usbkbd_driver(void);
unsigned tmc_lx_usbkbd_usage(unsigned);

#include "tmc_urb_core.h"
#include "tmc_hcd_core.h"

/* Real registered non-USB input devices share button state with USB mice.
 * No fabricated usb_interface or URB is created for a PS/2 pointer. */
int tmc_lx_input_register_pointer(struct input_dev *);
void tmc_lx_input_unregister_pointer(struct input_dev *);
void tmc_lx_os_hwheel(int);

int tmc_lx_input_register_keyboard(struct input_dev *);
void tmc_lx_input_unregister_keyboard(struct input_dev *);
void tmc_lx_input_absolute(struct input_dev *,int,int,int,void (*)(int,int,int,int));
