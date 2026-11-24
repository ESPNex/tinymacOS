/* Platform/runtime services, not substitute controller scheduling. */
struct IrqRegistration {unsigned irq;irqreturn_t (*handler)(int,void *);void *context;};
static struct IrqRegistration irq_registry[XHCI_CONTROLLERS];
int request_irq(unsigned irq,irqreturn_t (*fn)(int,void *),unsigned long flags,const char *name,void *ctx)
{
 (void)name;if(!irq||!fn||!ctx||(flags&~IRQF_SHARED))return -EINVAL;
 struct IrqRegistration *free=NULL;for(unsigned i=0;i<ARRAY_SIZE(irq_registry);i++){if(irq_registry[i].context==ctx)return -EEXIST;if(!irq_registry[i].context)free=irq_registry+i;}
 if(!free)return -ENOSPC;*free=(struct IrqRegistration){irq,fn,ctx};return 0;
}
void free_irq(unsigned irq,void *ctx)
{for(unsigned i=0;i<ARRAY_SIZE(irq_registry);i++)if(irq_registry[i].irq==irq&&irq_registry[i].context==ctx){memset(irq_registry+i,0,sizeof(*irq_registry));return;}}
irqreturn_t usb_hcd_irq(int irq,void *ctx){(void)irq;struct usb_hcd *h=ctx;return h&&h->accessible?h->driver->irq(h):IRQ_NONE;}
unsigned num_online_cpus(void){return 1;} /* This private executor is pinned to CPU0. */
int pci_alloc_irq_vectors(struct pci_dev *p,unsigned min,unsigned max,unsigned flags){(void)p;(void)min;(void)max;(void)flags;return -EOPNOTSUPP;}
int pci_irq_vector(struct pci_dev *p,unsigned i){return !i?p->irq:-EINVAL;}
void pci_free_irq_vectors(struct pci_dev *p){if(p->msix_enabled)BUG();}
const char *pci_name(struct pci_dev *p){return p->dev.name;}
/* CONFIG_PM and CONFIG_RESET_CONTROLLER are off. Absent optional reset lines
 * and PM wrappers have the same semantics as upstream's off-configuration. */
