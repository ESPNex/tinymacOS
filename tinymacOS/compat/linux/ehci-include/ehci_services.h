#pragma once
#define HCD_MEMORY 4
#define HCD_USB2 8
#define HCD_BH 16
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define KERN_WARNING ""
#define KERN_INFO ""
#define printk(...) ehci_log(1,__VA_ARGS__)
#define DECLARE_BITMAP(n,b) unsigned long n[((b)+63)/64]
typedef long long ktime_t;
enum hrtimer_restart {HRTIMER_NORESTART,HRTIMER_RESTART};
struct hrtimer {struct hrtimer *next;ktime_t expires;enum hrtimer_restart (*function)(struct hrtimer *);unsigned pending,running;};
struct platform_driver;
int platform_register_drivers(struct platform_driver * const *,unsigned);
void platform_unregister_drivers(struct platform_driver * const *,unsigned);

typedef i64 s64;
#define __maybe_unused __attribute__((unused))
#define NSEC_PER_MSEC 1000000LL
#define NSEC_PER_USEC 1000LL
#define CLOCK_MONOTONIC 1
#define HRTIMER_MODE_ABS 0
#define ktime_add(a,b) ((a)+(b))
#define ktime_compare(a,b) (((a)>(b))-((a)<(b)))
u64 tmc_ehci_os_clock_ns(void);
static inline ktime_t ktime_get(void){return (ktime_t)tmc_ehci_os_clock_ns();}
ktime_t ktime_get_real(void);
void hrtimer_init(struct hrtimer *,int,int);
void hrtimer_start_range_ns(struct hrtimer *,ktime_t,unsigned long,int);
int hrtimer_cancel(struct hrtimer *);
#define list_first_entry(h,t,m) list_entry((h)->next,t,m)
#define list_for_each_safe(p,n,h) for(p=(h)->next,n=p->next;p!=(h);p=n,n=p->next)
static inline void lx_splice(struct list_head *l,struct list_head *prev,struct list_head *next){if(list_empty(l))return;struct list_head *first=l->next,*last=l->prev;first->prev=prev;prev->next=first;last->next=next;next->prev=last;}
static inline void list_splice(struct list_head *l,struct list_head *h){lx_splice(l,h,h->next);}
static inline void list_splice_tail(struct list_head *l,struct list_head *h){lx_splice(l,h->prev,h);}
static inline void list_splice_tail_init(struct list_head *l,struct list_head *h){list_splice_tail(l,h);INIT_LIST_HEAD(l);}
static inline int list_is_singular(struct list_head *h){return !list_empty(h)&&h->next==h->prev;}
#define BIT(n) (1ul<<(n))
#define for_each_set_bit(n,p,size) for(n=0;n<(size);n++)if(test_bit(n,p))
#define HUB_CHAR_INDV_PORT_LPSM 1
#define USB_PORT_STAT_HIGH_SPEED 1024
#define USB_PORT_FEAT_TEST 21
struct usb_hub_descriptor {u8 bDescLength,bDescriptorType,bNbrPorts;u16 wHubCharacteristics;u8 bPwrOn2PwrGood,bHubContrCurrent;union {struct {u8 DeviceRemovable[4];u8 PortPwrCtrlMask[4];} hs;}u;} PACK;
static inline u32 le32_to_cpup(const u32 *p){return *p;}
u32 tmc_ehci_os_read32(const volatile void *);
void tmc_ehci_os_write32(volatile void *,u32);
#define readl(p) tmc_ehci_os_read32(p)
#define writel(v,p) tmc_ehci_os_write32(p,v)
static inline void put_unaligned_le32(u32 v,void *p){memcpy(p,&v,4);}
#define usleep_range(a,b) ehci_delay_us(a)
void tmc_ehci_os_fatal(void) __attribute__((noreturn));
#define BUG() tmc_ehci_os_fatal()
#define BUG_ON(c) do{if(c)BUG();}while(0)
#define usb_pipeint(p) (usb_pipetype(p)==PIPE_INTERRUPT)
#define usb_pipecontrol(p) (usb_pipetype(p)==PIPE_CONTROL)
int usb_hub_clear_tt_buffer(struct urb *);
#define ECOMM 70
#define HS_USECS(n) NS_TO_US(HS_NSECS(n))
#define HS_USECS_ISO(n) NS_TO_US(HS_NSECS_ISO(n))
static inline unsigned long rounddown_pow_of_two(unsigned long n){return n?1ul<<(63-__builtin_clzl(n)):0;}
#define ERR_PTR(n) ((void *)(long)(n))
#define PTR_ERR(p) ((long)(p))
#define IS_ERR(p) ((unsigned long)(p)>=(unsigned long)-4095)
#define max(a,b) ((a)>(b)?(a):(b))
#define rmb() __atomic_thread_fence(__ATOMIC_ACQUIRE)
static inline void *dev_get_drvdata(struct device *d){return to_pci_dev(d)->driver_data;}
#define scnprintf tmc_ehci_scnprintf
#define sscanf tmc_ehci_sscanf
#define kstrtouint tmc_ehci_kstrtouint
int scnprintf(char *,size_t,const char *,...);
int sscanf(const char *,const char *,...);
int kstrtouint(const char *,unsigned,unsigned *);
struct ehci_attribute {struct device_attribute attr;ssize_t (*store)(struct device *,struct device_attribute *,const char *,size_t);};
#define DEVICE_ATTR_RW(n) struct ehci_attribute dev_attr_##n={ {#n,n##_show},n##_store}
/* Private typed registry, not a fake Linux sysfs VFS. Original methods are
 * retained and exposed by checked host/kernel read/store APIs. */
