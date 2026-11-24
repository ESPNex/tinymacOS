/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
/* Private OS service ABI for one canonical, ORIGINAL Linux USB device model.
 * Configuration-disabled facilities follow upstream off-config semantics.
 * Runtime operations are declarations here, never successful placeholders. */
#include "base.h"
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
#include <stddef.h>
#define __KERNEL__ 1
#define CONFIG_USB 1
#define CONFIG_USB_DEFAULT_AUTHORIZATION_MODE 1
#define CONFIG_USB_PCI 1
#define CONFIG_HAS_DMA 1
#define CONFIG_HAS_IOPORT 1
#define _K_ARG(_0,_1,...) _1
#define _K_PLACE_1 0,
#define _K_VALUE(x) _K_VALUE2(_K_PLACE_##x)
#define _K_VALUE2(x) _K_ARG(x 1,0)
#define IS_ENABLED(x) _K_VALUE(x)
#define IS_REACHABLE(x) IS_ENABLED(x)
#define __iomem
#define __user
#define __force
#define __init
#define __exit
#define __initconst
#define __read_mostly
#define __maybe_unused __attribute__((unused))
#define __always_unused __attribute__((unused))
#define __must_check __attribute__((warn_unused_result))
#define __weak __attribute__((weak))
#define __acquires(x)
#define __releases(x)
#define __printf(a,b) __attribute__((format(printf,a,b)))
#define __aligned(x) __attribute__((aligned(x)))
#define __packed __attribute__((packed))
#define __counted_by(x)
#define __DECLARE_FLEX_ARRAY(t,n) struct {struct {} __empty_##n;t n[];}
#define static_assert _Static_assert
#define BUILD_BUG_ON(c) _Static_assert(!(c),"Linux ABI invariant")
#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)
#define EXPORT_SYMBOL_NS_GPL(x,ns)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_LICENSE(x)
#define MODULE_PARM_DESC(n,s)
#define MODULE_DEVICE_TABLE(t,n)
#define MODULE_ALIAS(x)
#define module_param_named(n,v,t,p)
#define module_param(n,t,p)
#define module_param_string(n,v,s,p)
#define MODULE_VERSION(x)
#define KBUILD_MODNAME "usbcore"
#define THIS_MODULE ((struct module *)0)
struct module {int unused;};
typedef u8 __u8;typedef u16 __u16;typedef u32 __u32;typedef u64 __u64;
typedef s8 __s8;typedef int16_t __s16;typedef int32_t __s32;typedef int64_t __s64;
typedef int16_t s16;typedef int32_t s32;typedef int64_t s64;
typedef u16 __le16;typedef u32 __le32;typedef u64 __le64;
typedef u16 __be16;typedef u32 __be32;
typedef u64 dma_addr_t;typedef u64 resource_size_t;typedef unsigned gfp_t;
typedef long ssize_t;typedef unsigned short umode_t;typedef u32 dev_t;
typedef struct {u8 b[16];} uuid_t;
typedef struct {u8 b[16];} guid_t;
typedef struct {u8 b[16];} uuid_le;
#define MEI_CL_NAME_SIZE 32
#define UUID_STRING_LEN 36
#define UUID_SIZE 16
#define GFP_KERNEL 0u
#define GFP_ATOMIC 1u
#define GFP_NOIO 0u
#define GFP_NOWAIT 1u
#define GFP_KERNEL_ACCOUNT 0u
#define __GFP_ZERO 2u
#define PAGE_SIZE 4096u
#define PAGE_SHIFT 12
#define SZ_1K 1024
#define U8_MAX 255
#define U16_MAX 65535
#define U32_MAX 0xffffffffu
#define EOPNOTSUPP 95
#define ENOSYS 38
#define ENODEV 19
#define ENOMEM 12
#define EINVAL 22
#define ENOENT 2
#define EBUSY 16
#define EEXIST 17
#define EIO 5
#define EPIPE 32
#define EAGAIN 11
#define ENOSPC 28
#define ENOMSG 42
#define EINTR 4
#define EOVERFLOW 75
#define ETIMEDOUT 110
#define ESHUTDOWN 108
#define ECONNRESET 104
#define EINPROGRESS 115
#define ENOTCONN 107
#define EHOSTUNREACH 113
#define EREMOTEIO 121
#define EPROTO 71
#define EILSEQ 84
#define ENODATA 61
#define ENOEXEC 8
#define EPERM 1
#define EACCES 13
#define EXDEV 18
#define EFBIG 27
#define ENXIO 6
#define ENOTTY 25
#define EIDRM 43
#define EBADR 53
#define EMSGSIZE 90
#define ENFILE 23
#define ERESTARTSYS 512
#define EPROBE_DEFER 517
#define EHOSTDOWN 112
#define ENOMEDIUM 123
#define ETIME 62
#define ECOMM 70
#define E2BIG 7
#define EALREADY 114
#define cpu_to_le16(v) ((u16)(v))
#define cpu_to_le32(v) ((u32)(v))
#define cpu_to_le64(v) ((u64)(v))
#define le16_to_cpu(v) (v)
#define le32_to_cpu(v) (v)
#define le64_to_cpu(v) (v)
#define __le16_to_cpu(v) (v)
#define cpu_to_be32(v) __builtin_bswap32(v)
#define be32_to_cpu(v) __builtin_bswap32(v)
#define le32_to_cpup(p) (*(p))
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define container_of_const(p,t,m) _Generic((p),const __typeof__(((t *)0)->m) *:((const t *)((const char *)(p)-offsetof(t,m))),default:container_of(p,t,m))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define min_t(t,a,b) min((t)(a),(t)(b))
#define max_t(t,a,b) max((t)(a),(t)(b))
#define clamp(v,l,h) min(max(v,l),h)
#define clamp_t(t,v,l,h) clamp((t)(v),(t)(l),(t)(h))
#define clamp_val(v,l,h) clamp(v,l,h)
#define ALIGN(v,a) (((v)+(a)-1)&~((a)-1))
#define IS_ALIGNED(v,a) (!((v)&((a)-1)))
#define DIV_ROUND_UP(n,d) (((n)+(d)-1)/(d))
#define DIV_ROUND_CLOSEST(n,d) (((n)+(d)/2)/(d))
#define likely(x) __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)
#define BIT(n) (1UL<<(n))
#define BIT_ULL(n) (1ULL<<(n))
#define BITS_PER_LONG 64
#define BITS_TO_LONGS(n) DIV_ROUND_UP(n,64)
#define DECLARE_BITMAP(n,b) unsigned long n[BITS_TO_LONGS(b)]
#define GENMASK(h,l) ((~0UL<<(l))&(~0UL>>(63-(h))))
#define FIELD_GET(mask,v) (((v)&(mask))>>__builtin_ctzl(mask))
#define FIELD_PREP(mask,v) (((v)<<__builtin_ctzl(mask))&(mask))
#define lower_32_bits(v) ((u32)(v))
#define upper_32_bits(v) ((u32)((u64)(v)>>32))
#define round_up(n,d) ALIGN(n,d)
#define round_down(n,d) ((n)&~((d)-1))
#define roundup(n,d) (DIV_ROUND_UP(n,d)*(d))
#define __ffs(v) __builtin_ctzl((unsigned long)(v))
#define ffs(v) __builtin_ffs(v)
#define fls(v) ((v)?32-__builtin_clz((unsigned)(v)):0)
#define ilog2(v) (63-__builtin_clzl((unsigned long)(v)))
#define test_bit(n,p) (!!(__atomic_load_n((p)+(n)/64,__ATOMIC_ACQUIRE)&BIT((n)%64)))
#define set_bit(n,p) __atomic_fetch_or((p)+(n)/64,BIT((n)%64),__ATOMIC_ACQ_REL)
#define clear_bit(n,p) __atomic_fetch_and((p)+(n)/64,~BIT((n)%64),__ATOMIC_ACQ_REL)
#define test_and_set_bit(n,p) (!!(set_bit(n,p)&BIT((n)%64)))
#define test_and_clear_bit(n,p) (!!(clear_bit(n,p)&BIT((n)%64)))
#define for_each_set_bit(n,p,size) for(n=0;n<(size);n++)if(test_bit(n,p))
#define READ_ONCE(v) __atomic_load_n(&(v),__ATOMIC_ACQUIRE)
#define WRITE_ONCE(v,x) __atomic_store_n(&(v),(x),__ATOMIC_RELEASE)
#define smp_mb() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define smp_rmb() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define smp_wmb() __atomic_thread_fence(__ATOMIC_RELEASE)
#define smp_mb__after_atomic smp_mb
#define smp_mb__before_atomic smp_mb
#define barrier() __asm__ volatile("":::"memory")
#define fallthrough __attribute__((fallthrough))
#define ERR_PTR(v) ((void *)(long)(v))
#define PTR_ERR(p) ((long)(p))
#define IS_ERR(p) ((unsigned long)(p)>=(unsigned long)-4095)
#define IS_ERR_OR_NULL(p) (!(p)||IS_ERR(p))
#define PTR_ERR_OR_ZERO(p) (IS_ERR(p)?PTR_ERR(p):0)
#define u64_to_user_ptr(v) ((void *)(unsigned long)(v))
struct list_head {struct list_head *next,*prev;};
#define LIST_HEAD_INIT(n) {&(n),&(n)}
#define LIST_HEAD(n) struct list_head n=LIST_HEAD_INIT(n)
static inline void INIT_LIST_HEAD(struct list_head *h){h->next=h->prev=h;}
static inline int list_empty(const struct list_head *h){return h->next==h;}
static inline void list_add(struct list_head *n,struct list_head *h){n->next=h->next;n->prev=h;h->next->prev=n;h->next=n;}
static inline void list_add_tail(struct list_head *n,struct list_head *h){list_add(n,h->prev);}
static inline void list_del(struct list_head *n){n->prev->next=n->next;n->next->prev=n->prev;n->next=n->prev=NULL;}
static inline void list_del_init(struct list_head *n){list_del(n);INIT_LIST_HEAD(n);}
static inline void list_move(struct list_head *n,struct list_head *h){list_del(n);list_add(n,h);}
static inline void list_move_tail(struct list_head *n,struct list_head *h){list_del(n);list_add_tail(n,h);}
#define list_entry(p,t,m) container_of(p,t,m)
#define list_first_entry(h,t,m) list_entry((h)->next,t,m)
#define list_last_entry(h,t,m) list_entry((h)->prev,t,m)
#define list_first_entry_or_null(h,t,m) (list_empty(h)?NULL:list_first_entry(h,t,m))
#define list_for_each(p,h) for(p=(h)->next;p!=(h);p=p->next)
#define list_for_each_safe(p,n,h) for(p=(h)->next,n=p->next;p!=(h);p=n,n=p->next)
#define list_for_each_entry(p,h,m) for(p=list_entry((h)->next,__typeof__(*p),m);&(p)->m!=(h);p=list_entry((p)->m.next,__typeof__(*p),m))
#define list_for_each_entry_safe(p,n,h,m) for(p=list_entry((h)->next,__typeof__(*p),m),n=list_entry(p->m.next,__typeof__(*p),m);&(p)->m!=(h);p=n,n=list_entry(n->m.next,__typeof__(*n),m))
#define list_for_each_entry_reverse(p,h,m) for(p=list_entry((h)->prev,__typeof__(*p),m);&(p)->m!=(h);p=list_entry((p)->m.prev,__typeof__(*p),m))
static inline void list_splice_tail_init(struct list_head *l,struct list_head *h){if(!list_empty(l)){struct list_head *a=l->next,*b=l->prev;a->prev=h->prev;h->prev->next=a;b->next=h;h->prev=b;INIT_LIST_HEAD(l);}}
typedef struct {int counter;} atomic_t;
#define ATOMIC_INIT(n) {n}
#define atomic_read(p) __atomic_load_n(&(p)->counter,__ATOMIC_ACQUIRE)
#define atomic_set(p,n) __atomic_store_n(&(p)->counter,n,__ATOMIC_RELEASE)
#define atomic_inc(p) __atomic_add_fetch(&(p)->counter,1,__ATOMIC_ACQ_REL)
#define atomic_dec(p) __atomic_sub_fetch(&(p)->counter,1,__ATOMIC_ACQ_REL)
#define atomic_inc_return atomic_inc
#define atomic_dec_return atomic_dec
#define atomic_dec_and_test(p) (atomic_dec(p)==0)
typedef struct {atomic_t refs;} refcount_t;
#define refcount_read(p) atomic_read(&(p)->refs)
#define refcount_set(p,v) atomic_set(&(p)->refs,v)
struct kref {refcount_t refcount;};
static inline void kref_init(struct kref *r){refcount_set(&r->refcount,1);}
static inline void kref_get(struct kref *r){atomic_inc(&r->refcount.refs);}
static inline int kref_put(struct kref *r,void (*release)(struct kref *)){if(atomic_dec_and_test(&r->refcount.refs)){release(r);return 1;}return 0;}
typedef struct {unsigned word;u32 irq_flags;} spinlock_t;
struct mutex {spinlock_t lock;u32 irq_flags;};
struct rw_semaphore {unsigned state;};
#define DEFINE_MUTEX(n) struct mutex n={0}
#define DEFINE_SPINLOCK(n) spinlock_t n={0}
#define DECLARE_RWSEM(n) struct rw_semaphore n={0}
void k_spin_lock(spinlock_t *),k_spin_unlock(spinlock_t *);
u32 k_irq_save(void);void k_irq_restore(u32);
#define spin_lock_init(s) ((s)->word=0)
#define spin_lock(s) k_spin_lock(s)
#define spin_unlock(s) k_spin_unlock(s)
#define spin_lock_irqsave(s,f) do{f=k_irq_save();spin_lock(s);}while(0)
#define spin_unlock_irqrestore(s,f) do{spin_unlock(s);k_irq_restore(f);}while(0)
#define spin_lock_irq(s) do{(s)->irq_flags=k_irq_save();spin_lock(s);}while(0)
#define spin_unlock_irq(s) do{u32 _f=(s)->irq_flags;spin_unlock(s);k_irq_restore(_f);}while(0)
void mutex_init(struct mutex *),mutex_lock(struct mutex *),mutex_unlock(struct mutex *);
int mutex_trylock(struct mutex *),mutex_lock_interruptible(struct mutex *);
void down_read(struct rw_semaphore *),up_read(struct rw_semaphore *),down_write(struct rw_semaphore *),up_write(struct rw_semaphore *);
struct workqueue_struct;struct work_struct {void (*function)(struct work_struct *);struct list_head entry;unsigned pending,running;struct workqueue_struct *queue;u64 sequence;};
struct timer_list {void (*function)(struct timer_list *);struct list_head entry;unsigned long expires;unsigned pending,running;};
struct delayed_work {struct work_struct work;struct timer_list timer;struct workqueue_struct *queue;};
struct completion {unsigned done;};
typedef struct {unsigned sequence;} wait_queue_head_t;
#define __WAIT_QUEUE_HEAD_INITIALIZER(n) {0}
#define DECLARE_WAIT_QUEUE_HEAD(n) wait_queue_head_t n={0}
#define init_waitqueue_head(q) ((q)->sequence=0)
#define wake_up(q) __atomic_add_fetch(&(q)->sequence,1,__ATOMIC_RELEASE)
#define wake_up_all wake_up
#define INIT_WORK(w,fn) k_init_work(w,fn)
#define INIT_DELAYED_WORK(w,fn) k_init_delayed_work(w,fn)
void k_init_work(struct work_struct *,void (*)(struct work_struct *));
void k_init_delayed_work(struct delayed_work *,void (*)(struct work_struct *));
void timer_setup(struct timer_list *,void (*)(struct timer_list *),unsigned);
int mod_timer(struct timer_list *,unsigned long),del_timer(struct timer_list *),del_timer_sync(struct timer_list *);
int schedule_work(struct work_struct *),queue_work(struct workqueue_struct *,struct work_struct *),cancel_work_sync(struct work_struct *);
int schedule_delayed_work(struct delayed_work *,unsigned long),mod_delayed_work(struct workqueue_struct *,struct delayed_work *,unsigned long),cancel_delayed_work_sync(struct delayed_work *);
void flush_work(struct work_struct *),flush_workqueue(struct workqueue_struct *),destroy_workqueue(struct workqueue_struct *);
struct workqueue_struct *alloc_workqueue(const char *,unsigned,int,...);
extern struct workqueue_struct *system_wq,*system_bh_wq,*system_bh_highpri_wq;
#define from_timer(v,t,m) container_of(t,__typeof__(*v),m)
#define to_delayed_work(w) container_of(w,struct delayed_work,work)
#define work_pending(w) ((w)->pending)
#define WQ_FREEZABLE 1
#define WQ_MEM_RECLAIM 2
#define WQ_BH 4
#define WQ_HIGHPRI 8
#define WQ_UNBOUND 16
void init_completion(struct completion *),reinit_completion(struct completion *),complete(struct completion *),complete_all(struct completion *),wait_for_completion(struct completion *);
unsigned long wait_for_completion_timeout(struct completion *,unsigned long);
int wait_for_completion_interruptible_timeout(struct completion *,unsigned long);
u64 k_clock_ms(void);void k_progress(void);
#define HZ 1000ul
#define jiffies ((unsigned long)k_clock_ms())
#define msecs_to_jiffies(n) ((unsigned long)(n))
#define jiffies_to_msecs(n) (n)
#define time_after(a,b) ((long)((b)-(a))<0)
#define time_after_eq(a,b) ((long)((a)-(b))>=0)
#define time_before(a,b) time_after(b,a)
#define time_before_eq(a,b) time_after_eq(b,a)
#define cpu_relax() k_progress()
#define wait_event(q,c) do{while(!(c))k_progress();}while(0)
#define wait_event_timeout(q,c,t) ({u64 _end=k_clock_ms()+(t);while(!(c)&&k_clock_ms()<_end)k_progress();(c)?1ul:0ul;})
#define wait_event_interruptible(q,c) ({wait_event(q,c);0;})
#define wait_event_interruptible_timeout wait_event_timeout
void msleep(unsigned),mdelay(unsigned),udelay(unsigned),usleep_range(unsigned,unsigned);
long schedule_timeout_uninterruptible(long);
struct task_struct {unsigned flags;char comm[16];};extern struct task_struct *current;
#define PF_NOFREEZE 1
#define TASK_UNINTERRUPTIBLE 1
int in_interrupt(void);
void *kmalloc(size_t,gfp_t),*kzalloc(size_t,gfp_t),*kmemdup(const void *,size_t,gfp_t),*krealloc(void *,size_t,gfp_t),*kcalloc(size_t,size_t,gfp_t),*kmalloc_array(size_t,size_t,gfp_t);
void kfree(const void *);
#define kzalloc_node(n,f,node) kzalloc(n,f)
#define kmalloc_node(n,f,node) kmalloc(n,f)
#define kcalloc_node(n,s,f,node) kcalloc(n,s,f)
static inline size_t k_struct_size(size_t b,size_t s,size_t n){size_t v;if(__builtin_mul_overflow(s,n,&v)||__builtin_add_overflow(b,v,&v))return SIZE_MAX;return v;}
#define struct_size(p,m,n) k_struct_size(sizeof(*(p)),sizeof((p)->m[0]),n)
#define array_size(a,b) k_struct_size(0,a,b)
#define size_mul(a,b) array_size(a,b)
char *kstrdup(const char *,gfp_t),*kasprintf(gfp_t,const char *,...);
size_t strlen(const char *),strnlen(const char *,size_t),strlcat(char *,const char *,size_t);
int strcmp(const char *,const char *),strncmp(const char *,const char *,size_t);
void *memcpy(void *,const void *,size_t),*memset(void *,int,size_t),*memchr(const void *,int,size_t);
int memcmp(const void *,const void *,size_t);
long strscpy(char *,const char *,size_t);
int snprintf(char *,size_t,const char *,...),scnprintf(char *,size_t,const char *,...),sprintf(char *,const char *,...),sscanf(const char *,const char *,...);
#define KERN_ERR ""
#define KERN_WARNING ""
#define KERN_INFO ""
#define KERN_DEBUG ""
void k_log(unsigned,const char *,...);
#define pr_err(...) k_log(1,__VA_ARGS__)
#define pr_warn(...) k_log(1,__VA_ARGS__)
#define pr_info(...) k_log(0,__VA_ARGS__)
#define pr_debug(...) k_log(0,__VA_ARGS__)
#define dev_err(d,...) k_log(1,__VA_ARGS__)
#define dev_warn(d,...) k_log(1,__VA_ARGS__)
#define dev_info(d,...) k_log(0,__VA_ARGS__)
#define dev_dbg(d,...) k_log(0,__VA_ARGS__)
#define printk(...) k_log(1,__VA_ARGS__)
#define WARN_ON(c) ({int _c=!!(c);if(_c)k_log(1,"Linux invariant %s:%u",__FILE__,__LINE__);_c;})
#define WARN_ON_ONCE WARN_ON
#define WARN_ONCE(c,...) WARN_ON(c)
#define dev_WARN_ONCE(d,c,...) WARN_ON(c)
#define dev_WARN(d,...) k_log(1,__VA_ARGS__)
void k_fatal(void) __attribute__((noreturn));
#define BUG() do{k_log(1,"BUG %s:%u",__FILE__,__LINE__);k_fatal();}while(0)
#define BUG_ON(c) do{if(c)BUG();}while(0)
struct bus_type;struct device_driver;struct attribute_group;struct device_type;struct dev_pm_ops;struct fwnode_handle;struct device_node;struct kobj_uevent_env;typedef unsigned kuid_t;typedef unsigned kgid_t;
struct attribute {const char *name;umode_t mode;};
struct kobject;
struct kernfs_node {struct kobject *owner;char *name;unsigned references,events,active;};
struct kobject {const char *name;struct kobject *parent;struct kernfs_node *sd;struct kernfs_node directory;};
struct device {
 struct kobject kobj;struct device *parent;const struct device_type *type;const struct bus_type *bus;struct device_driver *driver;
 struct mutex mutex;struct kref refs;void (*release)(struct device *);const struct attribute_group **groups;void *driver_data;
 const char *init_name;struct device_node *of_node;struct fwnode_handle *fwnode;dev_t devt;struct class *class;
 struct list_head children,sibling,bus_entry,managed;unsigned registered,authorized,wakeup_capable,wakeup_enabled,removable;
 struct {bool is_prepared;atomic_t usage_count;unsigned long last_busy;int runtime_status;} power;
 u64 *dma_mask,coherent_dma_mask;struct dma_parms *dma_parms;
};
struct device_type {const char *name;const struct attribute_group **groups;void (*release)(struct device *);int (*uevent)(const struct device *,struct kobj_uevent_env *);const struct dev_pm_ops *pm;char *(*devnode)(const struct device *,umode_t *,kuid_t *,kgid_t *);};
struct device_driver {const char *name;const struct bus_type *bus;struct module *owner;const char *mod_name;bool suppress_bind_attrs;int (*probe)(struct device *);int (*remove)(struct device *);void (*shutdown)(struct device *);const struct attribute_group **groups;const struct attribute_group **dev_groups;struct list_head registry;};
struct bus_type {const char *name;int (*match)(struct device *,const struct device_driver *);int (*uevent)(const struct device *,struct kobj_uevent_env *);int (*probe)(struct device *);void (*remove)(struct device *);void (*shutdown)(struct device *);const struct attribute_group **dev_groups;const struct attribute_group **drv_groups;const struct dev_pm_ops *pm;bool need_parent_lock;};
struct device_attribute {struct attribute attr;ssize_t (*show)(struct device *,struct device_attribute *,char *);ssize_t (*store)(struct device *,struct device_attribute *,const char *,size_t);};
struct driver_attribute {struct attribute attr;ssize_t (*show)(struct device_driver *,char *);ssize_t (*store)(struct device_driver *,const char *,size_t);};
struct attribute_group {const char *name;struct attribute **attrs;struct bin_attribute **bin_attrs;umode_t (*is_visible)(struct kobject *,struct attribute *,int);};
#define __ATTR(n,m,s,t) {.attr={#n,m},.show=s,.store=t}
#define DEVICE_ATTR_RO(n) struct device_attribute dev_attr_##n=__ATTR(n,0444,n##_show,NULL)
#define DEVICE_ATTR_RW(n) struct device_attribute dev_attr_##n=__ATTR(n,0644,n##_show,n##_store)
#define DEVICE_ATTR_WO(n) struct device_attribute dev_attr_##n=__ATTR(n,0200,NULL,n##_store)
#define DEVICE_ATTR(n,m,s,t) struct device_attribute dev_attr_##n=__ATTR(n,m,s,t)
#define DRIVER_ATTR_RW(n) struct driver_attribute driver_attr_##n=__ATTR(n,0644,n##_show,n##_store)
#define ATTRIBUTE_GROUPS(n) static const struct attribute_group n##_group={.attrs=n##_attrs};static const struct attribute_group *n##_groups[]={&n##_group,NULL}
#define to_kobj(d) (&(d)->kobj)
#define kobj_to_dev(k) container_of(k,struct device,kobj)
const char *dev_name(const struct device *);
int dev_set_name(struct device *,const char *,...);
static inline void dev_set_drvdata(struct device *d,void *p){d->driver_data=p;}
static inline void *dev_get_drvdata(const struct device *d){return d->driver_data;}
void device_initialize(struct device *),device_del(struct device *),put_device(struct device *),device_unregister(struct device *);
struct device *get_device(struct device *);
int device_add(struct device *),device_register(struct device *),device_attach(struct device *),device_reprobe(struct device *);
void device_release_driver(struct device *);
int device_bind_driver(struct device *);
void device_lock(struct device *),device_unlock(struct device *);
int device_trylock(struct device *);
int driver_register(struct device_driver *);void driver_unregister(struct device_driver *);
int driver_attach(struct device_driver *),bus_register(const struct bus_type *);
void bus_unregister(const struct bus_type *);
int bus_for_each_dev(const struct bus_type *,struct device *,void *,int (*)(struct device *,void *));
int bus_for_each_drv(const struct bus_type *,struct device_driver *,void *,int (*)(struct device_driver *,void *));
struct device *bus_find_device(const struct bus_type *,struct device *,const void *,int (*)(struct device *,const void *));
int device_for_each_child(struct device *,void *,int (*)(struct device *,void *));
int device_create_file(struct device *,const struct device_attribute *),driver_create_file(struct device_driver *,const struct driver_attribute *);
void device_remove_file(struct device *,const struct device_attribute *),driver_remove_file(struct device_driver *,const struct driver_attribute *);
void *devm_kzalloc(struct device *,size_t,gfp_t);
int devm_add_action_or_reset(struct device *,void (*)(void *),void *);
struct lock_class_key {unsigned value;};
#define lockdep_set_class(a,b) do{}while(0)
#define lockdep_assert_held(a) do{}while(0)
#define lockdep_assert_held_write(a) do{}while(0)
#define lockdep_set_novalidate_class(a) do{}while(0)
#define might_sleep() do{}while(0)
struct cdev {int unused;};struct file;struct inode;struct file_operations;
struct workqueue_attrs;struct dentry;
struct notifier_block {int (*notifier_call)(struct notifier_block *,unsigned long,void *);struct notifier_block *next;int priority;};
struct atomic_notifier_head {struct notifier_block *head;};
typedef struct {int event;} pm_message_t;
#define PMSG_AUTO_SUSPEND ((pm_message_t){0})
#define PMSG_AUTO_RESUME ((pm_message_t){0})
#define PMSG_SUSPEND ((pm_message_t){0})
#define PMSG_RESUME ((pm_message_t){0})
#define PMSG_ON ((pm_message_t){0})
#define PM_EVENT_AUTO 1
#define PM_EVENT_SUSPEND 2
#define PM_EVENT_RESUME 4
#define PM_EVENT_FREEZE 8
#define PM_EVENT_PRETHAW 16
#define PM_EVENT_HIBERNATE 32
#define PM_EVENT_RESTORE 64
#define PM_EVENT_IS_AUTO(m) ((m).event&PM_EVENT_AUTO)
#define pm_ptr(p) NULL
#define pm_sleep_ptr(p) NULL
struct dev_pm_ops {int unused;};
struct dev_pm_qos_request {int value;};
struct kobj_uevent_env {char *envp[32];char buf[2048];unsigned count,used;};
int add_uevent_var(struct kobj_uevent_env *,const char *,...);
struct ktermios {unsigned value;};
struct scatterlist {void *address;dma_addr_t dma_address;unsigned length,dma_length,offset;struct scatterlist *next;};
#define for_each_sg(first,p,n,i) for(i=0,p=first;i<(n);i++,p=p->next)
#define sg_dma_address(sg) ((sg)->dma_address)
#define sg_dma_len(sg) ((sg)->dma_length)
struct urb;struct pci_dev;struct platform_device;
typedef int irqreturn_t;
#define IRQ_NONE 0
#define IRQ_HANDLED 1
#define IRQF_SHARED 0x80
struct idr {void *entries;};
#define DEFINE_IDR(n) struct idr n={0}
struct ida {unsigned long used[16];};
#define DEFINE_IDA(n) struct ida n={0}
#define IDA_INIT(n) {0}
struct rwlock {spinlock_t lock;};typedef struct rwlock rwlock_t;
#define DEFINE_RWLOCK(n) rwlock_t n={0}
#define _IO(type,nr) (((type)<<8)|(nr))
#define _IOR(type,nr,size) _IO(type,nr)
#define _IOW(type,nr,size) _IO(type,nr)
#define _IOWR(type,nr,size) _IO(type,nr)
typedef u64 phys_addr_t;
enum phy_mode;
#define dev_notice(d,...) k_log(0,__VA_ARGS__)
#define ENOTSUPP 524
#define MAX_SCHEDULE_TIMEOUT LONG_MAX
#define subsys_initcall(fn) int tmc_usbcore_init(void){return fn();}
#define module_exit(fn) void tmc_usbcore_exit(void){fn();}
/* Sanitizers, DT, system PM, debugfs are disabled configurations, not device
 * operations implemented with fake success. */
