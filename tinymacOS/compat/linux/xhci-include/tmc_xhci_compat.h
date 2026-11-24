/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
/* Private whole-XHCI HCD ABI, sharing actual DMA-pool services and URB types.
 * It is NOT a binary alias of the native Boot/config/HID USB device objects. */
#include "tmc_xhci_dma_compat.h"
#include "xhci_prefix.h"
#define TMC_URB_EXTERNAL_LIST_SPIN 1
#include "tmc_xhci_urb_types.h"
#undef TMC_URB_EXTERNAL_LIST_SPIN
#define CONFIG_USB_PCI 1
#define CONFIG_HAS_IOPORT 1
#define CONFIG_USB_XHCI_HCD 1
#define CONFIG_USB_XHCI_HCD_PLATFORM 0
#define CONFIG_USB_XHCI_TT_NEWSCHED 0
#define CONFIG_USB_XHCI_ASPEED 0
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
static inline void xhci_spin_lock_irq(spinlock_t *s){s->irq_flags=dma_lock(s);}
static inline void xhci_spin_unlock_irq(spinlock_t *s){u32 f=s->irq_flags;dma_unlock(s,f);}
#define URB_NO_TRANSFER_DMA_MAP 4
#define USB_PID_IN 0x69
#define USB_PID_OUT 0xe1
#define USB_PID_SETUP 0x2d
struct usb_hcd;
struct usb_device;
struct usb_bus {int no_stop_on_short;const char *bus_name;struct device *controller,*sysdev;struct usb_device *root_hub;int busnum,no_sg_constraint;unsigned sg_tablesize;int bandwidth_allocated,bandwidth_int_reqs,bandwidth_isoc_reqs;unsigned long resuming_ports;};
#define __le64 u64
#define __u64 u64
#define __counted_by(n)
#define __DECLARE_FLEX_ARRAY(t,n) struct {struct {} __empty_##n;t n[];}
#define __le16_to_cpu(v) le16_to_cpu(v)
#include "../../../vendor/linux-v6.12/xhci-uapi-ch9.h"
#include "../../../vendor/linux-v6.12/xhci-uapi-ch11.h"
struct usb_host_endpoint {struct usb_endpoint_descriptor desc;struct usb_ss_ep_comp_descriptor ss_ep_comp;struct usb_ssp_isoc_ep_comp_descriptor ssp_isoc_ep_comp;struct list_head urb_list;int enabled;void *hcpriv;};
struct work_struct {void (*function)(struct work_struct *);struct work_struct *next;unsigned pending,running;};
struct giveback_urb_bh {bool running,high_prio;spinlock_t lock;struct list_head head;struct work_struct bh;struct usb_host_endpoint *completing_ep;};
struct usb_tt {struct usb_device *hub;int multi,think_time;void *hcpriv;spinlock_t lock;struct list_head clear_list;struct work_struct clear_work;};
struct usb_tt_clear {struct list_head clear_list;u16 tt,devinfo;struct usb_hcd *hcd;struct usb_host_endpoint *ep;};
struct usb_hub {struct usb_device *hdev;struct usb_tt tt;unsigned long power_bits[1];};
struct usb_device {struct device dev;struct usb_bus *bus;struct usb_device *parent;struct usb_host_endpoint ep0;char devpath[32];unsigned rx_lanes,tx_lanes,ssp_rate;unsigned slot_id,portnum,route;int state,speed,can_submit,devnum,devaddr,do_remote_wakeup,maxchild,persist_enabled,ttport;struct usb_tt *tt;atomic_t references,urbnum;unsigned long toggle[2];struct usb_host_endpoint *ep_in[16],*ep_out[16];};
#include "tmc_xhci_urb_core.h"
#undef spin_lock_irq
#undef spin_unlock_irq
#define spin_lock_irq(s) xhci_spin_lock_irq(s)
#define spin_unlock_irq(s) xhci_spin_unlock_irq(s)
void xhci_runtime_progress(void);
#undef cpu_relax
#define cpu_relax() xhci_runtime_progress()
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
#define HCD_FLAG_POLL_RH 0
#define USB_UHCI_LOADED 0
#define USB_OHCI_LOADED 1
#define USB_XHCI_LOADED 2
typedef int pm_message_t;
struct hc_driver {
	const char	*description;	/* "ehci-hcd" etc */
	const char	*product_desc;	/* product/vendor string */
	size_t		hcd_priv_size;	/* size of private data */

