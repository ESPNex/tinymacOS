/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "linux_usb.h"
#include <stdbool.h>
/* Private views for the ORIGINAL hub.c port-debounce/reset functions. They
 * are not ABI aliases of Boot/config/HID USB objects. Physical HCDs still
 * enumerate children; this is not the whole hub class driver or hotplug. */
#define EINVAL 22
#define EIO 5
#define ENODEV 19
#define EBUSY 16
#define EAGAIN 11
#define ENOTCONN 107
#define ETIMEDOUT 110
#define EOPNOTSUPP 95
#define USB_SPEED_LOW 1
#define USB_SPEED_FULL 2
#define USB_SPEED_HIGH 3
#define USB_SPEED_SUPER 5
#define USB_SPEED_SUPER_PLUS 6
#define USB_SSP_GEN_UNKNOWN 0
#define USB_STATE_NOTATTACHED 0
#define USB_STATE_DEFAULT 4
#define USB_PORT_STAT_CONNECTION 1
#define USB_PORT_STAT_ENABLE 2
#define USB_PORT_STAT_RESET 16
#define USB_PORT_STAT_LOW_SPEED 512
#define USB_PORT_STAT_HIGH_SPEED 1024
#define USB_PORT_STAT_LINK_STATE 0x1e0
#define USB_PORT_STAT_C_CONNECTION 1
#define USB_SS_PORT_LS_SS_INACTIVE 0xc0
#define USB_SS_PORT_LS_COMP_MOD 0x140
#define USB_PORT_FEAT_RESET 4
#define USB_PORT_FEAT_C_CONNECTION 16
#define USB_PORT_FEAT_C_RESET 20
#define USB_PORT_FEAT_BH_PORT_RESET 28
#define USB_PORT_FEAT_C_BH_PORT_RESET 29
#define USB_PORT_FEAT_C_PORT_LINK_STATE 25
#define HUB_EXT_PORT_STATUS 2
#define USB_EXT_PORT_RX_LANES(p) (((p)>>8)&15)
#define USB_EXT_PORT_TX_LANES(p) (((p)>>12)&15)
#define USB_PORT_QUIRK_FAST_ENUM 1
#define USB_QUIRK_HUB_SLOW_RESET 1
#define HUB_DEBOUNCE_TIMEOUT 2000
#define HUB_DEBOUNCE_STEP 25
#define HUB_DEBOUNCE_STABLE 100
#define HUB_SHORT_RESET_TIME 10
#define HUB_LONG_RESET_TIME 200
#define HUB_RESET_TIMEOUT 800
#define PORT_RESET_TRIES 5
struct device {unsigned present;};
struct usb_device;
struct usb_hcd {struct {int (*reset_device)(struct usb_hcd *,struct usb_device *);} *driver;};
struct usb_bus {struct usb_hcd *hcd;};
struct usb_device {
 TmcLinuxHidControl control;void *context;int speed,devnum,state,rx_lanes,tx_lanes,ssp_rate;
 unsigned quirks;struct usb_bus *bus;
};
struct usb_port {struct device dev;unsigned quirks;};
struct usb_hub {struct usb_device *hdev;struct usb_port *ports[15];struct device *intfdev;unsigned long warm_reset_bits[1];};
static struct {u64 opens,debounced,resets,failures,status_reads,feature_writes,delayed_ms,rejected;} tmc_linux_hub_counters;
#define HSTAT(f) __atomic_add_fetch(&tmc_linux_hub_counters.f,1,__ATOMIC_RELAXED)
static unsigned ehci_cf_port_reset_rwsem;
/* Actual readers/writer exclusion, shared with native EHCI CONFIGFLAG writes.
 * Boot enumeration/polling only; not a Linux scheduler or sleepable rwsem. */