static inline void kmsan_handle_urb(struct urb *u,bool in){(void)u;(void)in;}
static inline void of_node_put(struct device_node *n){BUG_ON(n!=NULL);}
static inline void set_dev_node(struct device *d,int n){(void)d;BUG_ON(n!=-1);}
static inline int dev_to_node(const struct device *d){(void)d;return -1;}
static inline void device_set_of_node_from_dev(struct device *d,const struct device *s){BUG_ON(s->of_node!=NULL);d->of_node=NULL;}
static inline void device_enable_async_suspend(struct device *d){(void)d;/* serialized executor */}
static inline int device_is_registered(const struct device *d){return d->registered;}
static inline int pm_runtime_set_active(struct device *d){d->power.runtime_status=0;return 0;}
static inline int pm_runtime_set_suspended(struct device *d){d->power.runtime_status=1;return 0;}
static inline void pm_runtime_enable(struct device *d){(void)d;}
static inline void pm_runtime_disable(struct device *d){(void)d;}
static inline void pm_suspend_ignore_children(struct device *d,bool b){(void)d;(void)b;}
static inline void pm_runtime_no_callbacks(struct device *d){(void)d;}
#define BUS_NOTIFY_ADD_DEVICE 1
#define BUS_NOTIFY_DEL_DEVICE 2
int bus_register_notifier(const struct bus_type *,struct notifier_block *),bus_unregister_notifier(const struct bus_type *,struct notifier_block *);
struct class {const char *name;const struct attribute_group **dev_groups;};
int class_register(const struct class *);void class_unregister(const struct class *);
struct dentry *debugfs_create_file(const char *,umode_t,struct dentry *,void *,const struct file_operations *);
void debugfs_lookup_and_remove(const char *,struct dentry *);
void idr_destroy(struct idr *);
static inline int PageHighMem(const void *p){(void)p;return 0;}
static inline void *sg_page(struct scatterlist *sg){return sg->address;}
static inline void *sg_virt(struct scatterlist *sg){return sg->address;}
static inline void yield(void){k_progress();}
#define UTF16_LITTLE_ENDIAN 0
int utf16s_to_utf8s(const void *,int,int,u8 *,int);
static inline int isprint(int c){return c>=32&&c<127;}
#define ACPI_COMPANION(d) NULL
#define ACPI_COMPANION_SET(d,c) do{(void)(d);BUG_ON((c)!=NULL);}while(0)
int vsnprintf(char *,size_t,const char *,va_list);
int atomic_notifier_chain_register(struct atomic_notifier_head *,struct notifier_block *),atomic_notifier_chain_unregister(struct atomic_notifier_head *,struct notifier_block *);
extern struct workqueue_struct *system_power_efficient_wq;
int queue_delayed_work(struct workqueue_struct *,struct delayed_work *,unsigned long);
void tmc_usbcore_dispatch(void);
static inline int of_device_is_available(const struct device_node *n){BUG_ON(n!=NULL);return 1;}
#define wchar_t tmc_wchar_t
typedef u16 tmc_wchar_t;
ssize_t tmc_usbcore_attribute_read(void *,const char *,char *);
ssize_t tmc_usbcore_attribute_write(void *,const char *,const char *,size_t);
struct kernel_param {void *arg;};
struct kernel_param_ops {int (*set)(const char *,const struct kernel_param *);int (*get)(char *,const struct kernel_param *);};
struct kparam_string {unsigned maxlen;char *string;};
int param_set_copystring(const char *,const struct kernel_param *),param_get_string(char *,const struct kernel_param *);
int kstrtou16(const char *,unsigned,u16 *);
char *strsep(char **,const char *);
#define module_param_cb(n,ops,arg,perm)
#define device_param_cb(n,ops,arg,perm)
#define __must_hold(x)
#define __acquire(x)
#define __release(x)
#define MKDEV(major,minor) (((major)<<20)|(minor))
#define USB_MAJOR 180
#define pr_warn_once(...) k_log(1,__VA_ARGS__)
#define WARN(c,...) WARN_ON(c)
#define DEVICE_REMOVABLE_UNKNOWN 0
#define DEVICE_REMOVABLE 1
#define DEVICE_FIXED 2
static inline void dev_set_removable(struct device *d,int v){d->removable=v;}
static inline void device_set_wakeup_capable(struct device *d,bool v){d->wakeup_capable=v;}
static inline int device_init_wakeup(struct device *d,bool v){d->wakeup_capable=v;d->wakeup_enabled=v;return 0;}
static inline void pm_wakeup_event(struct device *d,unsigned ms){(void)d;(void)ms;}
static inline int pm_runtime_barrier(struct device *d){(void)d;return 0;}
static inline int pm_runtime_get_sync(struct device *d){(void)d;return 1;}
static inline int pm_runtime_put(struct device *d){(void)d;return -ENOSYS;}
static inline void pm_runtime_get_noresume(struct device *d){(void)d;}
static inline void pm_runtime_use_autosuspend(struct device *d){(void)d;}
static inline int pm_runtime_put_sync_autosuspend(struct device *d){(void)d;return -ENOSYS;}
struct kernfs_node;
void sysfs_notify_dirent(struct kernfs_node *);
int sysfs_create_link(struct kobject *,struct kobject *,const char *);
void sysfs_remove_link(struct kobject *,const char *);
void add_device_randomness(const void *,unsigned);
int printk_ratelimit(void);
static inline unsigned long find_next_zero_bit(const unsigned long *b,unsigned long n,unsigned long start){while(start<n&&test_bit(start,b))start++;return start;}
#define EISDIR 21
#define KOBJ_CHANGE 2
struct kernfs_node;
void sysfs_notify(struct kobject *,const char *,const char *);
char *kobject_get_path(const struct kobject *,gfp_t);
int kobject_uevent_env(struct kobject *,int,char *[]);
static inline int pm_runtime_active(struct device *d){return d->power.runtime_status==0;}
static inline int pm_runtime_put_sync(struct device *d){(void)d;return -ENOSYS;}
static inline void kcov_remote_start_usb(unsigned n){(void)n;}
static inline void kcov_remote_stop(void){}
static inline unsigned memalloc_noio_save(void){return 0;}
static inline void memalloc_noio_restore(unsigned flags){BUG_ON(flags);}
#pragma once
#define LINUX_VERSION_MAJOR 6
#define LINUX_VERSION_PATCHLEVEL 12
#define bin2bcd(n) ((((n)/10)<<4)|((n)%10))
#define EFAULT 14
#define KOBJ_OFFLINE 3
#define MEMREMAP_WC 1
#define DMA_ATTR_WRITE_COMBINE 1
struct new_utsname {char sysname[65],release[65];};
const struct new_utsname *init_utsname(void);
static inline int device_may_wakeup(struct device *d){return d->wakeup_capable&&d->wakeup_enabled;}
static inline int device_can_wakeup(struct device *d){return d->wakeup_capable;}
static inline int device_set_wakeup_enable(struct device *d,bool b){if(b&&!d->wakeup_capable)return -EINVAL;d->wakeup_enabled=b;return 0;}
static inline void pm_runtime_put_noidle(struct device *d){(void)d;}
enum dma_data_direction {DMA_BIDIRECTIONAL=0,DMA_TO_DEVICE=1,DMA_FROM_DEVICE=2,DMA_NONE=3};
#define put_unaligned(v,p) do{__typeof__(*(p)) _v=(v);memcpy(p,&_v,sizeof(_v));}while(0)
#define get_unaligned(p) ({__typeof__(*(p)) _v;memcpy(&_v,p,sizeof(_v));_v;})
int object_is_on_stack(const void *);
dma_addr_t dma_map_single(struct device *,void *,size_t,enum dma_data_direction);
void dma_unmap_single(struct device *,dma_addr_t,size_t,enum dma_data_direction);
int dma_map_sg(struct device *,struct scatterlist *,int,enum dma_data_direction);
void dma_unmap_sg(struct device *,struct scatterlist *,int,enum dma_data_direction);
dma_addr_t dma_map_page(struct device *,void *,size_t,size_t,enum dma_data_direction);
void dma_unmap_page(struct device *,dma_addr_t,size_t,enum dma_data_direction);
int dma_mapping_error(struct device *,dma_addr_t);
static inline unsigned long kcov_remote_start_usb_softirq(u64 b){(void)b;return 0;}
static inline void kcov_remote_stop_softirq(unsigned long f){(void)f;}
static inline void list_replace_init(struct list_head *o,struct list_head *n){n->next=o->next;n->prev=o->prev;n->next->prev=n;n->prev->next=n;INIT_LIST_HEAD(o);}
int idr_alloc(struct idr *,void *,int,int,gfp_t);
void *idr_remove(struct idr *,int);
int request_irq(unsigned,irqreturn_t (*)(int,void *),unsigned long,const char *,void *);
void free_irq(unsigned,void *);
enum phy_mode {PHY_NONE,PHY_MODE_USB_HOST,PHY_MODE_USB_HOST_SS};
struct platform_device {struct device dev;};
static inline void *platform_get_drvdata(struct platform_device *d){return dev_get_drvdata(&d->dev);}
struct gen_pool;
struct gen_pool *devm_gen_pool_create(struct device *,int,int,const char *);
void *devm_memremap(struct device *,phys_addr_t,size_t,unsigned);
void *dmam_alloc_attrs(struct device *,size_t,dma_addr_t *,gfp_t,unsigned long);
int gen_pool_add_virt(struct gen_pool *,unsigned long,phys_addr_t,size_t,int);
#define ARCH_DMA_MINALIGN 16
#define BUILD_BUG() k_fatal()
#define DMA_MAPPING_ERROR (~(dma_addr_t)0)
struct dma_pool;
struct dma_pool *dma_pool_create(const char *,struct device *,size_t,size_t,size_t);
void dma_pool_destroy(struct dma_pool *);
void *dma_pool_alloc(struct dma_pool *,gfp_t,dma_addr_t *);
void dma_pool_free(struct dma_pool *,void *,dma_addr_t);
void *dma_alloc_coherent(struct device *,size_t,dma_addr_t *,gfp_t);
void dma_free_coherent(struct device *,size_t,void *,dma_addr_t);
void *gen_pool_dma_alloc(struct gen_pool *,size_t,dma_addr_t *);
void *gen_pool_dma_alloc_align(struct gen_pool *,size_t,dma_addr_t *,unsigned);
void gen_pool_free(struct gen_pool *,unsigned long,size_t);
unsigned long __get_free_pages(gfp_t,unsigned);void free_pages(unsigned long,unsigned);
static inline unsigned get_order(size_t n){unsigned o=0;size_t p=PAGE_SIZE;while(p<n){o++;p<<=1;}return o;}
struct component_ops {int (*bind)(struct device *,struct device *,void *);void (*unbind)(struct device *,struct device *,void *);};
int component_add(struct device *,const struct component_ops *);void component_del(struct device *,const struct component_ops *);
int sysfs_emit(char *,const char *,...);
int kstrtobool(const char *,bool *),kstrtou32(const char *,unsigned,u32 *);
struct kernfs_node *sysfs_break_active_protection(struct kobject *,const struct attribute *);
void sysfs_unbreak_active_protection(struct kernfs_node *);
struct kernfs_node *sysfs_get_dirent(struct kernfs_node *,const char *);void sysfs_put(struct kernfs_node *);
int dev_pm_qos_add_request(struct device *,struct dev_pm_qos_request *,int,int);
int dev_pm_qos_remove_request(struct dev_pm_qos_request *);
int dev_pm_qos_expose_flags(struct device *,int);
#define DEV_PM_QOS_FLAGS 1
#define PM_QOS_FLAG_NO_POWER_OFF 1
int device_property_match_property_string(struct device *,const char *,const char * const *,size_t);
int device_property_read_string(struct device *,const char *,const char **);
int match_string(const char * const *,size_t,const char *);
struct dentry *debugfs_create_dir(const char *,struct dentry *);void debugfs_remove_recursive(struct dentry *);
#define module_param_array_named(n,v,t,c,p)
#define module_init(fn) int tmc_hid_module_init(void){return fn();}
#undef module_exit
#define module_exit(fn) void tmc_hid_module_exit(void){fn();}
#define pr_warn_ratelimited(...) k_log(1,__VA_ARGS__)
#define PMSG_IS_AUTO(m) ((m).event&0x400)
static inline struct fwnode_handle *dev_fwnode(struct device *d){return d->fwnode;}
static inline void device_set_node(struct device *d,struct fwnode_handle *f){d->fwnode=f;}