	/* irq handler */
	irqreturn_t	(*irq) (struct usb_hcd *hcd);

	int	flags;
#define	HCD_MEMORY	0x0001		/* HC regs use memory (else I/O) */
#define	HCD_DMA		0x0002		/* HC uses DMA */
#define	HCD_SHARED	0x0004		/* Two (or more) usb_hcds share HW */
#define	HCD_USB11	0x0010		/* USB 1.1 */
#define	HCD_USB2	0x0020		/* USB 2.0 */
#define	HCD_USB3	0x0040		/* USB 3.0 */
#define	HCD_USB31	0x0050		/* USB 3.1 */
#define	HCD_USB32	0x0060		/* USB 3.2 */
#define	HCD_MASK	0x0070
#define	HCD_BH		0x0100		/* URB complete in BH context */

	/* called to init HCD and root hub */
	int	(*reset) (struct usb_hcd *hcd);
	int	(*start) (struct usb_hcd *hcd);

	/* NOTE:  these suspend/resume calls relate to the HC as
	 * a whole, not just the root hub; they're for PCI bus glue.
	 */
	/* called after suspending the hub, before entering D3 etc */
	int	(*pci_suspend)(struct usb_hcd *hcd, bool do_wakeup);

	/* called after entering D0 (etc), before resuming the hub */
	int	(*pci_resume)(struct usb_hcd *hcd, pm_message_t state);

	/* called just before hibernate final D3 state, allows host to poweroff parts */
	int	(*pci_poweroff_late)(struct usb_hcd *hcd, bool do_wakeup);

	/* cleanly make HCD stop writing memory and doing I/O */
	void	(*stop) (struct usb_hcd *hcd);

	/* shutdown HCD */
	void	(*shutdown) (struct usb_hcd *hcd);

	/* return current frame number */
	int	(*get_frame_number) (struct usb_hcd *hcd);

	/* manage i/o requests, device state */
	int	(*urb_enqueue)(struct usb_hcd *hcd,
				struct urb *urb, gfp_t mem_flags);
	int	(*urb_dequeue)(struct usb_hcd *hcd,
				struct urb *urb, int status);

	/*
	 * (optional) these hooks allow an HCD to override the default DMA
	 * mapping and unmapping routines.  In general, they shouldn't be
	 * necessary unless the host controller has special DMA requirements,
	 * such as alignment constraints.  If these are not specified, the
	 * general usb_hcd_(un)?map_urb_for_dma functions will be used instead
	 * (and it may be a good idea to call these functions in your HCD
	 * implementation)
	 */
	int	(*map_urb_for_dma)(struct usb_hcd *hcd, struct urb *urb,
				   gfp_t mem_flags);
	void    (*unmap_urb_for_dma)(struct usb_hcd *hcd, struct urb *urb);

	/* hw synch, freeing endpoint resources that urb_dequeue can't */
	void	(*endpoint_disable)(struct usb_hcd *hcd,
			struct usb_host_endpoint *ep);

	/* (optional) reset any endpoint state such as sequence number
	   and current window */
	void	(*endpoint_reset)(struct usb_hcd *hcd,
			struct usb_host_endpoint *ep);

	/* root hub support */
	int	(*hub_status_data) (struct usb_hcd *hcd, char *buf);
	int	(*hub_control) (struct usb_hcd *hcd,
				u16 typeReq, u16 wValue, u16 wIndex,
				char *buf, u16 wLength);
	int	(*bus_suspend)(struct usb_hcd *);
	int	(*bus_resume)(struct usb_hcd *);
	int	(*start_port_reset)(struct usb_hcd *, unsigned port_num);
	unsigned long	(*get_resuming_ports)(struct usb_hcd *);

		/* force handover of high-speed port to full-speed companion */
	void	(*relinquish_port)(struct usb_hcd *, int);
		/* has a port been handed over to a companion? */
	int	(*port_handed_over)(struct usb_hcd *, int);

		/* CLEAR_TT_BUFFER completion callback */
	void	(*clear_tt_buffer_complete)(struct usb_hcd *,
				struct usb_host_endpoint *);