struct reset_control *devm_reset_control_get_optional_exclusive(struct device *d,const char *id){(void)d;(void)id;return NULL;}
int reset_control_reset(struct reset_control *r){return r?-EOPNOTSUPP:0;}
void pm_runtime_get_noresume(struct device *d){(void)d;}
void pm_runtime_put_noidle(struct device *d){(void)d;}
void pm_runtime_forbid(struct device *d){(void)d;}
void pm_runtime_allow(struct device *d){(void)d;}
int pm_runtime_get(struct device *d){(void)d;return -ENOSYS;}
int pm_runtime_put(struct device *d){(void)d;return -ENOSYS;}
int pci_choose_state(struct pci_dev *p,pm_message_t msg){(void)p;(void)msg;return PCI_D0;}
int pci_set_power_state(struct pci_dev *p,int state)
{if(state!=PCI_D0&&state!=PCI_D3hot)return -EINVAL;int cap=pci_find_capability(p,1);if(!cap)return state==PCI_D0?0:-EOPNOTSUPP;u16 v;pci_read_config_word(p,cap+4,&v);pci_write_config_word(p,cap+4,(v&~3u)|state);return 0;}
void pci_d3cold_disable(struct pci_dev *p){(void)p;} /* CONFIG_PM off. */
int dma_set_mask(struct device *d,u64 mask){return dma_set_coherent_mask(d,mask);}
int dma_set_max_seg_size(struct device *d,unsigned size){if(!d||!size)return -EINVAL;return 0;} /* DMA API imposes a stricter 64KiB allocation limit. */
void init_completion(struct completion *c){__atomic_store_n(&c->done,0,__ATOMIC_RELEASE);}
void complete(struct completion *c){__atomic_add_fetch(&c->done,1,__ATOMIC_RELEASE);}
void complete_all(struct completion *c){__atomic_store_n(&c->done,UINT_MAX,__ATOMIC_RELEASE);}
unsigned long wait_for_completion_timeout(struct completion *c,unsigned long timeout)
{
 u64 start=tmc_lx_os_clock_ms();for(;;){unsigned n=__atomic_load_n(&c->done,__ATOMIC_ACQUIRE);if(n){if(n!=UINT_MAX)__atomic_sub_fetch(&c->done,1,__ATOMIC_ACQ_REL);u64 elapsed=tmc_lx_os_clock_ms()-start;return elapsed<timeout?timeout-elapsed:1;}if(tmc_lx_os_clock_ms()-start>=timeout)return 0;xhci_runtime_progress();}
}
void wait_for_completion(struct completion *c){while(!wait_for_completion_timeout(c,1000));}
void add_timer(struct timer_list *t){if(t->pending)BUG();mod_timer(t,t->expires);}
static void delayed_timer(struct timer_list *t){struct delayed_work *d=container_of(t,struct delayed_work,timer);schedule_work(&d->work);}
void xhci_init_delayed_work(struct delayed_work *d,void (*fn)(struct work_struct *))
{memset(d,0,sizeof(*d));d->work.function=fn;timer_setup(&d->timer,delayed_timer,0);}
static int cancel_queued_work(struct work_struct *w)
{
 struct work_struct **link=&work_head;while(*link&&*link!=w)link=&(*link)->next;
 if(!*link)return 0;*link=w->next;if(!*link)work_tail=link;w->pending=0;w->next=NULL;return 1;
}
int cancel_delayed_work(struct delayed_work *d){int a=del_timer(&d->timer);return cancel_queued_work(&d->work)||a;}
int cancel_delayed_work_sync(struct delayed_work *d){if(d->work.running)BUG();return cancel_delayed_work(d);}
int mod_delayed_work(void *queue,struct delayed_work *d,unsigned long delay)
{if(queue!=system_wq)BUG();int old=cancel_delayed_work(d);mod_timer(&d->timer,jiffies+delay);return old;}
struct tmc_radix_entry {unsigned long key;void *value;struct tmc_radix_entry *next;};
static struct tmc_radix_entry *preloaded;
int radix_tree_maybe_preload(gfp_t flags){if(preloaded)return -EBUSY;preloaded=kzalloc(sizeof(*preloaded),flags);return preloaded?0:-ENOMEM;}
void radix_tree_preload_end(void){kfree(preloaded);preloaded=NULL;}
void *radix_tree_lookup(struct radix_tree_root *r,unsigned long key){for(struct tmc_radix_entry *e=r->entries;e;e=e->next)if(e->key==key)return e->value;return NULL;}
int radix_tree_insert(struct radix_tree_root *r,unsigned long key,void *value)
{if(!value)return -EINVAL;if(radix_tree_lookup(r,key))return -EEXIST;struct tmc_radix_entry *e=preloaded?preloaded:kzalloc(sizeof(*e),GFP_ATOMIC);if(!e)return -ENOMEM;preloaded=NULL;e->key=key;e->value=value;e->next=r->entries;r->entries=e;return 0;}
void *radix_tree_delete(struct radix_tree_root *r,unsigned long key)
{struct tmc_radix_entry **link=&r->entries;while(*link&&(*link)->key!=key)link=&(*link)->next;if(!*link)return NULL;struct tmc_radix_entry *e=*link;*link=e->next;void *v=e->value;kfree(e);return v;}
size_t sg_pcopy_to_buffer(struct scatterlist *sg,unsigned count,void *buffer,size_t n,size_t skip)
{size_t copied=0;for(unsigned i=0;i<count&&sg&&copied<n;i++,sg=sg->next){if(skip>=sg->length){skip-=sg->length;continue;}size_t bytes=min(n-copied,sg->length-skip);if(!sg->address)break;memcpy((u8 *)buffer+copied,(u8 *)sg->address+skip,bytes);copied+=bytes;skip=0;}return copied;}
size_t sg_pcopy_from_buffer(struct scatterlist *sg,unsigned count,const void *buffer,size_t n,size_t skip)
{size_t copied=0;for(unsigned i=0;i<count&&sg&&copied<n;i++,sg=sg->next){if(skip>=sg->length){skip-=sg->length;continue;}size_t bytes=min(n-copied,sg->length-skip);if(!sg->address)break;memcpy((u8 *)sg->address+skip,(const u8 *)buffer+copied,bytes);copied+=bytes;skip=0;}return copied;}
struct StreamingMap {struct device *dev;void *original,*coherent;size_t n;dma_addr_t bus;enum dma_data_direction direction;};
static struct StreamingMap streaming[128];
dma_addr_t dma_map_single(struct device *dev,void *cpu,size_t n,enum dma_data_direction dir)
{
 if(!dev||!cpu||!n||n>65536||dir==DMA_NONE)return 0;struct StreamingMap *m=NULL;for(unsigned i=0;i<ARRAY_SIZE(streaming);i++)if(!streaming[i].dev){m=streaming+i;break;}if(!m)return 0;
 dma_addr_t bus;void *data=dma_alloc_coherent(dev,n,&bus,GFP_ATOMIC);if(!data)return 0;memcpy(data,cpu,n);*m=(struct StreamingMap){dev,cpu,data,n,bus,dir};return bus;
}
int dma_mapping_error(struct device *d,dma_addr_t bus){(void)d;return !bus;}
void dma_unmap_single(struct device *d,dma_addr_t bus,size_t n,enum dma_data_direction dir)
{for(unsigned i=0;i<ARRAY_SIZE(streaming);i++){struct StreamingMap *m=streaming+i;if(m->dev==d&&m->bus==bus){if(m->n!=n||m->direction!=dir)BUG();if(dir!=DMA_TO_DEVICE)memcpy(m->original,m->coherent,n);dma_free_coherent(d,n,m->coherent,bus);memset(m,0,sizeof(*m));return;}}BUG();}
/* A platform child cannot be advertised successfully without its role-switch
 * driver. Allocation/resource bookkeeping is real; registration fails closed. */