int tmc_ehci_create_attribute(struct device *,struct ehci_attribute *);
void tmc_ehci_remove_attribute(struct device *,struct ehci_attribute *);
#define device_create_file(d,a) tmc_ehci_create_attribute(d,a)
#define device_remove_file(d,a) tmc_ehci_remove_attribute(d,a)
extern unsigned ehci_cf_port_reset_rwsem;
void down_write(unsigned *);
void up_write(unsigned *);
long schedule_timeout_uninterruptible(long);
int hcd_periodic_completion_in_progress(struct usb_hcd *,struct usb_host_endpoint *);

#define PCI_VENDOR_ID_ATI		0x1002
#define PCI_VENDOR_ID_AMD		0x1022
#define PCI_VENDOR_ID_NEC		0x1033
#define PCI_VENDOR_ID_STMICRO		0x104A
#define PCI_DEVICE_ID_STMICRO_USB_HOST	0xCC00
#define PCI_VENDOR_ID_NVIDIA			0x10de
#define PCI_VENDOR_ID_TOSHIBA_2		0x102f
#define PCI_VENDOR_ID_TDI               0x192E
#define PCI_DEVICE_ID_TDI_EHCI          0x0101
#define PCI_VENDOR_ID_HUAWEI		0x19e5
#define PCI_VENDOR_ID_NETMOS		0x9710
#define __read_mostly
#define __initconst
#define PCI_DEVICE(v,d) .vendor=(v),.device=(d)
#define PCI_VDEVICE(v,d) PCI_DEVICE(PCI_VENDOR_ID_##v,d)
#define PCI_CAP_ID_DBG 0x0a
#define PROBE_PREFER_ASYNCHRONOUS 1
const struct pci_device_id *pci_match_id(const struct pci_device_id *,struct pci_dev *);
int pci_set_mwi(struct pci_dev *);
void pci_clear_mwi(struct pci_dev *);
#define DMA_BIT_MASK(n) ((n)==64?~0ull:((1ull<<(n))-1))
int dma_set_coherent_mask(struct device *,u64);
int pci_read_config_byte(struct pci_dev *,int,u8 *);
int pci_read_config_dword(struct pci_dev *,int,u32 *);
int pci_find_capability(struct pci_dev *,int);
static inline bool device_can_wakeup(struct device *d){return d->wakeup_capable;}
void usb_hcd_pci_shutdown(struct pci_dev *);
u64 tmc_ehci_os_clock_ns(void);
int tmc_linux_ehci_pci_init(void);
void tmc_linux_ehci_pci_exit(void);

#define ENODEV 19
#define USB_ENDPOINT_XFERTYPE_MASK 3
#define HUB_CLEAR_TT_BUFFER 8
#define USB_RT_PORT 0x23
#define USB_RT_HUB 0x20
#define usb_sndctrlpipe(d,ep) ((PIPE_CONTROL<<30)|((d)->devnum<<8)|((ep)<<15))
int usb_control_msg(struct usb_device *,unsigned,u8,u8,u16,u16,void *,u16,int);
int usb_set_port_feature(struct usb_device *,int,int);
int usb_clear_port_feature(struct usb_device *,int,int);
int schedule_work(struct work_struct *);
static void ehci_dispatch_work(void);

#define set_port_feature usb_set_port_feature