	/* xHCI specific functions */
		/* Called by usb_alloc_dev to alloc HC device structures */
	int	(*alloc_dev)(struct usb_hcd *, struct usb_device *);
		/* Called by usb_disconnect to free HC device structures */
	void	(*free_dev)(struct usb_hcd *, struct usb_device *);
	/* Change a group of bulk endpoints to support multiple stream IDs */
	int	(*alloc_streams)(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint **eps, unsigned int num_eps,
		unsigned int num_streams, gfp_t mem_flags);
	/* Reverts a group of bulk endpoints back to not using stream IDs.
	 * Can fail if we run out of memory.
	 */
	int	(*free_streams)(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint **eps, unsigned int num_eps,
		gfp_t mem_flags);

	/* Bandwidth computation functions */
	/* Note that add_endpoint() can only be called once per endpoint before
	 * check_bandwidth() or reset_bandwidth() must be called.
	 * drop_endpoint() can only be called once per endpoint also.
	 * A call to xhci_drop_endpoint() followed by a call to
	 * xhci_add_endpoint() will add the endpoint to the schedule with
	 * possibly new parameters denoted by a different endpoint descriptor
	 * in usb_host_endpoint.  A call to xhci_add_endpoint() followed by a
	 * call to xhci_drop_endpoint() is not allowed.
	 */
		/* Allocate endpoint resources and add them to a new schedule */
	int	(*add_endpoint)(struct usb_hcd *, struct usb_device *,
				struct usb_host_endpoint *);
		/* Drop an endpoint from a new schedule */
	int	(*drop_endpoint)(struct usb_hcd *, struct usb_device *,
				 struct usb_host_endpoint *);
		/* Check that a new hardware configuration, set using
		 * endpoint_enable and endpoint_disable, does not exceed bus
		 * bandwidth.  This must be called before any set configuration
		 * or set interface requests are sent to the device.
		 */
	int	(*check_bandwidth)(struct usb_hcd *, struct usb_device *);
		/* Reset the device schedule to the last known good schedule,
		 * which was set from a previous successful call to
		 * check_bandwidth().  This reverts any add_endpoint() and
		 * drop_endpoint() calls since that last successful call.
		 * Used for when a check_bandwidth() call fails due to resource
		 * or bandwidth constraints.
		 */
	void	(*reset_bandwidth)(struct usb_hcd *, struct usb_device *);
		/* Set the hardware-chosen device address */
	int	(*address_device)(struct usb_hcd *, struct usb_device *udev,
				  unsigned int timeout_ms);
		/* prepares the hardware to send commands to the device */
	int	(*enable_device)(struct usb_hcd *, struct usb_device *udev);
		/* Notifies the HCD after a hub descriptor is fetched.
		 * Will block.
		 */
	int	(*update_hub_device)(struct usb_hcd *, struct usb_device *hdev,
			struct usb_tt *tt, gfp_t mem_flags);
	int	(*reset_device)(struct usb_hcd *, struct usb_device *);
		/* Notifies the HCD after a device is connected and its
		 * address is set
		 */
	int	(*update_device)(struct usb_hcd *, struct usb_device *);
	int	(*set_usb2_hw_lpm)(struct usb_hcd *, struct usb_device *, int);
	/* USB 3.0 Link Power Management */
		/* Returns the USB3 hub-encoded value for the U1/U2 timeout. */
	int	(*enable_usb3_lpm_timeout)(struct usb_hcd *,
			struct usb_device *, enum usb3_link_state state);
		/* The xHCI host controller can still fail the command to
		 * disable the LPM timeouts, so this can return an error code.
		 */
	int	(*disable_usb3_lpm_timeout)(struct usb_hcd *,
			struct usb_device *, enum usb3_link_state state);
	int	(*find_raw_port_number)(struct usb_hcd *, int);
	/* Call for power on/off the port if necessary */
	int	(*port_power)(struct usb_hcd *hcd, int portnum, bool enable);
	/* Call for SINGLE_STEP_SET_FEATURE Test for USB2 EH certification */
#define EHSET_TEST_SINGLE_STEP_SET_FEATURE 0x06
	int	(*submit_single_step_set_feature)(struct usb_hcd *,
			struct urb *, int);
};
struct usb_hcd {struct usb_hcd *primary_hcd,*shared_hcd;void *regs,*usb_phy;int has_tt,state,msi_enabled,msix_enabled,speed,can_do_streams;char irq_descr[64];struct usb_bus self;const struct hc_driver *driver;unsigned long flags;int irq,running,accessible,uses_new_polling;void *localmem_pool;u64 rsrc_start,rsrc_len;struct timer_list rh_timer;unsigned root_changes,root_resume,polling,irq_running;unsigned long root_resume_ports;struct giveback_urb_bh high_prio_bh,low_prio_bh;u8 hcd_priv[] __attribute__((aligned(16)));};
static inline struct usb_hcd *bus_to_hcd(struct usb_bus *b){return container_of(b,struct usb_hcd,self);}
#define HCD_HW_ACCESSIBLE(h) ((h)->accessible)
#define HCD_RH_RUNNING(h) ((h)->running)
static inline int is_root_hub(struct usb_device *d){return d&&d->bus->root_hub==d;}
static inline unsigned usb_pipeendpoint(unsigned p){return (p>>15)&15;}
static inline unsigned usb_pipedevice(unsigned p){return (p>>8)&127;}
static inline int usb_pipein(unsigned p){return !!(p&USB_DIR_IN);}
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
void *xhci_calloc(size_t,size_t,gfp_t);
#define kcalloc xhci_calloc
struct pci_dev {struct device dev;u32 address;u16 vendor,device;unsigned irq,revision,msix_enabled;void *driver_data;struct usb_hcd *hcd;};
struct pci_device_id {u32 class,class_mask,vendor,device;};
struct pci_driver {const char *name;const struct pci_device_id *id_table;int (*probe)(struct pci_dev *,const struct pci_device_id *);void (*remove)(struct pci_dev *);void (*shutdown)(struct pci_dev *);struct {int probe_type;const void *pm;} driver;};
#define PCI_DEVICE_CLASS(c,m) .class=(c),.class_mask=(m)
#define PCI_CLASS_SERIAL_USB_XHCI 0x0c0330
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
void xhci_delay_us(unsigned);
#define udelay xhci_delay_us
#define msleep(n) xhci_delay_us((n)*1000u)
u64 tmc_lx_os_clock_ms(void);
/* These imports perform actual I/O; the host fixture emulates hardware. */
u8 tmc_xhci_os_in8(u16);u16 tmc_xhci_os_in16(u16);u32 tmc_xhci_os_in32(u16);
void tmc_xhci_os_out8(u16,u8);void tmc_xhci_os_out16(u16,u16);void tmc_xhci_os_out32(u16,u32);
#define inb(p) tmc_xhci_os_in8(p)
#define inw(p) tmc_xhci_os_in16(p)
#define inl(p) tmc_xhci_os_in32(p)
#define outb(v,p) tmc_xhci_os_out8(p,v)
#define outw(v,p) tmc_xhci_os_out16(p,v)
#define outl(v,p) tmc_xhci_os_out32(p,v)
u32 tmc_xhci_os_pci_next(u32,u32);
u32 tmc_xhci_os_pci_read(u32,unsigned);
void tmc_xhci_os_pci_write(u32,unsigned,u32);
void tmc_xhci_os_delay_us(unsigned);
const char *tmc_xhci_os_board_name(void);
void xhci_log(unsigned,const char *,...);
void tmc_lx_hid_log(void *,unsigned,const char *,...);
struct urb *usb_alloc_urb(int,gfp_t);
void usb_free_urb(struct urb *);
int usb_submit_urb(struct urb *,gfp_t);
void usb_kill_urb(struct urb *);
#undef dev_err
#undef dev_WARN
#undef dev_warn
#undef dev_dbg
#define dev_err(d,f,...) xhci_log(2,f,##__VA_ARGS__)
#define dev_warn(d,f,...) xhci_log(1,f,##__VA_ARGS__)
#define dev_info(d,f,...) xhci_log(0,f,##__VA_ARGS__)
#define dev_WARN(d,f,...) xhci_log(2,f,##__VA_ARGS__)
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
#define module_init(fn) int tmc_linux_xhci_module_init(void){return fn();}
#define module_exit(fn) void tmc_linux_xhci_module_exit(void){fn();}
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
#define XHCI_USBLEGSUP 0xc0
#define XHCI_USBLEGSUP_RWC 0x8f00
#define XHCI_USBLEGSUP_RO 0x5040
#define XHCI_USBCMD 0
#define XHCI_USBCMD_HCRESET 2
#define XHCI_USBCMD_RUN 1
#define XHCI_USBCMD_CONFIGURE 0x40
#define XHCI_USBCMD_EGSM 8
#define XHCI_USBINTR 4
#define XHCI_USBINTR_RESUME 2
#define USB_RESUME_TIMEOUT 40
#define C_HUB_OVER_CURRENT 1
#define C_HUB_LOCAL_POWER 0
#define GetHubStatus 0xa000
#define GetPortStatus 0xa300
#define SetHubFeature 0x2003
#define ClearHubFeature 0x2001
#define SetPortFeature 0x2303
#define ClearPortFeature 0x2301
#define GetHubDescriptor 0xa006

