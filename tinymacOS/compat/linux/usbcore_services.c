/* SPDX-License-Identifier: GPL-2.0-only */
#include <tmc_kernel.h>
/* All platform effects cross these hooks.  No USB transaction is synthesized. */
extern void *tmc_usbcore_os_alloc(size_t);
extern void tmc_usbcore_os_free(void *);
extern u64 tmc_usbcore_os_clock_ms(void);
extern void tmc_usbcore_os_delay_us(unsigned);
extern void tmc_usbcore_os_poll(void);
extern u32 tmc_usbcore_os_irq_save(void);
extern void tmc_usbcore_os_irq_restore(u32);
extern void tmc_usbcore_os_log(unsigned,const char *,va_list);
extern void tmc_usbcore_os_fatal(void) __attribute__((noreturn));
void k_fatal(void){tmc_usbcore_os_fatal();}
void k_log(unsigned l,const char *f,...){va_list a;va_start(a,f);tmc_usbcore_os_log(l,f,a);va_end(a);}
u32 k_irq_save(void){return tmc_usbcore_os_irq_save();}
void k_irq_restore(u32 f){tmc_usbcore_os_irq_restore(f);}
void k_spin_lock(spinlock_t *s){if(__atomic_exchange_n(&s->word,1,__ATOMIC_ACQUIRE))BUG();}
void k_spin_unlock(spinlock_t *s){BUG_ON(!__atomic_exchange_n(&s->word,0,__ATOMIC_RELEASE));}
static unsigned held_mutexes;
void mutex_init(struct mutex *m){memset(m,0,sizeof(*m));}
int mutex_trylock(struct mutex *m){u32 f=k_irq_save();if(__atomic_exchange_n(&m->lock.word,1,__ATOMIC_ACQUIRE)){k_irq_restore(f);return 0;}held_mutexes++;k_irq_restore(f);return 1;}
void mutex_lock(struct mutex *m){BUG_ON(!mutex_trylock(m));}
int mutex_lock_interruptible(struct mutex *m){mutex_lock(m);return 0;}
void mutex_unlock(struct mutex *m){u32 f=k_irq_save();BUG_ON(!held_mutexes);held_mutexes--;k_spin_unlock(&m->lock);k_irq_restore(f);}
void down_read(struct rw_semaphore *s){BUG_ON(s->state==~0u);s->state++;}
void up_read(struct rw_semaphore *s){BUG_ON(!s->state||s->state==~0u);s->state--;}
void down_write(struct rw_semaphore *s){BUG_ON(s->state);s->state=~0u;}
void up_write(struct rw_semaphore *s){BUG_ON(s->state!=~0u);s->state=0;}
union allocation_header {struct {u64 magic;size_t size;} info;max_align_t alignment;};
#define ALLOCATION_MAGIC 0x544d43555342434fULL
void *kmalloc(size_t n,gfp_t f){if(!n||n>16*1024*1024||f&~(GFP_ATOMIC|__GFP_ZERO))return NULL;union allocation_header *h=tmc_usbcore_os_alloc(sizeof(*h)+n);if(!h)return NULL;h->info.magic=ALLOCATION_MAGIC;h->info.size=n;void *p=h+1;if(f&__GFP_ZERO)memset(p,0,n);return p;}
void *kzalloc(size_t n,gfp_t f){return kmalloc(n,f|__GFP_ZERO);}
void kfree(const void *p){if(p){union allocation_header *h=(union allocation_header *)p-1;BUG_ON(h->info.magic!=ALLOCATION_MAGIC);h->info.magic=0;tmc_usbcore_os_free(h);}}
void *krealloc(void *p,size_t n,gfp_t f){if(!n){kfree(p);return NULL;}if(!p)return kmalloc(n,f);union allocation_header *h=(union allocation_header *)p-1;BUG_ON(h->info.magic!=ALLOCATION_MAGIC);if(n<=h->info.size)return p;void *q=kmalloc(n,f);if(!q)return NULL;memcpy(q,p,h->info.size);kfree(p);return q;}
void *kcalloc(size_t n,size_t s,gfp_t f){size_t b;if(__builtin_mul_overflow(n,s,&b))return NULL;return kzalloc(b,f);}
void *kmalloc_array(size_t n,size_t s,gfp_t f){size_t b;if(__builtin_mul_overflow(n,s,&b))return NULL;return kmalloc(b,f);}
void *kmemdup(const void *p,size_t n,gfp_t f){if(!p)return NULL;void *q=kmalloc(n,f);if(q)memcpy(q,p,n);return q;}
char *kstrdup(const char *s,gfp_t f){return s?kmemdup(s,strlen(s)+1,f):NULL;}
char *kasprintf(gfp_t f,const char *fmt,...){va_list a,b;va_start(a,fmt);va_copy(b,a);int n=vsnprintf(NULL,0,fmt,a);va_end(a);char *p=n>=0?kmalloc((size_t)n+1,f):NULL;if(p)vsnprintf(p,n+1,fmt,b);va_end(b);return p;}
long strscpy(char *d,const char *s,size_t n){if(!n)return -E2BIG;size_t len=strnlen(s,n-1);memcpy(d,s,len);d[len]=0;return s[len]?-E2BIG:(long)len;}
int scnprintf(char *p,size_t n,const char *f,...){va_list a;va_start(a,f);int r=vsnprintf(p,n,f,a);va_end(a);return r<0?r:(int)(n?min_t(size_t,r,n-1):0);}
u64 k_clock_ms(void){return tmc_usbcore_os_clock_ms();}
static LIST_HEAD(ready_work);static LIST_HEAD(timers);
struct workqueue_struct {const char *name;unsigned allocated,draining,flags;u64 issued;};
static struct workqueue_struct builtin={.name="usbcore"};
static struct workqueue_struct builtin_bh={.name="usbcore-bh",.flags=WQ_BH};
static struct workqueue_struct builtin_highpri={.name="usbcore-bh-high",.flags=WQ_BH|WQ_HIGHPRI};
struct workqueue_struct *system_wq=&builtin,*system_bh_wq=&builtin_bh,*system_bh_highpri_wq=&builtin_highpri,*system_power_efficient_wq=&builtin;
struct executing {const void *object;struct executing *previous;struct workqueue_struct *queue;u64 sequence;};static struct executing *active;
static int executing(const void *p){for(struct executing *e=active;e;e=e->previous)if(e->object==p)return 1;return 0;}
void k_init_work(struct work_struct *w,void (*fn)(struct work_struct *)){memset(w,0,sizeof(*w));INIT_LIST_HEAD(&w->entry);w->function=fn;}
void timer_setup(struct timer_list *t,void (*fn)(struct timer_list *),unsigned flags){BUG_ON(flags);memset(t,0,sizeof(*t));INIT_LIST_HEAD(&t->entry);t->function=fn;}
static void delayed_expired(struct timer_list *t){struct delayed_work *d=container_of(t,struct delayed_work,timer);queue_work(d->queue,&d->work);}
void k_init_delayed_work(struct delayed_work *d,void (*fn)(struct work_struct *)){k_init_work(&d->work,fn);timer_setup(&d->timer,delayed_expired,0);}
int queue_work(struct workqueue_struct *q,struct work_struct *w){if(!q||q->draining||!w||!w->function||w->pending)return 0;w->pending=1;w->queue=q;w->sequence=++q->issued;list_add_tail(&w->entry,&ready_work);return 1;}
int schedule_work(struct work_struct *w){return queue_work(system_wq,w);}
int cancel_work_sync(struct work_struct *w){BUG_ON(executing(w));int old=w->pending;if(old){list_del_init(&w->entry);w->pending=0;}return old;}
int mod_timer(struct timer_list *t,unsigned long expires){BUG_ON(!t||!t->function);int old=t->pending;if(old)list_del_init(&t->entry);t->expires=expires;t->pending=1;list_add_tail(&t->entry,&timers);return old;}
int del_timer(struct timer_list *t){int old=t->pending;if(old){list_del_init(&t->entry);t->pending=0;}return old;}
int del_timer_sync(struct timer_list *t){BUG_ON(executing(t));return del_timer(t);}
int queue_delayed_work(struct workqueue_struct *q,struct delayed_work *d,unsigned long delay){if(!q||q->draining||d->work.pending||d->timer.pending)return 0;d->queue=q;if(!delay)return queue_work(q,&d->work);mod_timer(&d->timer,jiffies+delay);return 1;}
int schedule_delayed_work(struct delayed_work *d,unsigned long delay){return queue_delayed_work(system_wq,d,delay);}
int cancel_delayed_work_sync(struct delayed_work *d){int a=del_timer_sync(&d->timer);return cancel_work_sync(&d->work)||a;}
int mod_delayed_work(struct workqueue_struct *q,struct delayed_work *d,unsigned long delay){int old=del_timer(&d->timer);if(d->work.pending){list_del_init(&d->work.entry);d->work.pending=0;old=1;}queue_delayed_work(q,d,delay);return old;}
static int can_run(struct work_struct *w){
 if(w->queue->flags&WQ_BH)return 1;
 if(held_mutexes)return 0;
 /* Sleeping workers never recursively execute other sleeping workers. Only
  * timers and independent BH givebacks progress while one worker waits. */
 return active==NULL;
}
static int run_one(void){
 struct timer_list *t;list_for_each_entry(t,&timers,entry){if(time_before(jiffies,t->expires)||executing(t))continue;del_timer(t);struct executing frame={t,active,NULL,0};active=&frame;t->function(t);active=frame.previous;return 1;}
 struct work_struct *w;list_for_each_entry(w,&ready_work,entry){if(executing(w)||!can_run(w))continue;list_del_init(&w->entry);w->pending=0;struct executing frame={w,active,w->queue,w->sequence};active=&frame;w->function(w);active=frame.previous;return 1;}return 0;
}
void tmc_usbcore_dispatch(void){for(unsigned n=0;n<64&&run_one();n++);}
void k_progress(void){tmc_usbcore_os_poll();tmc_usbcore_dispatch();tmc_usbcore_os_delay_us(50);}
static int unfinished(struct workqueue_struct *q,u64 seq,const void *object){
 struct work_struct *w;list_for_each_entry(w,&ready_work,entry)if(w->queue==q&&w->sequence<=seq&&(!object||w==object))return 1;
 for(struct executing *e=active;e;e=e->previous)if(e->queue==q&&e->sequence<=seq&&(!object||e->object==object))return 1;
 return 0;
}
void flush_work(struct work_struct *w){BUG_ON(executing(w));struct workqueue_struct *q=w->queue;u64 seq=w->sequence;while(unfinished(q,seq,w))k_progress();}
void flush_workqueue(struct workqueue_struct *q){BUG_ON(!q);u64 seq=q->issued;for(struct executing *e=active;e;e=e->previous)BUG_ON(e->queue==q);while(unfinished(q,seq,NULL))k_progress();}
struct workqueue_struct *alloc_workqueue(const char *n,unsigned flags,int max_active,...){if(flags&~31u||max_active<0)return NULL;struct workqueue_struct *q=kzalloc(sizeof(*q),GFP_KERNEL);if(q){q->name=n;q->allocated=1;q->flags=flags;}return q;}
void destroy_workqueue(struct workqueue_struct *q){if(!q)return;BUG_ON(!q->allocated);struct timer_list *t;list_for_each_entry(t,&timers,entry)if(t->function==delayed_expired)BUG_ON(container_of(t,struct delayed_work,timer)->queue==q);q->draining=1;flush_workqueue(q);kfree(q);}
void init_completion(struct completion *c){c->done=0;}
void reinit_completion(struct completion *c){c->done=0;}
void complete(struct completion *c){if(c->done!=~0u)c->done++;}
void complete_all(struct completion *c){c->done=~0u;}
unsigned long wait_for_completion_timeout(struct completion *c,unsigned long t){u64 end=k_clock_ms()+t;while(!c->done&&k_clock_ms()<end)k_progress();if(!c->done)return 0;if(c->done!=~0u)c->done--;return k_clock_ms()<end?(unsigned long)(end-k_clock_ms()):1;}
int wait_for_completion_interruptible_timeout(struct completion *c,unsigned long t){return wait_for_completion_timeout(c,t);}
void wait_for_completion(struct completion *c){while(!c->done)k_progress();if(c->done!=~0u)c->done--;}
void udelay(unsigned us){tmc_usbcore_os_delay_us(us);}
void usleep_range(unsigned l,unsigned h){BUG_ON(h<l);u64 end=k_clock_ms()+DIV_ROUND_UP(l,1000);while(k_clock_ms()<end)k_progress();}
void msleep(unsigned ms){u64 end=k_clock_ms()+ms;while(k_clock_ms()<end)k_progress();}
void mdelay(unsigned ms){while(ms--)udelay(1000);}
long schedule_timeout_uninterruptible(long t){if(t>0)msleep(t);return 0;}
int in_interrupt(void){return 0;}
static struct task_struct boot_task={.comm="usb-poll"};struct task_struct *current=&boot_task;
/* A real in-kernel device/driver registry, not a facade onto native USB. */
struct bus_state {struct list_head entry,devices,drivers;const struct bus_type *bus;struct notifier_block *notify;};
static LIST_HEAD(buses);
static struct bus_state *find_bus(const struct bus_type *b){struct bus_state *s;list_for_each_entry(s,&buses,entry)if(s->bus==b)return s;return NULL;}
static void remove_attr(void *,const void *);
static int add_groups(struct device *,const struct attribute_group **);
static void remove_groups(struct device *,const struct attribute_group **);
struct managed {struct list_head entry;void (*release)(void *);void *data;};
static void release_managed(struct device *d){while(!list_empty(&d->managed)){struct managed *m=list_first_entry(&d->managed,struct managed,entry);list_del(&m->entry);void (*f)(void *)=m->release;void *p=m->data;kfree(m);if(f)f(p);}}
int devm_add_action_or_reset(struct device *d,void (*f)(void *),void *p){if(!d||!f)return -EINVAL;struct managed *m=kmalloc(sizeof(*m),GFP_KERNEL);if(!m){f(p);return -ENOMEM;}m->release=f;m->data=p;list_add(&m->entry,&d->managed);return 0;}
void *devm_kzalloc(struct device *d,size_t n,gfp_t f){void *p=kzalloc(n,f);if(p&&devm_add_action_or_reset(d,(void (*)(void *))kfree,p))return NULL;return p;}
void device_initialize(struct device *d){kref_init(&d->refs);mutex_init(&d->mutex);INIT_LIST_HEAD(&d->children);INIT_LIST_HEAD(&d->sibling);INIT_LIST_HEAD(&d->bus_entry);INIT_LIST_HEAD(&d->managed);d->registered=0;d->kobj.directory=(struct kernfs_node){.owner=&d->kobj};d->kobj.sd=&d->kobj.directory;}
static void release_device(struct kref *r){struct device *d=container_of(r,struct device,refs);BUG_ON(d->registered||!list_empty(&d->children));release_managed(d);remove_attr(d,NULL);char *name=(char *)d->kobj.name;void (*fn)(struct device *)=d->release?d->release:(d->type?d->type->release:NULL);BUG_ON(!fn);fn(d);kfree(name);}
struct device *get_device(struct device *d){if(d){BUG_ON(refcount_read(&d->refs.refcount)==0);kref_get(&d->refs);}return d;}
void put_device(struct device *d){if(d)kref_put(&d->refs,release_device);}
const char *dev_name(const struct device *d){return d->kobj.name?d->kobj.name:(d->init_name?d->init_name:"");}
int dev_set_name(struct device *d,const char *f,...){va_list a,b;va_start(a,f);va_copy(b,a);int n=vsnprintf(NULL,0,f,a);va_end(a);char *p=n>=0?kmalloc(n+1,GFP_KERNEL):NULL;if(!p){va_end(b);return -ENOMEM;}vsnprintf(p,n+1,f,b);va_end(b);kfree(d->kobj.name);d->kobj.name=p;return 0;}
void device_lock(struct device *d){mutex_lock(&d->mutex);}void device_unlock(struct device *d){mutex_unlock(&d->mutex);}int device_trylock(struct device *d){return mutex_trylock(&d->mutex);}
int bus_register(const struct bus_type *b){if(!b||!b->name)return -EINVAL;if(find_bus(b))return -EEXIST;struct bus_state *s=kzalloc(sizeof(*s),GFP_KERNEL);if(!s)return -ENOMEM;s->bus=b;INIT_LIST_HEAD(&s->devices);INIT_LIST_HEAD(&s->drivers);list_add_tail(&s->entry,&buses);return 0;}
void bus_unregister(const struct bus_type *b){struct bus_state *s=find_bus(b);if(!s)return;BUG_ON(!list_empty(&s->devices)||!list_empty(&s->drivers)||s->notify);list_del(&s->entry);kfree(s);}
static void notify_bus(struct device *d,unsigned event){struct bus_state *s=find_bus(d->bus);if(s)for(struct notifier_block *n=s->notify;n;){struct notifier_block *next=n->next;n->notifier_call(n,event,d);n=next;}}
int bus_register_notifier(const struct bus_type *b,struct notifier_block *n){struct bus_state *s=find_bus(b);if(!s||!n)return -ENODEV;for(struct notifier_block *p=s->notify;p;p=p->next)if(p==n)return -EEXIST;n->next=s->notify;s->notify=n;return 0;}
int bus_unregister_notifier(const struct bus_type *b,struct notifier_block *n){struct bus_state *s=find_bus(b);if(!s)return -ENODEV;struct notifier_block **p=&s->notify;while(*p&&*p!=n)p=&(*p)->next;if(!*p)return -ENOENT;*p=n->next;n->next=NULL;return 0;}
static int probe_driver(struct device *d,struct device_driver *drv){if(d->driver||!d->registered||d->bus!=drv->bus)return 0;int match=d->bus->match?d->bus->match(d,drv):1;if(match<=0)return match;device_lock(d);d->driver=drv;int r=d->bus->probe?d->bus->probe(d):(drv->probe?drv->probe(d):0);if(!r){r=add_groups(d,drv->dev_groups);if(r){if(d->bus->remove)d->bus->remove(d);else if(drv->remove)drv->remove(d);}}if(r){release_managed(d);d->driver=NULL;d->driver_data=NULL;}device_unlock(d);return r<0?r:1;}
static int driver_registered(struct bus_state *s,const struct device_driver *d){struct device_driver *p;list_for_each_entry(p,&s->drivers,registry)if(p==d)return 1;return 0;}
static int snapshot_drivers(struct bus_state *s,struct device_driver ***out,size_t *count){size_t n=0;struct device_driver *p;list_for_each_entry(p,&s->drivers,registry)n++;*out=NULL;*count=n;if(!n)return 0;*out=kcalloc(n,sizeof(**out),GFP_KERNEL);if(!*out)return -ENOMEM;n=0;list_for_each_entry(p,&s->drivers,registry)(*out)[n++]=p;return 0;}
int device_attach(struct device *d){if(!d||!d->registered)return -ENODEV;if(d->driver)return 1;if(!d->bus)return 0;struct bus_state *s=find_bus(d->bus);if(!s)return -ENODEV;struct device_driver **v;size_t n;int r=snapshot_drivers(s,&v,&n);if(r)return r;get_device(d);for(size_t i=0;i<n;i++){if(!d->registered||!driver_registered(s,v[i]))continue;r=probe_driver(d,v[i]);if(r>0||r==-EPROBE_DEFER)break;r=0;}put_device(d);kfree(v);return r;}
void device_release_driver(struct device *d){if(!d||!d->driver)return;device_lock(d);remove_groups(d,d->driver->dev_groups);if(d->bus&&d->bus->remove)d->bus->remove(d);else if(d->driver->remove)d->driver->remove(d);release_managed(d);d->driver=NULL;d->driver_data=NULL;device_unlock(d);}
int device_bind_driver(struct device *d){if(!d||!d->registered||!d->driver)return -EINVAL;return add_groups(d,d->driver->dev_groups);}
int device_reprobe(struct device *d){device_release_driver(d);int r=device_attach(d);return r<0?r:0;}
int device_add(struct device *d){if(!d||d->registered||!refcount_read(&d->refs.refcount)||!dev_name(d)[0])return -EINVAL;struct bus_state *s=d->bus?find_bus(d->bus):NULL;if(d->bus&&!s)return -ENODEV;if(d->parent&&!d->parent->registered)return -ENODEV;if(s){struct device *p;list_for_each_entry(p,&s->devices,bus_entry)if(!strcmp(dev_name(d),dev_name(p)))return -EEXIST;}
 if(d->parent){get_device(d->parent);list_add_tail(&d->sibling,&d->parent->children);d->kobj.parent=&d->parent->kobj;}
 if(s){list_add_tail(&d->bus_entry,&s->devices);}
 d->registered=1;
 int r=add_groups(d,d->groups);if(!r&&d->type)r=add_groups(d,d->type->groups);if(!r&&d->bus)r=add_groups(d,d->bus->dev_groups);
 if(r){remove_attr(d,NULL);if(s)list_del_init(&d->bus_entry);if(d->parent){list_del_init(&d->sibling);put_device(d->parent);}d->registered=0;return r;}
 notify_bus(d,BUS_NOTIFY_ADD_DEVICE);device_attach(d);return 0;}
