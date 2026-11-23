/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* Private descriptor-core ABI for the complete original config.c. Objects
 * never cross into the older bounded URB/HCD adapter. Hardware descriptor
 * reads, allocation, time and diagnostics are real OS imports below. */
#include "base.h"
#include <stdbool.h>
#include "linux_usb_config.h"
typedef u8 __u8;
#define GFP_KERNEL 0
#define EINVAL 22
#define ENOMEM 12
#define EPIPE 32
#define ENOMSG 42
#define ENODEV 19
#define EIO 5
#define NULL ((void *)0)
#define USB_MAXINTERFACES 32
#define USB_MAXENDPOINTS 30
#define USB_MAXIADS 16
#define USB_SPEED_LOW 1
#define USB_SPEED_FULL 2
#define USB_SPEED_HIGH 3
#define USB_SPEED_SUPER 5
#define USB_SPEED_SUPER_PLUS 6
#define USB_DT_DEVICE 1
#define USB_DT_CONFIG 2
#define USB_DT_INTERFACE 4
#define USB_DT_ENDPOINT 5
#define USB_DT_INTERFACE_ASSOCIATION 11
#define USB_DT_BOS 15
#define USB_DT_DEVICE_CAPABILITY 16
#define USB_DT_SS_ENDPOINT_COMP 48
#define USB_DT_SSP_ISOC_ENDPOINT_COMP 49
#define USB_DT_CONFIG_SIZE 9
#define USB_DT_INTERFACE_SIZE 9
#define USB_DT_ENDPOINT_SIZE 7
#define USB_DT_ENDPOINT_AUDIO_SIZE 9
#define USB_DT_INTERFACE_ASSOCIATION_SIZE 8
#define USB_DT_BOS_SIZE 5
#define USB_DT_SS_EP_COMP_SIZE 6
#define USB_DT_SSP_ISOC_EP_COMP_SIZE 8
#define USB_DT_USB_WIRELESS_CAP_SIZE 11
#define USB_DT_USB_EXT_CAP_SIZE 7
#define USB_DT_USB_SS_CAP_SIZE 10
#define USB_DT_USB_SS_CONTN_ID_SIZE 20
#define USB_DT_USB_PTM_ID_SIZE 3
#define USB_DT_USB_SSP_CAP_SIZE(n) (12+((n)+1)*4)
#define USB_CAP_TYPE_WIRELESS_USB 1
#define USB_CAP_TYPE_EXT 2
#define USB_SS_CAP_TYPE 3
#define CONTAINER_ID_TYPE 4
#define USB_SSP_CAP_TYPE 10
#define USB_PTM_CAP_TYPE 11
#define USB_SSP_SUBLINK_SPEED_ATTRIBS 31
#define USB_ENDPOINT_NUMBER_MASK 15
#define USB_ENDPOINT_DIR_MASK 128
#define USB_ENDPOINT_XFER_CONTROL 0
#define USB_ENDPOINT_XFER_ISOC 1
#define USB_ENDPOINT_XFER_BULK 2
#define USB_ENDPOINT_XFER_INT 3
#define USB_EP_MAXP_MULT_MASK (3u<<11)
#define USB_SS_MULT(a) (1+((a)&3))
#define USB_SS_SSP_ISOC_COMP(a) ((a)&128)
#define USB_QUIRK_ENDPOINT_IGNORE 1u
#define USB_QUIRK_LINEAR_FRAME_INTR_BINTERVAL 2u
#define USB_QUIRK_LINEAR_UFRAME_INTR_BINTERVAL 4u
#define USB_QUIRK_HONOR_BNUMINTERFACES 8u
#define USB_QUIRK_DELAY_INIT 16u
#define min(a,b) MIN(a,b)
#define max(a,b) MAX(a,b)
#define clamp(x,a,b) MIN(MAX(x,a),b)
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define cpu_to_le16(x) ((u16)(x))
#define fallthrough __attribute__((fallthrough))
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
struct usb_descriptor_header {u8 bLength,bDescriptorType;} PACK;
struct usb_config_descriptor {u8 bLength,bDescriptorType;u16 wTotalLength;u8 bNumInterfaces,bConfigurationValue,iConfiguration,bmAttributes,bMaxPower;} PACK;
struct usb_interface_descriptor {u8 bLength,bDescriptorType,bInterfaceNumber,bAlternateSetting,bNumEndpoints,bInterfaceClass,bInterfaceSubClass,bInterfaceProtocol,iInterface;} PACK;
struct usb_endpoint_descriptor {u8 bLength,bDescriptorType,bEndpointAddress,bmAttributes;u16 wMaxPacketSize;u8 bInterval,bRefresh,bSynchAddress;} PACK;
struct usb_ss_ep_comp_descriptor {u8 bLength,bDescriptorType,bMaxBurst,bmAttributes;u16 wBytesPerInterval;} PACK;
struct usb_ssp_isoc_ep_comp_descriptor {u8 bLength,bDescriptorType;u16 wReserved;u32 dwBytesPerInterval;} PACK;
struct usb_interface_assoc_descriptor {u8 bLength,bDescriptorType,bFirstInterface,bInterfaceCount,bFunctionClass,bFunctionSubClass,bFunctionProtocol,iFunction;} PACK;
struct usb_bos_descriptor {u8 bLength,bDescriptorType;u16 wTotalLength;u8 bNumDeviceCaps;} PACK;
struct usb_dev_cap_header {u8 bLength,bDescriptorType,bDevCapabilityType;} PACK;
struct usb_ext_cap_descriptor {u8 bLength,bDescriptorType,bDevCapabilityType;u32 bmAttributes;} PACK;
struct usb_ss_cap_descriptor {u8 data[10];} PACK;
struct usb_ss_container_id_descriptor {u8 data[20];} PACK;
struct usb_ptm_cap_descriptor {u8 data[3];} PACK;
struct usb_ssp_cap_descriptor {u8 bLength,bDescriptorType,bDevCapabilityType,bReserved;u32 bmAttributes;u16 wFunctionalitySupport,wReserved;u32 bmSublinkSpeedAttr[];} PACK;
struct usb_host_bos {struct usb_bos_descriptor *desc;struct usb_ext_cap_descriptor *ext_cap;struct usb_ss_cap_descriptor *ss_cap;struct usb_ss_container_id_descriptor *ss_id;struct usb_ptm_cap_descriptor *ptm_cap;struct usb_ssp_cap_descriptor *ssp_cap;};
struct list_head {struct list_head *next,*prev;};
static inline void INIT_LIST_HEAD(struct list_head *p){p->next=p->prev=p;}
struct kref {unsigned count;};
static inline void kref_init(struct kref *r){r->count=1;}
static inline int kref_put(struct kref *r,void (*release)(struct kref *))
{
 unsigned n=__atomic_load_n(&r->count,__ATOMIC_ACQUIRE);
 for(;;){if(!n||n==~0u)return 0;if(__atomic_compare_exchange_n(&r->count,&n,n-1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){if(n==1){release(r);return 1;}return 0;}}
}
struct usb_host_endpoint {struct usb_endpoint_descriptor desc;struct usb_ss_ep_comp_descriptor ss_ep_comp;struct usb_ssp_isoc_ep_comp_descriptor ssp_isoc_ep_comp;struct list_head urb_list;u8 *extra;int extralen;};
struct usb_host_interface {struct usb_interface_descriptor desc;struct usb_host_endpoint *endpoint;u8 *extra;int extralen;char *string;};
struct usb_interface_cache {struct kref ref;unsigned num_altsetting;struct usb_host_interface altsetting[];};
struct usb_host_config {struct usb_config_descriptor desc;struct usb_interface_cache *intf_cache[USB_MAXINTERFACES];struct usb_interface_assoc_descriptor *intf_assoc[USB_MAXIADS];u8 *extra;int extralen;char *string;};
struct device {struct usb_device *usb;};
struct usb_device {
 struct device dev;struct {u8 bNumConfigurations;} descriptor;
 unsigned speed,quirks;struct usb_host_config *config;u8 **rawdescriptors;struct usb_host_bos *bos;
 TmcConfigRead read;void *context;
};
#define to_usb_device(d) ((d)->usb)
#define ref_to_usb_interface_cache(r) container_of(r,struct usb_interface_cache,ref)
static inline unsigned usb_endpoint_type(const struct usb_endpoint_descriptor *d){return d->bmAttributes&3;}
static inline unsigned usb_endpoint_num(const struct usb_endpoint_descriptor *d){return d->bEndpointAddress&15;}
static inline unsigned usb_endpoint_maxp(const struct usb_endpoint_descriptor *d){return d->wMaxPacketSize&2047;}
static inline int usb_endpoint_xfer_control(const struct usb_endpoint_descriptor *d){return usb_endpoint_type(d)==0;}
static inline int usb_endpoint_xfer_isoc(const struct usb_endpoint_descriptor *d){return usb_endpoint_type(d)==1;}
static inline int usb_endpoint_xfer_bulk(const struct usb_endpoint_descriptor *d){return usb_endpoint_type(d)==2;}
static inline int usb_endpoint_xfer_int(const struct usb_endpoint_descriptor *d){return usb_endpoint_type(d)==3;}
/* No platform-specific endpoint-ignore table is enabled by this adapter. */
static inline int usb_endpoint_is_ignored(struct usb_device *d,struct usb_host_interface *i,struct usb_endpoint_descriptor *e){(void)d;(void)i;(void)e;return 0;}
static inline int fls(unsigned v){return v?32-__builtin_clz(v):0;}
static inline size_t config_struct_size(size_t base,size_t item,size_t n){return n>(~(size_t)0-base)/item?~(size_t)0:base+item*n;}
#define struct_size(p,m,n) config_struct_size(sizeof(*(p)),sizeof((p)->m[0]),(n))
void *tmc_config_alloc(size_t,unsigned);
void tmc_config_free(void *);
#define kmalloc tmc_config_alloc
#define kzalloc tmc_config_alloc
#define kfree tmc_config_free
void tmc_config_delay(unsigned);
#define msleep tmc_config_delay
void tmc_config_log(struct device *,unsigned,const char *,...);
#define dev_notice(d,fmt,...) tmc_config_log(d,1,fmt,##__VA_ARGS__)
#define dev_warn(d,fmt,...) tmc_config_log(d,1,fmt,##__VA_ARGS__)
#define dev_err(d,fmt,...) tmc_config_log(d,2,fmt,##__VA_ARGS__)
#define dev_dbg(d,fmt,...) tmc_config_log(d,0,fmt,##__VA_ARGS__)
int tmc_config_get_descriptor(struct usb_device *,unsigned,unsigned,void *,unsigned);
#define usb_get_descriptor tmc_config_get_descriptor
#define usb_get_configuration tmc_config_get_configuration
#define usb_destroy_configuration tmc_config_destroy_configuration
#define usb_release_interface_cache tmc_config_release_interface_cache
#define usb_get_bos_descriptor tmc_config_get_bos_descriptor
#define usb_release_bos_descriptor tmc_config_release_bos_descriptor
void tmc_config_destroy_configuration(struct usb_device *);
void tmc_config_release_bos_descriptor(struct usb_device *);