#define strcmp tmc_xhci_strcmp
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

#include "xhci_services.h"

#define hcd_giveback_urb_in_bh(h) (!!((h)->driver->flags & HCD_BH))
#define system_bh_highpri_wq ((void *)1)
#define system_bh_wq ((void *)2)
int xhci_queue_bh(void *,struct work_struct *);
#define queue_work(q,w) xhci_queue_bh(q,w)

#define usb_pipeisoc(p) (usb_pipetype(p)==PIPE_ISOCHRONOUS)

#define __le64 u64
#define __counted_by(n)
#define BIT_ULL(n) (1ull<<(n))
#define cpu_to_le64(x) ((u64)(x))
#define le64_to_cpu(x) ((u64)(x))
typedef int pm_message_t;
struct va_format {const char *fmt;va_list *va;};
struct completion {unsigned done;};
struct delayed_work {struct work_struct work;struct timer_list timer;};
struct radix_tree_root {struct tmc_radix_entry *entries;};
static inline int usb_hcd_is_primary_hcd(struct usb_hcd *h){return !h->primary_hcd;}
static inline u64 lo_hi_readq(const volatile void *p){return readl(p)|((u64)readl((const volatile u8 *)p+4)<<32);}
static inline void lo_hi_writeq(u64 v,volatile void *p){writel((u32)v,p);writel(v>>32,(volatile u8 *)p+4);}
enum usb_link_tunnel_mode {USB_LINK_UNKNOWN,USB_LINK_NATIVE,USB_LINK_TUNNELED};
struct tty_port {unsigned unused;};
#define DECLARE_KFIFO_PTR(n,t) struct {t *data;unsigned in,out,mask;} n
int snprintf(char *,size_t,const char *,...);