struct platform_device *platform_device_alloc(const char *name,int id){struct platform_device *p=kzalloc(sizeof(*p),GFP_KERNEL);if(p){p->name=name;p->id=id;}return p;}
int platform_device_add_resources(struct platform_device *p,const struct resource *r,unsigned n)
{if(!p||!r||!n||p->resources)return -EINVAL;p->resources=kcalloc(n,sizeof(*r),GFP_KERNEL);if(!p->resources)return -ENOMEM;memcpy(p->resources,r,n*sizeof(*r));p->count=n;return 0;}
int platform_device_add(struct platform_device *p){(void)p;return -EOPNOTSUPP;}
void platform_device_put(struct platform_device *p){if(p){kfree(p->resources);kfree(p);}}
void platform_device_unregister(struct platform_device *p){platform_device_put(p);}
int device_create_managed_software_node(struct device *d,const struct property_entry *p,const void *parent){(void)d;(void)p;(void)parent;return -EOPNOTSUPP;}
int devm_add_action_or_reset(struct device *d,void (*fn)(void *),void *p){(void)d;if(fn)fn(p);return -EOPNOTSUPP;}
void usb_wakeup_notification(struct usb_device *hub,unsigned port){if(hub&&port>0&&port<=31){struct usb_hcd *h=bus_to_hcd(hub->bus);h->root_changes|=1u<<port;usb_hcd_resume_root_hub(h);}}
int tmc_linux_xhci_device_port(TmcXhciDevice *d,TmcXhciDevice *parent,unsigned port)
{
 if(!d||!port||port>15||d->core.slot_id||d->parent_ref)return -EINVAL;
 struct usb_hcd *h=bus_to_hcd(d->core.bus);
 if(parent){
  if(parent->core.bus!=d->core.bus||!parent->core.slot_id||port>(unsigned)parent->core.maxchild)return -EINVAL;
  unsigned shift=0;for(struct usb_device *p=&parent->core;p->parent&&p->parent->parent;p=p->parent)shift+=4;
  if(shift>=20)return -EINVAL;
  d->core.route=parent->core.route|(min(port,15u)<<shift);
  d->parent_ref=usb_get_dev(&parent->core);
  if(d->core.speed<USB_SPEED_HIGH){if(parent->core.speed==USB_SPEED_HIGH){d->core.tt=&parent->hub.tt;d->core.ttport=port;}else{d->core.tt=parent->core.tt;d->core.ttport=parent->core.ttport;}}
 }else if(port>(unsigned)h->self.root_hub->maxchild)return -EINVAL;
 d->core.parent=parent?&parent->core:h->self.root_hub;d->core.portnum=port;
 int rc=-ENOMEM;
 if(h->driver->alloc_dev(h,&d->core)){
  rc=h->driver->enable_device(h,&d->core);
  if(!rc)return 0;
  h->driver->free_dev(h,&d->core);d->core.slot_id=0;
 }
 if(d->parent_ref)usb_put_dev(d->parent_ref);
 d->parent_ref=NULL;d->core.parent=NULL;d->core.tt=NULL;d->core.ttport=0;d->core.route=0;d->core.portnum=0;
 return rc;
}
int tmc_linux_xhci_hub_device(TmcXhciDevice *d,unsigned ports,unsigned multi,unsigned think_time)
{
 if(!d||!ports||ports>15)return -EINVAL;d->core.maxchild=ports;d->hub.hdev=&d->core;d->hub.tt.hub=&d->core;d->hub.tt.multi=!!multi;d->hub.tt.think_time=think_time;INIT_LIST_HEAD(&d->hub.tt.clear_list);d->hub.tt.clear_work.function=hub_tt_work;return bus_to_hcd(d->core.bus)->driver->update_hub_device(bus_to_hcd(d->core.bus),&d->core,&d->hub.tt,GFP_KERNEL);
}

#include "../../vendor/linux-v6.12/xhci-pci-asmedia.inc"
#include "../../vendor/linux-v6.12/xhci-pci-disable.inc"

int pci_write_config_dword(struct pci_dev *p,int off,u32 value){if(!p||off<0||off>252||(off&3))return -EINVAL;extern void tmc_xhci_os_pci_write(u32,unsigned,u32);tmc_xhci_os_pci_write(p->address,off,value);return 0;}