void device_del(struct device *d){if(!d||!d->registered)return;device_release_driver(d);BUG_ON(!list_empty(&d->children));remove_attr(d,NULL);notify_bus(d,BUS_NOTIFY_DEL_DEVICE);if(d->bus)list_del_init(&d->bus_entry);if(d->parent){list_del_init(&d->sibling);put_device(d->parent);}d->registered=0;}
int device_register(struct device *d){device_initialize(d);return device_add(d);}
void device_unregister(struct device *d){device_del(d);put_device(d);}
int bus_for_each_dev(const struct bus_type *b,struct device *start,void *data,int (*fn)(struct device *,void *)){struct bus_state *s=find_bus(b);if(!s)return -ENODEV;size_t n=0;struct device *d;list_for_each_entry(d,&s->devices,bus_entry)n++;if(!n)return 0;struct device **v=kcalloc(n,sizeof(*v),GFP_KERNEL);if(!v)return -ENOMEM;size_t i=0;list_for_each_entry(d,&s->devices,bus_entry)v[i++]=get_device(d);int r=0,go=!start;for(i=0;i<n;i++){d=v[i];if(!go){if(d==start)go=1;continue;}if(!r&&d->registered&&d->bus==b)r=fn(d,data);}for(i=0;i<n;i++)put_device(v[i]);kfree(v);return r;}
struct attach_arg {struct device_driver *driver;};
static int attach_device(struct device *d,void *p){struct attach_arg *a=p;if(!d->driver){int r=probe_driver(d,a->driver);if(r==-EPROBE_DEFER)return 0;}return 0;}
int driver_attach(struct device_driver *d){struct attach_arg a={d};return bus_for_each_dev(d->bus,NULL,&a,attach_device);}
int driver_register(struct device_driver *d){if(!d||!d->name||!d->bus)return -EINVAL;struct bus_state *s=find_bus(d->bus);if(!s)return -ENODEV;struct device_driver *p;list_for_each_entry(p,&s->drivers,registry)if(p==d||!strcmp(p->name,d->name))return -EEXIST;list_add_tail(&d->registry,&s->drivers);int r=0;const struct attribute_group **g=d->bus->drv_groups;if(g)for(unsigned i=0;g[i];i++){if(g[i]->name||g[i]->is_visible||g[i]->bin_attrs){r=-EOPNOTSUPP;break;}if(g[i]->attrs)for(unsigned j=0;g[i]->attrs[j];j++){r=driver_create_file(d,container_of(g[i]->attrs[j],struct driver_attribute,attr));if(r)break;}if(r)break;}if(!r)r=driver_attach(d);if(r){remove_attr(d,NULL);list_del_init(&d->registry);}return r;}
static int detach_device(struct device *d,void *p){if(d->driver==p)device_release_driver(d);return 0;}
void driver_unregister(struct device_driver *d){struct bus_state *s=find_bus(d->bus);if(!s)return;struct device_driver *p;int found=0;list_for_each_entry(p,&s->drivers,registry)if(p==d){found=1;break;}if(!found)return;remove_attr(d,NULL);list_del_init(&d->registry);/* detach cannot fail because OOM would leave callbacks into removed code */for(;;){struct device *v=NULL;list_for_each_entry(v,&s->devices,bus_entry)if(v->driver==d)break;if(&v->bus_entry==&s->devices)break;get_device(v);detach_device(v,d);put_device(v);}}
int bus_for_each_drv(const struct bus_type *b,struct device_driver *start,void *data,int (*fn)(struct device_driver *,void *)){struct bus_state *s=find_bus(b);if(!s)return -ENODEV;struct device_driver **v;size_t n;int r=snapshot_drivers(s,&v,&n);if(r)return r;int go=!start;for(size_t i=0;i<n;i++){if(!go){if(v[i]==start)go=1;continue;}if(driver_registered(s,v[i])){r=fn(v[i],data);if(r)break;}}kfree(v);return r;}
struct finder {const void *data;int (*match)(struct device *,const void *);struct device *found;};
static int find_match(struct device *d,void *p){struct finder *f=p;if(!f->match(d,f->data))return 0;f->found=get_device(d);return 1;}
struct device *bus_find_device(const struct bus_type *b,struct device *start,const void *data,int (*match)(struct device *,const void *)){struct finder f={data,match,NULL};bus_for_each_dev(b,start,&f,find_match);return f.found;}
int device_for_each_child(struct device *d,void *data,int (*fn)(struct device *,void *)){size_t n=0;struct device *p;list_for_each_entry(p,&d->children,sibling)n++;if(!n)return 0;struct device **v=kcalloc(n,sizeof(*v),GFP_KERNEL);if(!v)return -ENOMEM;size_t i=0;list_for_each_entry(p,&d->children,sibling)v[i++]=get_device(p);int r=0;for(i=0;i<n;i++)if(!r&&v[i]->registered)r=fn(v[i],data);for(i=0;i<n;i++)put_device(v[i]);kfree(v);return r;}
/* Attribute files are backed by the internal registry. UI exposure can call
 * tmc_usbcore_attribute_{read,write}; unsupported debugfs stays unavailable. */