static inline int usb_urb_dir_in(const struct urb *u){return !!(u->transfer_flags&URB_DIR_IN);}
static inline int usb_urb_dir_out(const struct urb *u){return !usb_urb_dir_in(u);}
int sprintf(char *,const char *,...);
struct tasklet_struct {void (*function)(struct tasklet_struct *);unsigned pending;};
struct debugfs_regset32 {const void *regs;int nregs;void *base;};
#define U32_MAX 0xffffffffu
#define U16_MAX 0xffffu
#define upper_32_bits(v) ((u32)((u64)(v)>>32))
#define lower_32_bits(v) ((u32)(v))
#define GENMASK_ULL(h,l) ((~0ull<<(l))&(~0ull>>(63-(h))))
#define GENMASK(h,l) ((~0ul<<(l))&(~0ul>>(63-(h))))
#define HC_STATE_SUSPENDED 4
#define HC_STATE_HALT 0
#define HC_STATE_RUNNING 1
#define HCD_FLAG_DEFER_RH_REGISTER 4
#define DMI_PRODUCT_NAME 2
#define DMI_SYS_VENDOR 3
struct iommu_domain {unsigned type;};
#define IOMMU_DOMAIN_IDENTITY 0
static inline struct iommu_domain *iommu_get_domain_for_dev(struct device *d){(void)d;return NULL;}
char *strstr(const char *,const char *);
void add_timer(struct timer_list *);
static inline void *kzalloc_node(size_t n,gfp_t flags,int node){(void)node;return kzalloc(n,flags);}
static inline int dev_to_node(struct device *d){(void)d;return 0;}
enum dma_data_direction {DMA_BIDIRECTIONAL,DMA_TO_DEVICE,DMA_FROM_DEVICE,DMA_NONE};
dma_addr_t dma_map_single(struct device *,void *,size_t,enum dma_data_direction);
void dma_unmap_single(struct device *,dma_addr_t,size_t,enum dma_data_direction);
int dma_mapping_error(struct device *,dma_addr_t);
size_t sg_pcopy_to_buffer(struct scatterlist *,unsigned,void *,size_t,size_t);
size_t sg_pcopy_from_buffer(struct scatterlist *,unsigned,const void *,size_t,size_t);
#define readl_poll_timeout_atomic(addr,val,cond,delay,timeout) ({u64 _start=tmc_xhci_os_clock_ns();int _rc;for(;;){val=readl(addr);if(cond){_rc=0;break;}if(tmc_xhci_os_clock_ns()-_start>=(u64)(timeout)*1000){_rc=-ETIMEDOUT;break;}if(delay)xhci_delay_us(delay);else tmc_lx_os_spin_wait();}_rc;})

