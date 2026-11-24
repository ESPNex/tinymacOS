/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
/* Private whole-EHCI HCD ABI, sharing actual DMA-pool services and URB types.
 * It is NOT a binary alias of the native Boot/config/HID USB device objects. */
#include "../dma-include/tmc_dma_compat.h"
#include "ehci_prefix.h"
#define TMC_URB_EXTERNAL_LIST_SPIN 1
#include "../include/linux/tmc_urb_types.h"
#undef TMC_URB_EXTERNAL_LIST_SPIN
#define CONFIG_USB_PCI 1
#define CONFIG_HAS_IOPORT 1
#define CONFIG_USB_EHCI_HCD 1
#define CONFIG_USB_EHCI_HCD_PLATFORM 0
#define CONFIG_USB_EHCI_TT_NEWSCHED 0
#define CONFIG_USB_EHCI_ASPEED 0
#define _LX_ARG(_0,_1,...) _1
#define _LX_PLACE_1 0,
#define _LX_VALUE(x) _LX_VALUE2(_LX_PLACE_##x)
#define _LX_VALUE2(x) _LX_ARG(x 1,0)
#define IS_ENABLED(x) _LX_VALUE(x)
#define __iomem
#define __le32 u32
#define __le16 u16
typedef u8 __u8;
typedef u32 __u32;
typedef u16 __u16;
#define cpu_to_le16(x) ((u16)(x))
#define cpu_to_le32(x) ((u32)(x))
#define le16_to_cpu(x) (x)
#define __init
#define __exit
#define __acquires(x)
#define __releases(x)
#define __aligned(x) __attribute__((aligned(x)))
#define READ_ONCE(x) __atomic_load_n(&(x),__ATOMIC_ACQUIRE)
#define WRITE_ONCE(x,v) __atomic_store_n(&(x),(v),__ATOMIC_RELEASE)
#define barrier() __asm__ volatile("":::"memory")
#define mb() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define wmb() __atomic_thread_fence(__ATOMIC_RELEASE)
#define likely(x) __builtin_expect(!!(x),1)
#define min_t(t,a,b) ((t)(a)<(t)(b)?(t)(a):(t)(b))
#define DIV_ROUND_UP(n,d) (((n)+(d)-1)/(d))
#define __ffs(v) __builtin_ctzl((unsigned long)(v))
static inline void list_add_tail(struct list_head *n,struct list_head *h){list_add(n,h->prev);}
static inline void list_replace_init(struct list_head *old,struct list_head *n){n->next=old->next;n->next->prev=n;n->prev=old->prev;n->prev->next=n;INIT_LIST_HEAD(old);}
static inline void list_del_init(struct list_head *n){list_del(n);INIT_LIST_HEAD(n);}
static inline void list_move(struct list_head *n,struct list_head *h){list_del(n);list_add(n,h);}
static inline void list_move_tail(struct list_head *n,struct list_head *h){list_del(n);list_add_tail(n,h);}
#define list_for_each_entry_reverse(p,h,m) for(p=list_entry((h)->prev,__typeof__(*p),m);&(p)->m!=(h);p=list_entry((p)->m.prev,__typeof__(*p),m))
#define list_prepare_entry(p,h,m) ((p)?(p):list_entry(h,__typeof__(*p),m))
#define list_for_each_entry_continue(p,h,m) for(p=list_entry((p)->m.next,__typeof__(*p),m);&(p)->m!=(h);p=list_entry((p)->m.next,__typeof__(*p),m))
static inline void spin_lock(spinlock_t *s){while(__atomic_exchange_n(&s->word,1,__ATOMIC_ACQUIRE))tmc_lx_os_spin_wait();}
static inline void spin_unlock(spinlock_t *s){__atomic_store_n(&s->word,0,__ATOMIC_RELEASE);}
static inline void ehci_spin_lock_irq(spinlock_t *s){s->irq_flags=dma_lock(s);}
static inline void ehci_spin_unlock_irq(spinlock_t *s){u32 f=s->irq_flags;dma_unlock(s,f);}
#define URB_NO_TRANSFER_DMA_MAP 4
#define USB_PID_IN 0x69
#define USB_PID_OUT 0xe1
#define USB_PID_SETUP 0x2d
struct usb_hcd;
struct usb_device;
struct usb_bus {const char *bus_name;struct device *controller,*sysdev;struct usb_device *root_hub;int busnum,no_sg_constraint;unsigned sg_tablesize;int bandwidth_allocated,bandwidth_int_reqs,bandwidth_isoc_reqs;unsigned long resuming_ports;};
struct usb_ctrlrequest {u8 bRequestType,bRequest;u16 wValue,wIndex,wLength;} PACK;
struct usb_ssp_isoc_ep_comp_descriptor {u32 dwBytesPerInterval;};
struct usb_endpoint_descriptor {u8 bLength,bDescriptorType,bEndpointAddress,bmAttributes;u16 wMaxPacketSize;u8 bInterval;} PACK;
struct usb_host_endpoint {struct usb_endpoint_descriptor desc;struct {u8 bMaxBurst,bmAttributes;} ss_ep_comp;struct usb_ssp_isoc_ep_comp_descriptor ssp_isoc_ep_comp;struct list_head urb_list;int enabled;void *hcpriv;};
struct work_struct {void (*function)(struct work_struct *);struct work_struct *next;unsigned pending,running;};
struct giveback_urb_bh {bool running,high_prio;spinlock_t lock;struct list_head head;struct work_struct bh;struct usb_host_endpoint *completing_ep;};
struct usb_tt {struct usb_device *hub;int multi,think_time;void *hcpriv;spinlock_t lock;struct list_head clear_list;struct work_struct clear_work;};
struct usb_tt_clear {struct list_head clear_list;u16 tt,devinfo;struct usb_hcd *hcd;struct usb_host_endpoint *ep;};
struct usb_hub {struct usb_device *hdev;struct usb_tt tt;unsigned long power_bits[1];};
struct usb_device {struct device dev;struct usb_bus *bus;int state,speed,can_submit,devnum,devaddr,do_remote_wakeup,maxchild,persist_enabled,ttport;struct usb_tt *tt;atomic_t references,urbnum;unsigned long toggle[2];struct usb_host_endpoint *ep_in[16],*ep_out[16];};
#include "../include/linux/tmc_urb_core.h"
#undef spin_lock_irq
#undef spin_unlock_irq
#define spin_lock_irq(s) ehci_spin_lock_irq(s)
#define spin_unlock_irq(s) ehci_spin_unlock_irq(s)
void ehci_runtime_progress(void);
#undef cpu_relax
#define cpu_relax() ehci_runtime_progress()
#define HZ 1000ul
#define jiffies ((unsigned long)tmc_lx_os_clock_ms())
#define time_after(a,b) ((long)((b)-(a))<0)
#define time_after_eq(a,b) ((long)((a)-(b))>=0)
#define time_before(a,b) time_after(b,a)
#define from_timer(v,t,m) container_of(t,__typeof__(*v),m)
struct timer_list {struct timer_list *next;unsigned long expires;void (*function)(struct timer_list *);unsigned pending,running;};
void timer_setup(struct timer_list *,void (*)(struct timer_list *),unsigned);
int mod_timer(struct timer_list *,unsigned long);
int del_timer(struct timer_list *);
void del_timer_sync(struct timer_list *);
#define init_waitqueue_head(q) ((q)->sequence=0)
#define wake_up_all(q) wake_up(q)
#define wait_event_interruptible(q,condition) ({wait_event(q,condition);0;})
#define set_bit(n,p) __atomic_fetch_or((p)+(n)/64,1ul<<((n)%64),__ATOMIC_ACQ_REL)
#define clear_bit(n,p) __atomic_fetch_and((p)+(n)/64,~(1ul<<((n)%64)),__ATOMIC_ACQ_REL)
#define test_bit(n,p) (!!(__atomic_load_n((p)+(n)/64,__ATOMIC_ACQUIRE)&(1ul<<((n)%64))))
#define test_and_set_bit(n,p) (!!(set_bit(n,p)&(1ul<<((n)%64))))
struct usb_hcd;
typedef int irqreturn_t;
#define IRQ_NONE 0
#define IRQ_HANDLED 1
#define HCD_DMA 1
#define HCD_USB11 2
#define HCD_FLAG_POLL_RH 0
#define USB_UHCI_LOADED 0
#define USB_OHCI_LOADED 1
#define USB_EHCI_LOADED 2
struct hc_driver {const char *description,*product_desc;size_t hcd_priv_size;int flags;irqreturn_t (*irq)(struct usb_hcd *);int (*reset)(struct usb_hcd *);int (*start)(struct usb_hcd *);void (*stop)(struct usb_hcd *);int (*urb_enqueue)(struct usb_hcd *,struct urb *,gfp_t);int (*urb_dequeue)(struct usb_hcd *,struct urb *,int);void (*endpoint_disable)(struct usb_hcd *,struct usb_host_endpoint *);int (*get_frame_number)(struct usb_hcd *);int (*hub_status_data)(struct usb_hcd *,char *);int (*hub_control)(struct usb_hcd *,u16,u16,u16,char *,u16);void (*shutdown)(struct usb_hcd *);void (*endpoint_reset)(struct usb_hcd *,struct usb_host_endpoint *);void (*clear_tt_buffer_complete)(struct usb_hcd *,struct usb_host_endpoint *);int (*bus_suspend)(struct usb_hcd *);int (*bus_resume)(struct usb_hcd *);void (*relinquish_port)(struct usb_hcd *,int);int (*port_handed_over)(struct usb_hcd *,int);unsigned long (*get_resuming_ports)(struct usb_hcd *);void (*free_dev)(struct usb_hcd *,struct usb_device *);int (*port_power)(struct usb_hcd *,int,bool);int (*pci_suspend)(struct usb_hcd *,bool);int (*pci_resume)(struct usb_hcd *,bool);};
struct usb_hcd {void *regs,*usb_phy;int has_tt;struct usb_bus self;const struct hc_driver *driver;unsigned long flags;int irq,running,accessible,uses_new_polling;void *localmem_pool;u64 rsrc_start,rsrc_len;struct timer_list rh_timer;unsigned root_changes,root_resume,polling,irq_running;unsigned long root_resume_ports;struct giveback_urb_bh high_prio_bh,low_prio_bh;u8 hcd_priv[] __attribute__((aligned(16)));};
static inline struct usb_hcd *bus_to_hcd(struct usb_bus *b){return container_of(b,struct usb_hcd,self);}
#define HCD_HW_ACCESSIBLE(h) ((h)->accessible)
#define HCD_RH_RUNNING(h) ((h)->running)
static inline int is_root_hub(struct usb_device *d){return d&&d->bus->root_hub==d;}
static inline unsigned usb_pipeendpoint(unsigned p){return (p>>15)&15;}
static inline unsigned usb_pipedevice(unsigned p){return (p>>8)&127;}
static inline int usb_pipein(unsigned p){return !!(p&USB_DIR_IN);}
static inline int usb_endpoint_dir_in(const struct usb_endpoint_descriptor *d){return !!(d->bEndpointAddress&128);}
static inline unsigned usb_maxpacket(struct usb_device *d,unsigned p){struct usb_host_endpoint *e=usb_pipe_endpoint(d,p);return e?usb_endpoint_maxp(&e->desc):0;}
static inline unsigned usb_gettoggle(struct usb_device *d,unsigned ep,unsigned out){return !!(d->toggle[out]&(1ul<<ep));}
static inline void usb_settoggle(struct usb_device *d,unsigned ep,unsigned out,unsigned v){if(v)set_bit(ep,&d->toggle[out]);else clear_bit(ep,&d->toggle[out]);}
#define sg_dma_address(s) ((s)->dma_address)
#define sg_dma_len(s) ((s)->dma_length)
#define sg_next(s) ((s)->next)
static inline void *dma_pool_zalloc(struct dma_pool *p,gfp_t f,dma_addr_t *d){return dma_pool_alloc(p,f|__GFP_ZERO,d);}
struct kmem_cache {size_t size;unsigned active;};
struct kmem_cache *kmem_cache_create(const char *,size_t,size_t,unsigned,void (*)(void *));
void kmem_cache_destroy(struct kmem_cache *);
void *kmem_cache_zalloc(struct kmem_cache *,gfp_t);
void kmem_cache_free(struct kmem_cache *,void *);
void *ehci_calloc(size_t,size_t,gfp_t);
#define kcalloc ehci_calloc
struct pci_dev {struct device dev;u32 address;u16 vendor,device;unsigned irq,revision;void *driver_data;struct usb_hcd *hcd;};
struct pci_device_id {u32 class,class_mask,vendor,device;};
struct pci_driver {const char *name;const struct pci_device_id *id_table;int (*probe)(struct pci_dev *,const struct pci_device_id *);void (*remove)(struct pci_dev *);void (*shutdown)(struct pci_dev *);struct {int probe_type;} driver;};
#define PCI_DEVICE_CLASS(c,m) .class=(c),.class_mask=(m)
#define PCI_CLASS_SERIAL_USB_EHCI 0x0c0320
#define PCI_VENDOR_ID_INTEL 0x8086
#define PCI_VENDOR_ID_VIA 0x1106
#define PCI_VENDOR_ID_ZHAOXIN 0x1d17
#define PCI_VENDOR_ID_HP 0x103c
#define PCI_COMMAND 4
#define DMI_BOARD_NAME 0
#define to_pci_dev(d) container_of(d,struct pci_dev,dev)
static inline void *pci_get_drvdata(struct pci_dev *d){return d->driver_data;}
int pci_read_config_word(struct pci_dev *,int,u16 *);
int pci_write_config_word(struct pci_dev *,int,u16);
int pci_write_config_byte(struct pci_dev *,int,u8);
int pci_register_driver(struct pci_driver *);
void pci_unregister_driver(struct pci_driver *);
int usb_hcd_pci_probe(struct pci_dev *,const struct hc_driver *);
void usb_hcd_pci_remove(struct pci_dev *);
int usb_disabled(void);
void usb_hc_died(struct usb_hcd *);
void usb_hcd_poll_rh_status(struct usb_hcd *);
void usb_hcd_resume_root_hub(struct usb_hcd *);
void usb_hcd_start_port_resume(struct usb_bus *,int);
void usb_hcd_end_port_resume(struct usb_bus *,int);
void synchronize_irq(unsigned);
int usb_hcd_link_urb_to_ep(struct usb_hcd *,struct urb *);
void usb_hcd_unlink_urb_from_ep(struct usb_hcd *,struct urb *);
int usb_hcd_check_unlink_urb(struct usb_hcd *,struct urb *,int);
void usb_hcd_giveback_urb(struct usb_hcd *,struct urb *,int);
long usb_calc_bus_time(int,int,int,int);
static inline void device_set_wakeup_capable(struct device *d,bool on){d->wakeup_capable=on;}
static inline bool device_may_wakeup(struct device *d){return d->wakeup_capable&&d->wakeup_enabled;}
const char *dmi_get_system_info(unsigned);
void ehci_delay_us(unsigned);
#define udelay ehci_delay_us
#define msleep(n) ehci_delay_us((n)*1000u)
u64 tmc_lx_os_clock_ms(void);
/* These imports perform actual I/O; the host fixture emulates hardware. */
u8 tmc_ehci_os_in8(u16);u16 tmc_ehci_os_in16(u16);u32 tmc_ehci_os_in32(u16);
void tmc_ehci_os_out8(u16,u8);void tmc_ehci_os_out16(u16,u16);void tmc_ehci_os_out32(u16,u32);
#define inb(p) tmc_ehci_os_in8(p)
#define inw(p) tmc_ehci_os_in16(p)
#define inl(p) tmc_ehci_os_in32(p)
#define outb(v,p) tmc_ehci_os_out8(p,v)
#define outw(v,p) tmc_ehci_os_out16(p,v)
#define outl(v,p) tmc_ehci_os_out32(p,v)
u32 tmc_ehci_os_pci_next(u32,u32);
u32 tmc_ehci_os_pci_read(u32,unsigned);
void tmc_ehci_os_pci_write(u32,unsigned,u32);
void tmc_ehci_os_delay_us(unsigned);
const char *tmc_ehci_os_board_name(void);
void ehci_log(unsigned,const char *,...);
void tmc_lx_hid_log(void *,unsigned,const char *,...);
struct urb *usb_alloc_urb(int,gfp_t);
void usb_free_urb(struct urb *);
int usb_submit_urb(struct urb *,gfp_t);
void usb_kill_urb(struct urb *);
#undef dev_err
#undef dev_WARN
#undef dev_warn
#undef dev_dbg
#define dev_err(d,f,...) ehci_log(2,f,##__VA_ARGS__)
#define dev_warn(d,f,...) ehci_log(1,f,##__VA_ARGS__)
#define dev_info(d,f,...) ehci_log(0,f,##__VA_ARGS__)
#define dev_WARN(d,f,...) ehci_log(2,f,##__VA_ARGS__)
#define dev_dbg(d,f,...) do{(void)(d);}while(0)
#undef dev_WARN_ONCE
#define dev_WARN_ONCE(d,c,f,...) WARN_ONCE(c,f,##__VA_ARGS__)
#define pr_debug(f,...) do{}while(0)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_LICENSE(x)
#define MODULE_DEVICE_TABLE(a,b)
#define MODULE_SOFTDEP(x)
#define MODULE_PARM_DESC(a,b)
#define module_param(a,b,c)
#define module_init(fn) int tmc_linux_ehci_module_init(void){return fn();}
#define module_exit(fn) void tmc_linux_ehci_module_exit(void){fn();}
#define S_IRUGO 0444
#define S_IWUSR 0200
struct dentry;
/* CONFIG_DEBUG_FS/DYNAMIC_DEBUG are off, matching upstream disabled stubs. */
#define debugfs_lookup_and_remove(n,d) do{(void)(n);(void)(d);}while(0)
#define debugfs_remove(d) do{(void)(d);}while(0)
#define BitTime(n) (7*8*(n)/6)
#define BW_HOST_DELAY 1000L
#define BW_HUB_LS_SETUP 333L
#define HS_NSECS(n) (((55*8*2083)+(2083UL*(3+BitTime(n))))/1000+5)
#define HS_NSECS_ISO(n) (((38*8*2083)+(2083UL*(3+BitTime(n))))/1000+5)
#define NS_TO_US(n) DIV_ROUND_UP(n,1000L)
#define FRAME_TIME_MAX_USECS_ALLOC 900L
#define EHCI_USBLEGSUP 0xc0
#define EHCI_USBLEGSUP_RWC 0x8f00
#define EHCI_USBLEGSUP_RO 0x5040
#define EHCI_USBCMD 0
#define EHCI_USBCMD_HCRESET 2
#define EHCI_USBCMD_RUN 1
#define EHCI_USBCMD_CONFIGURE 0x40
#define EHCI_USBCMD_EGSM 8
#define EHCI_USBINTR 4
#define EHCI_USBINTR_RESUME 2
#define USB_RESUME_TIMEOUT 40
#define USB_DT_HUB 0x29
#define USB_DT_HUB_NONVAR_SIZE 7
#define HUB_CHAR_NO_LPSM 2
#define HUB_CHAR_INDV_PORT_OCPM 8
#define C_HUB_OVER_CURRENT 1
#define C_HUB_LOCAL_POWER 0
#define USB_PORT_STAT_CONNECTION 1
#define USB_PORT_STAT_ENABLE 2
#define USB_PORT_STAT_SUSPEND 4
#define USB_PORT_STAT_OVERCURRENT 8
#define USB_PORT_STAT_RESET 16
#define USB_PORT_STAT_POWER 256
#define USB_PORT_STAT_LOW_SPEED 512
#define USB_PORT_STAT_C_CONNECTION 1
#define USB_PORT_STAT_C_ENABLE 2
#define USB_PORT_STAT_C_SUSPEND 4
#define USB_PORT_STAT_C_OVERCURRENT 8
#define USB_PORT_STAT_C_RESET 16
#define USB_PORT_FEAT_ENABLE 1
#define USB_PORT_FEAT_SUSPEND 2
#define USB_PORT_FEAT_RESET 4
#define USB_PORT_FEAT_POWER 8
#define USB_PORT_FEAT_C_CONNECTION 16
#define USB_PORT_FEAT_C_ENABLE 17
#define USB_PORT_FEAT_C_SUSPEND 18
#define USB_PORT_FEAT_C_OVER_CURRENT 19
#define USB_PORT_FEAT_C_RESET 20
#define GetHubStatus 0xa000
#define GetPortStatus 0xa300
#define SetHubFeature 0x2003
#define ClearHubFeature 0x2001
#define SetPortFeature 0x2303
#define ClearPortFeature 0x2301
#define GetHubDescriptor 0xa006

#define strcmp tmc_ehci_strcmp
static inline int strcmp(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
#define ENODEV 19
#define ECONNRESET 104
#define ENOSPC 28
#define EPROTO 71
#define EILSEQ 84
#define EOVERFLOW 75
#define ENOSR 63
#define EPIPE 32
#define EFBIG 27
#define ETIMEDOUT 110
#define ESHUTDOWN 108

#define EHOSTUNREACH 113
#define list_for_each(p,h) for((p)=(h)->next;(p)!=(h);(p)=(p)->next)

#include "ehci_services.h"

#define hcd_giveback_urb_in_bh(h) (!!((h)->driver->flags & HCD_BH))
#define system_bh_highpri_wq ((void *)1)
#define system_bh_wq ((void *)2)
int ehci_queue_bh(void *,struct work_struct *);
#define queue_work(q,w) ehci_queue_bh(q,w)

#define usb_pipeisoc(p) (usb_pipetype(p)==PIPE_ISOCHRONOUS)
