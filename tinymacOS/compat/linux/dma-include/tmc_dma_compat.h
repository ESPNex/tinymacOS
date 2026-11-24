/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
/* Private whole-dmapool ABI. No type aliases with USB/HID private objects. */
#include "base.h"
#include "linux_dma_pool.h"
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
typedef u64 dma_addr_t;
typedef unsigned gfp_t;
typedef long ssize_t;
#define GFP_KERNEL 0u
#define GFP_ATOMIC 1u
#define __GFP_ZERO 2u
#define PAGE_SIZE 4096u
#define EINVAL 22
#define ENOMEM 12
#define EBUSY 16
#define EEXIST 17
#define ENOENT 2
#define EIO 5
#define NULL ((void *)0)
#define unlikely(x) __builtin_expect(!!(x),0)
#define min(a,b) ((a)<(b)?(a):(b))
#define max_t(t,a,b) ((t)(a)>(t)(b)?(t)(a):(t)(b))
#define ALIGN(n,a) (((n)+(a)-1)&~((a)-1))
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
struct list_head {struct list_head *next,*prev;};
static inline void INIT_LIST_HEAD(struct list_head *h){h->next=h->prev=h;}
static inline int list_empty(const struct list_head *h){return h->next==h;}
static inline void list_add(struct list_head *n,struct list_head *h){n->next=h->next;n->prev=h;h->next->prev=n;h->next=n;}
static inline void list_del(struct list_head *n){n->prev->next=n->next;n->next->prev=n->prev;n->next=n->prev=NULL;}
#define list_entry(p,t,m) container_of(p,t,m)
#define list_for_each_entry(p,h,m) for(p=list_entry((h)->next,__typeof__(*p),m);&(p)->m!=(h);p=list_entry((p)->m.next,__typeof__(*p),m))
#define list_for_each_entry_safe(p,n,h,m) for(p=list_entry((h)->next,__typeof__(*p),m),n=list_entry((p)->m.next,__typeof__(*p),m);&(p)->m!=(h);p=n,n=list_entry((n)->m.next,__typeof__(*n),m))
typedef struct {unsigned word;u32 irq_flags;} spinlock_t;
struct mutex {spinlock_t lock;u32 flags;};
#define DEFINE_MUTEX(name) struct mutex name={0}
static inline u32 dma_lock(spinlock_t *s){u32 f=tmc_lx_os_irq_save();while(__atomic_exchange_n(&s->word,1,__ATOMIC_ACQUIRE))tmc_lx_os_spin_wait();return f;}
static inline void dma_unlock(spinlock_t *s,u32 f){__atomic_store_n(&s->word,0,__ATOMIC_RELEASE);tmc_lx_os_irq_restore(f);}
#define spin_lock_init(s) ((s)->word=0)
#define spin_lock_irqsave(s,f) do{(f)=dma_lock(s);}while(0)
#define spin_unlock_irqrestore(s,f) dma_unlock((s),(u32)(f))
static inline void mutex_lock(struct mutex *m){u32 f=dma_lock(&m->lock);m->flags=f;}
static inline void mutex_unlock(struct mutex *m){u32 f=m->flags;dma_unlock(&m->lock,f);}
struct device_attribute;
struct device {struct list_head dma_pools;struct device_attribute *attribute;struct DmaResource *resources;spinlock_t resource_lock;const char *name;u64 mask;unsigned wakeup_capable,wakeup_enabled;};
struct device_attribute {const char *name;ssize_t (*show)(struct device *,struct device_attribute *,char *);};
#define DEVICE_ATTR_RO(n) struct device_attribute dev_attr_##n={#n,n##_show}
int device_create_file(struct device *,struct device_attribute *);
void device_remove_file(struct device *,struct device_attribute *);
int dma_sysfs_emit(char *,const char *,...);
int dma_sysfs_emit_at(char *,size_t,const char *,...);
#define sysfs_emit dma_sysfs_emit
#define sysfs_emit_at dma_sysfs_emit_at
void *dma_kmalloc(size_t,gfp_t);
void *dma_kzalloc(size_t,gfp_t);
void dma_kfree(void *);
#define kmalloc dma_kmalloc
#define kzalloc dma_kzalloc
#define kfree dma_kfree
long dma_strscpy(char *,const char *,size_t);
#define strscpy dma_strscpy
void *dma_alloc_coherent(struct device *,size_t,dma_addr_t *,gfp_t);
void dma_free_coherent(struct device *,size_t,void *,dma_addr_t);
/* The entire allocator is non-sleeping on this polled native runtime.
 * No flag claiming reclaim, scheduling or sleepability is accepted. */
void dma_might_alloc(gfp_t);
#define might_alloc dma_might_alloc
static inline bool want_init_on_alloc(gfp_t f){return !!(f&__GFP_ZERO);}
static inline bool want_init_on_free(void){return true;}
void dma_error(struct device *,const char *,...);
#define dev_err dma_error
#define WARN_ON(x) dma_warn(!!(x))
int dma_warn(int);
#define EXPORT_SYMBOL(n)
void *devres_alloc(void (*)(struct device *,void *),size_t,gfp_t);
void devres_add(struct device *,void *);
void devres_free(void *);
int devres_release(struct device *,void (*)(struct device *,void *),int (*)(struct device *,void *,void *),void *);
void dma_release_resources(struct device *);
/* Enable upstream invalid-free/poison diagnostics in host and kernel. */
#define CONFIG_SLUB_DEBUG_ON 1
#define POOL_POISON_FREED 0xa7
#define POOL_POISON_ALLOCATED 0xa9
#define KERN_ERR ""
#define DUMP_PREFIX_OFFSET 0
void print_hex_dump(const char *,const char *,int,int,int,const void *,size_t,bool);
struct dma_pool;
struct dma_pool *dma_pool_create(const char *,struct device *,size_t,size_t,size_t);
void dma_pool_destroy(struct dma_pool *);
void *dma_pool_alloc(struct dma_pool *,gfp_t,dma_addr_t *);
void dma_pool_free(struct dma_pool *,void *,dma_addr_t);
struct dma_pool *dmam_pool_create(const char *,struct device *,size_t,size_t,size_t);
void dmam_pool_destroy(struct dma_pool *);
