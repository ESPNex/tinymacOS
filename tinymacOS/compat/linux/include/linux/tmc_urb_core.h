/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <limits.h>
#define EPERM 1
#define ENOEXEC 8
#define EIDRM 43
#define EBADR 53
#define EXDEV 18
#define EMSGSIZE 90
#define EOPNOTSUPP 95
#define EINPROGRESS 115
#define EREMOTEIO 121
#define USB_STATE_UNAUTHENTICATED 5
#define USB_STATE_CONFIGURED 7
#define USB_SPEED_LOW 1
#define USB_SPEED_FULL 2
#define USB_SPEED_HIGH 3
#define USB_SPEED_SUPER 5
#define USB_SPEED_SUPER_PLUS 6
#define USB_DIR_IN 0x80
#define USB_ENDPOINT_XFER_CONTROL 0
#define USB_ENDPOINT_XFER_ISOC 1
#define USB_ENDPOINT_XFER_BULK 2
#define USB_ENDPOINT_XFER_INT 3
#define PIPE_ISOCHRONOUS 0
#define PIPE_INTERRUPT 1
#define PIPE_CONTROL 2
#define PIPE_BULK 3
/* Linux USB flags, including core-internal mapping bits. */
#define URB_SHORT_NOT_OK 0x0001
#define URB_ISO_ASAP 0x0002
#define URB_ZERO_PACKET 0x0040
#define URB_NO_INTERRUPT 0x0080
#define URB_FREE_BUFFER 0x0100
#define URB_DIR_IN 0x0200
#define URB_DIR_OUT 0
#define URB_DIR_MASK URB_DIR_IN
#define URB_DMA_MAP_SINGLE 0x00010000
#define URB_DMA_MAP_PAGE 0x00020000
#define URB_DMA_MAP_SG 0x00040000
#define URB_MAP_LOCAL 0x00080000
#define URB_SETUP_MAP_SINGLE 0x00100000
#define URB_SETUP_MAP_LOCAL 0x00200000
#define URB_DMA_SG_COMBINED 0x00400000
#define USB_SS_MULT(a) (1+((a)&3))
#define USB_SS_SSP_ISOC_COMP(a) ((a)&0x80)
#define le32_to_cpu(x) (x)
#define usb_pipetype(p) ((unsigned)(p)>>30)
#define usb_pipeout(p) (!((p)&USB_DIR_IN))
static inline struct usb_host_endpoint *usb_pipe_endpoint(struct usb_device *d,unsigned pipe)
{
 unsigned ep=(pipe>>15)&15;return (pipe&USB_DIR_IN)?d->ep_in[ep]:d->ep_out[ep];
}
static inline int usb_endpoint_type(const struct usb_endpoint_descriptor *e){return e->bmAttributes&3;}
static inline int usb_endpoint_dir_out(const struct usb_endpoint_descriptor *e){return !(e->bEndpointAddress&128);}
static inline int usb_endpoint_maxp(const struct usb_endpoint_descriptor *e){return e->wMaxPacketSize&0x7ff;}
static inline int usb_endpoint_maxp_mult(const struct usb_endpoint_descriptor *e){return 1+((e->wMaxPacketSize>>11)&3);}
static inline int usb_endpoint_num(const struct usb_endpoint_descriptor *e){return e->bEndpointAddress&15;}
#define for_each_sg(first,p,n,i) for((i)=0,(p)=(first);(i)<(n);(i)++,(p)=(p)->next)
static inline size_t tmc_lx_struct_size(size_t base,size_t elem,size_t n)
{
 size_t bytes;if(__builtin_mul_overflow(elem,n,&bytes)||__builtin_add_overflow(base,bytes,&bytes))return SIZE_MAX;return bytes;
}
#define struct_size(p,m,n) tmc_lx_struct_size(sizeof(*(p)),sizeof((p)->m[0]),(size_t)(n))
#define unlikely(x) __builtin_expect(!!(x),0)
#define likely(x) __builtin_expect(!!(x),1)
#define min(a,b) ((a)<(b)?(a):(b))
#define ilog2(n) (31-__builtin_clz((unsigned)(n)))
#define fallthrough __attribute__((fallthrough))
#define EXPORT_SYMBOL_GPL(name)
/* Optional KMSAN instrumentation is disabled, as in a non-KMSAN Linux build;
 * this does not replace any transfer, validation or scheduling operation. */