struct attr_file {struct list_head entry;void *owner;const void *attr;unsigned is_driver;};static LIST_HEAD(attributes);
static int add_attr(void *owner,const void *attr,unsigned driver){if(!owner||!attr)return -EINVAL;const struct attribute *a=attr;struct attr_file *p;list_for_each_entry(p,&attributes,entry)if(p->owner==owner&&!strcmp(((const struct attribute *)p->attr)->name,a->name))return -EEXIST;p=kmalloc(sizeof(*p),GFP_KERNEL);if(!p)return -ENOMEM;p->owner=owner;p->attr=attr;p->is_driver=driver;list_add_tail(&p->entry,&attributes);return 0;}
static void remove_attr(void *owner,const void *attr){struct attr_file *p,*n;list_for_each_entry_safe(p,n,&attributes,entry)if(p->owner==owner&&(!attr||p->attr==attr)){list_del(&p->entry);kfree(p);}}
int device_create_file(struct device *d,const struct device_attribute *a){if(!d||!d->registered)return -ENODEV;return add_attr(d,a,0);}
void device_remove_file(struct device *d,const struct device_attribute *a){remove_attr(d,a);}
int driver_create_file(struct device_driver *d,const struct driver_attribute *a){return add_attr(d,a,1);}
void driver_remove_file(struct device_driver *d,const struct driver_attribute *a){remove_attr(d,a);}
ssize_t tmc_usbcore_attribute_read(void *owner,const char *name,char *page){struct attr_file *p;list_for_each_entry(p,&attributes,entry)if(p->owner==owner&&!strcmp(((const struct attribute *)p->attr)->name,name)){if(p->is_driver==2){const struct bin_attribute *a=p->attr;return a->read?a->read(NULL,&((struct device *)owner)->kobj,(void *)a,page,0,a->size):-EACCES;}if(p->is_driver){const struct driver_attribute *a=p->attr;return a->show?a->show(owner,page):-EACCES;}const struct device_attribute *a=p->attr;return a->show?a->show(owner,(void *)a,page):-EACCES;}return -ENOENT;}
ssize_t tmc_usbcore_attribute_write(void *owner,const char *name,const char *buf,size_t n){struct attr_file *p;list_for_each_entry(p,&attributes,entry)if(p->owner==owner&&!strcmp(((const struct attribute *)p->attr)->name,name)){if(p->is_driver==2)return -EACCES;if(p->is_driver){const struct driver_attribute *a=p->attr;return a->store?a->store(owner,buf,n):-EACCES;}const struct device_attribute *a=p->attr;return a->store?a->store(owner,(void *)a,buf,n):-EACCES;}return -ENOENT;}
/* Explicit unsupported debug filesystem; never returns a made-up node. */
struct dentry *debugfs_create_file(const char *n,umode_t m,struct dentry *p,void *d,const struct file_operations *f){(void)n;(void)m;(void)p;(void)d;(void)f;return ERR_PTR(-ENODEV);}
void debugfs_lookup_and_remove(const char *n,struct dentry *p){(void)n;(void)p;}
int atomic_notifier_chain_register(struct atomic_notifier_head *h,struct notifier_block *n){for(struct notifier_block *p=h->head;p;p=p->next)if(p==n)return -EEXIST;struct notifier_block **p=&h->head;while(*p&&(*p)->priority>=n->priority)p=&(*p)->next;n->next=*p;*p=n;return 0;}
int atomic_notifier_chain_unregister(struct atomic_notifier_head *h,struct notifier_block *n){struct notifier_block **p=&h->head;while(*p&&*p!=n)p=&(*p)->next;if(!*p)return -ENOENT;*p=n->next;n->next=NULL;return 0;}
int add_uevent_var(struct kobj_uevent_env *e,const char *fmt,...){if(e->count>=ARRAY_SIZE(e->envp)||e->used>=sizeof(e->buf))return -ENOMEM;va_list a;va_start(a,fmt);int n=vsnprintf(e->buf+e->used,sizeof(e->buf)-e->used,fmt,a);va_end(a);if(n<0||(size_t)n>=sizeof(e->buf)-e->used)return -ENOMEM;e->envp[e->count++]=e->buf+e->used;e->used+=n+1;return 0;}
int utf16s_to_utf8s(const void *input,int units,int endian,u8 *out,int capacity){if(!input||!out||units<0||capacity<0||endian!=UTF16_LITTLE_ENDIAN)return -EINVAL;const u8 *s=input;int n=0;for(int i=0;i<units;i++){u32 c=s[i*2]|(u32)s[i*2+1]<<8;if(c>=0xd800&&c<=0xdbff){if(i+1>=units)return -EINVAL;u32 low=s[(i+1)*2]|(u32)s[(i+1)*2+1]<<8;if(low<0xdc00||low>0xdfff)return -EINVAL;c=0x10000+((c-0xd800)<<10)+(low-0xdc00);i++;}else if(c>=0xdc00&&c<=0xdfff)return -EINVAL;int bytes=c<0x80?1:c<0x800?2:c<0x10000?3:4;if(n+bytes>capacity)break;if(bytes==1)out[n++]=c;else{out[n++]=(bytes==2?0xc0:bytes==3?0xe0:0xf0)|(c>>(6*(bytes-1)));for(int j=bytes-2;j>=0;j--)out[n++]=0x80|((c>>(6*j))&0x3f);}}return n;}