#define EAGAIN 11
#define ETIME 62
#define ENOSYS 38
#define GFP_NOIO GFP_KERNEL
#define GFP_NOWAIT GFP_ATOMIC
#define USB_SPEED_UNKNOWN 0
#define USB_SSP_GEN_2x1 1
#define USB_SSP_GEN_2x2 2
static inline int fls(unsigned v){return v?32-__builtin_clz(v):0;}
static inline unsigned hweight32(u32 v){v=v-((v>>1)&0x55555555u);v=(v&0x33333333u)+((v>>2)&0x33333333u);return (((v+(v>>4))&0x0f0f0f0fu)*0x01010101u)>>24;}
static inline unsigned roundup_pow_of_two(unsigned v){return v<=1?1:1u<<fls(v-1);}
static inline void mutex_init(struct mutex *m){memset(m,0,sizeof(*m));}
void wait_for_completion(struct completion *);
void pm_runtime_put_noidle(struct device *);
void pm_runtime_get_noresume(struct device *);
int dma_set_mask(struct device *,u64);
int usb_hcd_map_urb_for_dma(struct usb_hcd *,struct urb *,gfp_t);
void usb_hcd_unmap_urb_for_dma(struct usb_hcd *,struct urb *);

#define BUILD_BUG_ON(c) _Static_assert(!(c),"upstream ABI invariant")
#define WARN_ON_ONCE(c) WARN_ON(c)
#define array_size(a,b) ({size_t _n;__builtin_mul_overflow((size_t)(a),(size_t)(b),&_n)?SIZE_MAX:_n;})
#define clamp_val(v,l,h) min(max(v,l),h)
static inline void *kcalloc_node(size_t n,size_t size,gfp_t flags,int node){(void)node;return kcalloc(n,size,flags);}
void *radix_tree_lookup(struct radix_tree_root *,unsigned long);
void *radix_tree_delete(struct radix_tree_root *,unsigned long);
int radix_tree_insert(struct radix_tree_root *,unsigned long,void *);
int radix_tree_maybe_preload(gfp_t);
void radix_tree_preload_end(void);
#define INIT_RADIX_TREE(r,flags) ((r)->entries=NULL)
void init_completion(struct completion *);
#define reinit_completion init_completion
void complete(struct completion *);
void complete_all(struct completion *);
unsigned long wait_for_completion_timeout(struct completion *,unsigned long);
int cancel_delayed_work_sync(struct delayed_work *);
int cancel_delayed_work(struct delayed_work *);
int mod_delayed_work(void *,struct delayed_work *,unsigned long);
void xhci_init_delayed_work(struct delayed_work *,void (*)(struct work_struct *));
#define INIT_DELAYED_WORK(w,f) xhci_init_delayed_work(w,f)
#define to_delayed_work(w) container_of(w,struct delayed_work,work)
#define delayed_work_pending(w) ((w)->timer.pending||(w)->work.pending)
#define system_wq ((void *)3)
#define list_first_entry_or_null(h,t,m) (list_empty(h)?NULL:list_first_entry(h,t,m))
void usb_wakeup_notification(struct usb_device *,unsigned);
#define le64_to_cpus(p) do{}while(0) /* x86 little-endian in-place conversion */
#define dev_dbg_ratelimited dev_dbg
#define roundup(v,n) ((((v)+(n)-1)/(n))*(n))
#define rounddown(v,n) (((v)/(n))*(n))
#define FIELD_PREP(mask,val) (((u64)(val)<<__builtin_ctzll(mask))&(mask))
static inline void hi_lo_writeq(u64 v,volatile void *p){writel(v>>32,(volatile u8 *)p+4);writel(v,p);}
#define ffs(v) __builtin_ffs(v)