static inline void kmsan_handle_urb(struct urb *u,int out){(void)u;(void)out;}
#define WARN_ONCE(cond,fmt,...) ({int _v=!!(cond);static unsigned _warned;if(_v&&!__atomic_exchange_n(&_warned,1,__ATOMIC_RELAXED))tmc_lx_hid_log(0,1,"USB core: " fmt,##__VA_ARGS__);_v;})
#define dev_WARN_ONCE(dev,cond,fmt,...) WARN_ONCE(cond,fmt,##__VA_ARGS__)
#define dev_WARN(dev,fmt,...) tmc_lx_hid_log(dev,1,"USB core: " fmt,##__VA_ARGS__)
#define dev_dbg(dev,fmt,...) do {(void)(dev);}while(0)
void usb_init_urb(struct urb *);
struct urb *usb_get_urb(struct urb *);
#define usb_put_urb usb_free_urb
void usb_anchor_urb(struct urb *,struct usb_anchor *);
void usb_unanchor_urb(struct urb *);
void usb_anchor_suspend_wakeups(struct usb_anchor *);
void usb_anchor_resume_wakeups(struct usb_anchor *);
void usb_kill_anchored_urbs(struct usb_anchor *);
void usb_poison_anchored_urbs(struct usb_anchor *);
void usb_unpoison_anchored_urbs(struct usb_anchor *);
void usb_unlink_anchored_urbs(struct usb_anchor *);
void usb_scuttle_anchored_urbs(struct usb_anchor *);
int usb_wait_anchor_empty_timeout(struct usb_anchor *,unsigned);
int usb_anchor_empty(struct usb_anchor *);
struct urb *usb_get_from_anchor(struct usb_anchor *);
int usb_pipe_type_check(struct usb_device *,unsigned);
int usb_urb_ep_type_check(const struct urb *);
int usb_unlink_urb(struct urb *);
void usb_poison_urb(struct urb *);
void usb_unpoison_urb(struct urb *);
void usb_block_urb(struct urb *);
int usb_hcd_submit_urb(struct urb *,gfp_t);
int usb_hcd_unlink_urb(struct urb *,int);
void tmc_lx_native_giveback(struct urb *,int,unsigned);
static inline void init_usb_anchor(struct usb_anchor *a)
{
 INIT_LIST_HEAD(&a->urb_list);spin_lock_init(&a->lock);a->wait.sequence=0;
 atomic_set(&a->suspend_wakeups,0);a->poisoned=0;
}
void tmc_lx_spin_lock_irq(spinlock_t *);
void tmc_lx_spin_unlock_irq(spinlock_t *);
#define spin_lock_irq(l) tmc_lx_spin_lock_irq(l)
#define spin_unlock_irq(l) tmc_lx_spin_unlock_irq(l)
#define smp_mb__after_atomic() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define cpu_relax() tmc_lx_os_spin_wait()
void tmc_lx_os_spin_wait(void);
u64 tmc_lx_os_clock_ms(void);
/* Polled-runtime waits: predicates are evaluated repeatedly with acquire
 * atomics, native TLB progress and timeout from the OS monotonic clock.
 * No fake success, fixed-iteration timeout or discarded wakeup operation.
 * Current serialized native adapters retire URBs synchronously on unlink;
 * this is not a general Linux task scheduler or sleepable waitqueue port.
 */
extern wait_queue_head_t usb_kill_urb_queue;
static inline void wake_up(wait_queue_head_t *q){__atomic_add_fetch(&q->sequence,1,__ATOMIC_RELEASE);}
/* Optional CONFIG_DEBUG_ATOMIC_SLEEP instrumentation is disabled. This is
 * not a replacement for the missing general Linux sleepable scheduler. */
static inline void tmc_lx_might_sleep(void){}
#define might_sleep() tmc_lx_might_sleep()
#define msecs_to_jiffies(n) ((u64)(n)) /* wait time unit in this port is milliseconds */
#define wait_event(q,condition) do {while(!(condition)){(void)__atomic_load_n(&(q).sequence,__ATOMIC_ACQUIRE);cpu_relax();}}while(0)
#define wait_event_timeout(q,condition,timeout) ({u64 _ms=(timeout),_start=tmc_lx_os_clock_ms(),_now=_start;long _left=0;for(;;){if(condition){_left=_now-_start<_ms?(long)(_ms-(_now-_start)):1;break;}if(_now-_start>=_ms)break;(void)__atomic_load_n(&(q).sequence,__ATOMIC_ACQUIRE);cpu_relax();_now=tmc_lx_os_clock_ms();}_left;})