static void remove_groups(struct device *d,const struct attribute_group **groups){if(!groups)return;for(unsigned i=0;groups[i];i++){struct attribute **a=groups[i]->attrs;if(a)for(unsigned j=0;a[j];j++)remove_attr(d,a[j]);struct bin_attribute **b=groups[i]->bin_attrs;if(b)for(unsigned j=0;b[j];j++)remove_attr(d,b[j]);}}
static int add_groups(struct device *d,const struct attribute_group **groups){if(!groups)return 0;int r=0;for(unsigned i=0;groups[i];i++){if(groups[i]->name||groups[i]->is_visible){r=-EOPNOTSUPP;break;}struct attribute **a=groups[i]->attrs;if(a)for(unsigned j=0;a[j];j++){r=add_attr(d,a[j],0);if(r)goto fail;}struct bin_attribute **b=groups[i]->bin_attrs;if(b)for(unsigned j=0;b[j];j++){r=add_attr(d,b[j],2);if(r)goto fail;}}if(!r)return 0;fail:remove_groups(d,groups);return r;}

int tmc_usbcore_attribute_exists(void *owner,const char *name){struct attr_file *p;list_for_each_entry(p,&attributes,entry)if(p->owner==owner&&!strcmp(((const struct attribute *)p->attr)->name,name))return 1;return 0;}

void *devres_open_group(struct device *d,void *id,gfp_t f){struct managed *m=kzalloc(sizeof(*m),f);if(!m)return NULL;m->data=id?id:m;list_add(&m->entry,&d->managed);return m->data;}
int devres_release_group(struct device *d,void *id){struct managed *m;int found=0;list_for_each_entry(m,&d->managed,entry)if(!m->release&&m->data==id){found=1;break;}if(!found)return -ENOENT;for(;;){m=list_first_entry(&d->managed,struct managed,entry);int end=!m->release&&m->data==id;list_del(&m->entry);void (*fn)(void *)=m->release;void *data=m->data;kfree(m);if(fn)fn(data);if(end)break;}return 0;}
static int rescan_one(struct device *d,void *p){(void)p;int r=device_attach(d);return r<0?r:0;}
int bus_rescan_devices(const struct bus_type *b){return bus_for_each_dev(b,NULL,NULL,rescan_one);}