static void down_read(unsigned *s)
{
 unsigned n=__atomic_load_n(s,__ATOMIC_ACQUIRE);
 for(;;){if(!(n&0x80000000u)&&__atomic_compare_exchange_n(s,&n,n+1,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return;n=__atomic_load_n(s,__ATOMIC_ACQUIRE);__asm__ volatile("pause");}
}
static void up_read(unsigned *s){__atomic_sub_fetch(s,1,__ATOMIC_RELEASE);}
void tmc_linux_usb_cf_begin(void)
{unsigned n=0;while(!__atomic_compare_exchange_n(&ehci_cf_port_reset_rwsem,&n,0x80000000u,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)){n=0;__asm__ volatile("pause");}}
void tmc_linux_usb_cf_end(void){__atomic_store_n(&ehci_cf_port_reset_rwsem,0,__ATOMIC_RELEASE);}
extern void usb_delay_ms(unsigned);
static void msleep(unsigned n){__atomic_add_fetch(&tmc_linux_hub_counters.delayed_ms,n,__ATOMIC_RELAXED);usb_delay_ms(n);}
static void usleep_range(unsigned low,unsigned high){(void)high;msleep((low+999)/1000);}
#define dev_dbg(d,fmt,...) do {(void)(d);}while(0)
#define dev_err(d,fmt,...) do {(void)(d);}while(0) /* non-secret failure counter returned at the public boundary */
static int hub_is_superspeed(struct usb_device *d){return d->speed>=USB_SPEED_SUPER;}
static int hub_is_superspeedplus(struct usb_device *d){return d->speed==USB_SPEED_SUPER_PLUS;}
static int test_bit(unsigned bit,const unsigned long *p){return !!(p[bit/64]&(1UL<<(bit%64)));}
static void clear_bit(unsigned bit,unsigned long *p){p[bit/64]&=~(1UL<<(bit%64));}
static struct usb_hcd *bus_to_hcd(struct usb_bus *b){return b->hcd;}
static void update_devnum(struct usb_device *d,int n){d->devnum=n;}
static void usb_set_device_state(struct usb_device *d,int state){d->state=state;}
static int usb_hub_port_status(struct usb_hub *h,int port,u16 *status,u16 *change)
{
 u8 data[4];HSTAT(status_reads);int n=h->hdev->control(h->hdev->context,0xa3,0,0,port,data,4);
 if(n<0)return n;if(n!=4)return -EIO;*status=data[0]|data[1]<<8;*change=data[2]|data[3]<<8;return 0;
}
static int feature(struct usb_device *d,int port,int f,unsigned request)
{HSTAT(feature_writes);int result=d->control(d->context,0x23,request,f,port,0,0);return result>0?-EIO:result;}
static int usb_clear_port_feature(struct usb_device *d,int port,int f){return feature(d,port,f,1);}
static int set_port_feature(struct usb_device *d,int port,int f){return feature(d,port,f,3);}
/* These SuperSpeed services explicitly reject: the public adapter accepts
 * USB2 and USB 3.0 hubs only; SSP lane/rate decoding is not exposed. */
static int hub_ext_port_status(struct usb_hub *h,int port,int type,u16 *st,u16 *ch,u32 *ext)
{(void)h;(void)port;(void)type;(void)st;(void)ch;(void)ext;return -EOPNOTSUPP;}
static int get_port_ssp_rate(struct usb_device *d,u32 ext){(void)d;(void)ext;return USB_SSP_GEN_UNKNOWN;}
#include "../../vendor/linux-v6.12/hub-debounce.inc"
#include "../../vendor/linux-v6.12/hub-reset.inc"
int tmc_linux_hub_port_init(TmcLinuxHidControl control,void *context,unsigned port,unsigned speed,unsigned *status)
{
 if(!control||!status||port<1||port>15||(speed!=USB_SPEED_LOW&&speed!=USB_SPEED_FULL&&speed!=USB_SPEED_HIGH&&speed!=USB_SPEED_SUPER)){HSTAT(rejected);return -EINVAL;}
 struct usb_device d={.control=control,.context=context,.speed=speed};struct usb_port p={.dev={1}};
 struct usb_hub h={.hdev=&d,.intfdev=&p.dev};h.ports[port-1]=&p;HSTAT(opens);
 u16 initial,change;int result=usb_hub_port_status(&h,port,&initial,&change);
 if(result)goto fail;if(!(initial&USB_PORT_STAT_CONNECTION)){result=-ENOTCONN;goto fail;}
 result=hub_port_debounce(&h,port,true);
 if(result<0)goto fail;if(result&8){result=-EIO;goto fail;}HSTAT(debounced);
 result=hub_port_reset(&h,port,0,HUB_SHORT_RESET_TIME,false);if(result)goto fail;HSTAT(resets);
 u16 st,ch;result=usb_hub_port_status(&h,port,&st,&ch);if(result)goto fail;
 if(!(st&USB_PORT_STAT_CONNECTION)||!(st&USB_PORT_STAT_ENABLE)||(st&(USB_PORT_STAT_RESET|8))){result=-ENOTCONN;goto fail;}
 *status=st;return 0;
fail:HSTAT(failures);return result;
}