#define __must_hold(x)
static inline bool usb_acpi_power_manageable(struct usb_device *d,int p){(void)d;(void)p;return false;}
static inline int usb_acpi_set_power_state(struct usb_device *d,int p,bool on){(void)d;(void)p;(void)on;return -ENODEV;}
void pm_runtime_forbid(struct device *);
void pm_runtime_allow(struct device *);
#define DeviceRequest ((USB_DIR_IN|USB_TYPE_STANDARD|USB_RECIP_DEVICE)<<8)
#define put_unaligned(v,p) do{__typeof__(*(p)) _v=(v);memcpy(p,&_v,sizeof(_v));}while(0)

#include "../../../vendor/linux-v6.12/xhci-pci-ids.h"

#define PCI_IRQ_MSIX 1
#define PCI_IRQ_MSI 2
#define IRQF_SHARED 1
#define PCI_D0 0
#define PCI_D3hot 3
#define PMSG_SUSPEND 0
#define pm_ptr(x) NULL
#define EXPORT_SYMBOL_NS_GPL(a,b)
struct usb_hcd *usb_create_shared_hcd(const struct hc_driver *,struct device *,const char *,struct usb_hcd *);
int usb_add_hcd(struct usb_hcd *,unsigned,unsigned long);
void usb_remove_hcd(struct usb_hcd *);
void usb_put_hcd(struct usb_hcd *);
irqreturn_t usb_hcd_irq(int,void *);
unsigned num_online_cpus(void);
int pci_irq_vector(struct pci_dev *,unsigned);
int pci_alloc_irq_vectors(struct pci_dev *,unsigned,unsigned,unsigned);
void pci_free_irq_vectors(struct pci_dev *);
int request_irq(unsigned,irqreturn_t (*)(int,void *),unsigned long,const char *,void *);
void free_irq(unsigned,void *);
const char *pci_name(struct pci_dev *);
struct reset_control *devm_reset_control_get_optional_exclusive(struct device *,const char *);
int reset_control_reset(struct reset_control *);
int pci_choose_state(struct pci_dev *,pm_message_t);
int pci_set_power_state(struct pci_dev *,int);
void pci_d3cold_disable(struct pci_dev *);
int pm_runtime_get(struct device *);
int pm_runtime_put(struct device *);
int dma_set_max_seg_size(struct device *,unsigned);
struct property_entry {const char *name;bool value;};
#define PROPERTY_ENTRY_BOOL(n) {.name=n,.value=true}
struct resource {u64 start,end;const char *name;unsigned flags;};
struct platform_device {struct device dev;struct resource *resources;unsigned count;const char *name;int id;};
#define PLATFORM_DEVID_NONE -1
#define IORESOURCE_MEM 0x200
struct platform_device *platform_device_alloc(const char *,int);
int platform_device_add_resources(struct platform_device *,const struct resource *,unsigned);
int platform_device_add(struct platform_device *);
void platform_device_put(struct platform_device *);
void platform_device_unregister(struct platform_device *);
int device_create_managed_software_node(struct device *,const struct property_entry *,const void *);
int devm_add_action_or_reset(struct device *,void (*)(void *),void *);

int pci_write_config_dword(struct pci_dev *,int,u32);
int tmc_linux_xhci_core_init(void);void tmc_linux_xhci_core_exit(void);int tmc_linux_xhci_pci_init(void);void tmc_linux_xhci_pci_exit(void);

#define CONFIG_HAS_DMA 1
#define snprintf tmc_lx_snprintf

int tmc_lx_snprintf(char *,size_t,const char *,...);
struct seq_file;