#define module_driver(d,reg,unreg) int tmc_generic_init(void){return reg(&(d));} void tmc_generic_exit(void){unreg(&(d));}
#define DRIVER_ATTR_WO(n) struct driver_attribute driver_attr_##n=__ATTR(n,0200,NULL,n##_store)
#define __ATTRIBUTE_GROUPS(n) static const struct attribute_group *n##_groups[]={&n##_group,NULL}
#define dev_warn_once(d,...) dev_warn(d,__VA_ARGS__)
static inline int mutex_lock_killable(struct mutex *m){return mutex_lock_interruptible(m);}
static inline void *kvzalloc(size_t n,gfp_t f){return kzalloc(n,f);}
static inline void kvfree(const void *p){kfree(p);}
static inline void *vzalloc(size_t n){return kzalloc(n,GFP_KERNEL);}
static inline void vfree(const void *p){kfree(p);}
static inline size_t array3_size(size_t a,size_t b,size_t c){size_t x,y;if(__builtin_mul_overflow(a,b,&x)||__builtin_mul_overflow(x,c,&y))return SIZE_MAX;return y;}
static inline u16 get_unaligned_le16(const void *p){u16 v;memcpy(&v,p,2);return le16_to_cpu(v);}
static inline u32 get_unaligned_le32(const void *p){u32 v;memcpy(&v,p,4);return le32_to_cpu(v);}
typedef i64 loff_t;
struct bin_attribute {struct attribute attr;size_t size;ssize_t (*read)(struct file *,struct kobject *,struct bin_attribute *,char *,loff_t,size_t);};
char *strrchr(const char *,int);
void *devres_open_group(struct device *,void *,gfp_t);int devres_release_group(struct device *,void *);
int bus_rescan_devices(const struct bus_type *);
static inline void list_replace(struct list_head *o,struct list_head *n){n->next=o->next;n->prev=o->prev;n->next->prev=n;n->prev->next=n;}
