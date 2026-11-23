/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* Native representations for services used by Linux v6.12 usb/core/urb.c.
 * All list manipulation is protected by the caller's compatibility/anchor
 * lock. Atomic references and wait predicates use acquire/release ordering.
 */
typedef struct {unsigned word;} spinlock_t;
typedef struct {int counter;} atomic_t;
static inline int atomic_read(const atomic_t *a){return __atomic_load_n(&a->counter,__ATOMIC_ACQUIRE);}
static inline void atomic_set(atomic_t *a,int n){__atomic_store_n(&a->counter,n,__ATOMIC_RELEASE);}
static inline void atomic_inc(atomic_t *a){__atomic_add_fetch(&a->counter,1,__ATOMIC_ACQ_REL);}
static inline void atomic_dec(atomic_t *a){__atomic_sub_fetch(&a->counter,1,__ATOMIC_ACQ_REL);}
struct kref {atomic_t refcount;};
static inline void kref_init(struct kref *r){atomic_set(&r->refcount,1);}
/* Saturating references: overflow, underflow and get-from-zero must never
 * wrap into a premature free. Saturated objects deliberately leak safely,
 * matching the Linux refcount safety policy rather than wrapping atomics.
 */
static inline void kref_get(struct kref *r)
{
 int value=atomic_read(&r->refcount);
 for(;;){
  int next=value>0&&value<0x7fffffff?value+1:0x7fffffff;
  if(__atomic_compare_exchange_n(&r->refcount.counter,&value,next,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return;
 }
}
static inline int kref_put(struct kref *r,void (*release)(struct kref *))
{
 int value=atomic_read(&r->refcount);
 for(;;){
  if(value==0x7fffffff)return 0;
  int next=value>0?value-1:0x7fffffff;
  if(__atomic_compare_exchange_n(&r->refcount.counter,&value,next,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
   if(!next){release(r);return 1;}return 0;
  }
 }
}
struct list_head {struct list_head *next,*prev;};
static inline void INIT_LIST_HEAD(struct list_head *p){p->next=p->prev=p;}
static inline int list_empty(const struct list_head *p){return p->next==p;}
static inline void list_add_tail(struct list_head *p,struct list_head *head)
{
 p->prev=head->prev;p->next=head;head->prev->next=p;head->prev=p;
}
static inline void list_del(struct list_head *p){p->prev->next=p->next;p->next->prev=p->prev;p->next=p->prev=0;}
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define list_entry(p,t,m) container_of(p,t,m)
#define list_for_each_entry(p,h,m) for(p=list_entry((h)->next,__typeof__(*p),m);&(p)->m!=(h);p=list_entry((p)->m.next,__typeof__(*p),m))
typedef struct {unsigned sequence;} wait_queue_head_t;
struct usb_anchor {
 struct list_head urb_list;spinlock_t lock;wait_queue_head_t wait;
 atomic_t suspend_wakeups;unsigned poisoned;
};
struct scatterlist {unsigned length;struct scatterlist *next;};
struct usb_iso_packet_descriptor {unsigned offset,length,actual_length;int status;};
struct urb {
 struct usb_device *dev;void *context,*transfer_buffer;int status,actual_length,transfer_buffer_length;
 dma_addr_t transfer_dma;unsigned transfer_flags,pipe;int interval;void (*complete)(struct urb *);
 /* Native adapter state, distinct from upstream core state below. */
 int submitted,killed,type,unlinked;u8 *setup_packet;u64 serial;
 struct kref kref;struct list_head urb_list,anchor_list;
 struct usb_anchor *anchor;struct usb_host_endpoint *ep;void *hcpriv;
 atomic_t reject,use_count;struct scatterlist *sg;int num_sgs,number_of_packets;
 struct usb_iso_packet_descriptor iso_frame_desc[];
};
