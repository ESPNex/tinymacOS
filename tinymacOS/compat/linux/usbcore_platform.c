/* SPDX-License-Identifier: GPL-2.0-only */
#include <tmc_kernel.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <usb.h>
#include <phy.h>
/* This HCD boundary deliberately uses PIO/copy semantics. The underlying
 * original controller runtime owns all actual DMA, never this ABI's device. */
dma_addr_t dma_map_single(struct device *d,void *p,size_t n,enum dma_data_direction dir){(void)d;(void)p;(void)n;(void)dir;return DMA_MAPPING_ERROR;}
int dma_mapping_error(struct device *d,dma_addr_t a){(void)d;return a==DMA_MAPPING_ERROR;}
void dma_unmap_single(struct device *d,dma_addr_t a,size_t n,enum dma_data_direction dir){(void)d;(void)a;(void)n;(void)dir;BUG();}
dma_addr_t dma_map_page(struct device *d,void *p,size_t o,size_t n,enum dma_data_direction dir){(void)o;return dma_map_single(d,p,n,dir);}
void dma_unmap_page(struct device *d,dma_addr_t a,size_t n,enum dma_data_direction dir){dma_unmap_single(d,a,n,dir);}
int dma_map_sg(struct device *d,struct scatterlist *s,int n,enum dma_data_direction dir){(void)d;(void)s;(void)n;(void)dir;return 0;}
void dma_unmap_sg(struct device *d,struct scatterlist *s,int n,enum dma_data_direction dir){(void)d;(void)s;(void)n;(void)dir;BUG();}
void *dma_alloc_coherent(struct device *d,size_t n,dma_addr_t *a,gfp_t f){(void)d;(void)n;(void)f;*a=DMA_MAPPING_ERROR;return NULL;}
void dma_free_coherent(struct device *d,size_t n,void *p,dma_addr_t a){(void)d;(void)n;(void)a;BUG_ON(p!=NULL);}
struct dma_pool *dma_pool_create(const char *s,struct device *d,size_t n,size_t a,size_t b){(void)s;(void)d;(void)n;(void)a;(void)b;return NULL;}
void dma_pool_destroy(struct dma_pool *p){BUG_ON(p!=NULL);}
void *dma_pool_alloc(struct dma_pool *p,gfp_t f,dma_addr_t *a){(void)p;(void)f;*a=DMA_MAPPING_ERROR;return NULL;}
void dma_pool_free(struct dma_pool *p,void *v,dma_addr_t a){(void)p;(void)a;BUG_ON(v!=NULL);}
void *gen_pool_dma_alloc(struct gen_pool *p,size_t n,dma_addr_t *a){(void)p;(void)n;*a=DMA_MAPPING_ERROR;return NULL;}
void gen_pool_free(struct gen_pool *p,unsigned long a,size_t n){(void)p;(void)a;(void)n;BUG();}
int request_irq(unsigned n,irqreturn_t (*fn)(int,void *),unsigned long f,const char *s,void *p){(void)n;(void)fn;(void)f;(void)s;(void)p;return -EOPNOTSUPP;}
void free_irq(unsigned n,void *p){(void)n;(void)p;BUG();}
int object_is_on_stack(const void *p){(void)p;return 1;/* direct mapping unsupported */}
const struct new_utsname *init_utsname(void){static const struct new_utsname u={"tinymacOS","10.1.7"};return &u;}
/* Genuine metadata registry and references used by upstream hub/port code. */
extern int tmc_usbcore_attribute_exists(void *,const char *);
struct kernfs_node *sysfs_get_dirent(struct kernfs_node *root,const char *name){if(!root||!root->owner)return NULL;struct device *d=kobj_to_dev(root->owner);if(!d->registered||!tmc_usbcore_attribute_exists(d,name))return NULL;struct kernfs_node *n=kzalloc(sizeof(*n),GFP_KERNEL);if(!n)return NULL;n->name=kstrdup(name,GFP_KERNEL);if(!n->name){kfree(n);return NULL;}n->owner=&get_device(d)->kobj;n->references=1;return n;}
void sysfs_put(struct kernfs_node *n){if(!n)return;BUG_ON(!n->references);if(!--n->references){put_device(kobj_to_dev(n->owner));kfree(n->name);kfree(n);}}
void sysfs_notify_dirent(struct kernfs_node *n){if(n&&n->owner&&kobj_to_dev(n->owner)->registered)n->events++;}
void sysfs_notify(struct kobject *k,const char *group,const char *name){if(!k)return;char path[128];snprintf(path,sizeof(path),"%s%s%s",group?group:"",group?"/":"",name);if(tmc_usbcore_attribute_exists(kobj_to_dev(k),path))k->directory.events++;}
struct kernfs_node *sysfs_break_active_protection(struct kobject *k,const struct attribute *a){struct kernfs_node *n=sysfs_get_dirent(k->sd,a->name);if(n)n->active++;return n;}
void sysfs_unbreak_active_protection(struct kernfs_node *n){if(n){BUG_ON(!n->active);n->active--;sysfs_put(n);}}
struct metadata_link {struct list_head entry;struct kobject *from,*to;char *name;};static LIST_HEAD(links);
int sysfs_create_link(struct kobject *from,struct kobject *to,const char *name){if(!from||!to||!kobj_to_dev(from)->registered||!kobj_to_dev(to)->registered)return -ENODEV;struct metadata_link *l;list_for_each_entry(l,&links,entry)if(l->from==from&&!strcmp(l->name,name))return -EEXIST;l=kzalloc(sizeof(*l),GFP_KERNEL);if(!l)return -ENOMEM;l->name=kstrdup(name,GFP_KERNEL);if(!l->name){kfree(l);return -ENOMEM;}l->from=from;l->to=&get_device(kobj_to_dev(to))->kobj;list_add_tail(&l->entry,&links);return 0;}
void sysfs_remove_link(struct kobject *from,const char *name){struct metadata_link *l,*n;list_for_each_entry_safe(l,n,&links,entry)if(l->from==from&&!strcmp(l->name,name)){list_del(&l->entry);put_device(kobj_to_dev(l->to));kfree(l->name);kfree(l);}}
char *kobject_get_path(const struct kobject *k,gfp_t f){size_t n=1;for(const struct kobject *p=k;p;p=p->parent)n+=strlen(p->name?p->name:"")+1;char *s=kmalloc(n,f);if(!s)return NULL;char *e=s+n-1;*e=0;for(const struct kobject *p=k;p;p=p->parent){size_t l=strlen(p->name?p->name:"");e-=l;memcpy(e,p->name?p->name:"",l);*--e='/';}memmove(s,e,(s+n)-e);return s;}
static u64 uevent_sequence;
int kobject_uevent_env(struct kobject *k,int event,char *env[]){if(!k||!kobj_to_dev(k)->registered)return -ENODEV;(void)env;uevent_sequence++;k_log(0,"USB metadata event %d #%llu %s",event,(unsigned long long)uevent_sequence,k->name);return 0;}
int sysfs_emit(char *p,const char *fmt,...){va_list a;va_start(a,fmt);int n=vsnprintf(p,PAGE_SIZE,fmt,a);va_end(a);return n<0?n:min_t(int,n,PAGE_SIZE-1);}
int kstrtou32(const char *s,unsigned base,u32 *out){if(!s||!out||(base&&base!=10&&base!=16))return -EINVAL;u64 v=0;if(!base)base=10;if(base==16&&s[0]=='0'&&(s[1]=='x'||s[1]=='X'))s+=2;unsigned digits=0;for(;*s&&*s!='\n';s++){unsigned c=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='f'?*s-'a'+10:*s>='A'&&*s<='F'?*s-'A'+10:99;if(c>=base)return -EINVAL;v=v*base+c;if(v>U32_MAX)return -EOVERFLOW;digits++;}if(!digits||(*s=='\n'&&s[1]))return -EINVAL;*out=v;return 0;}
int kstrtobool(const char *s,bool *out){if(!s||!out)return -EINVAL;if(!strcmp(s,"1")||!strcmp(s,"1\n")||!strcmp(s,"yes")||!strcmp(s,"yes\n")){*out=1;return 0;}if(!strcmp(s,"0")||!strcmp(s,"0\n")||!strcmp(s,"no")||!strcmp(s,"no\n")){*out=0;return 0;}return -EINVAL;}
struct idr_entry {struct idr_entry *next;int id;void *value;};
int idr_alloc(struct idr *r,void *value,int start,int end,gfp_t f){if(start<0||end<0||(end&&end<=start))return -EINVAL;for(int id=start;id<(end?end:INT_MAX);id++){struct idr_entry *e=r->entries;while(e&&e->id!=id)e=e->next;if(e)continue;e=kmalloc(sizeof(*e),f);if(!e)return -ENOMEM;e->id=id;e->value=value;e->next=r->entries;r->entries=e;return id;}return -ENOSPC;}
void *idr_remove(struct idr *r,int id){struct idr_entry **p=(struct idr_entry **)&r->entries;while(*p&&(*p)->id!=id)p=&(*p)->next;if(!*p)return NULL;struct idr_entry *e=*p;void *v=e->value;*p=e->next;kfree(e);return v;}
void idr_destroy(struct idr *r){while(r->entries)idr_remove(r,((struct idr_entry *)r->entries)->id);}
struct component_registration {struct list_head entry;struct device *dev;const struct component_ops *ops;};static LIST_HEAD(components);
int component_add(struct device *d,const struct component_ops *ops){struct component_registration *c;list_for_each_entry(c,&components,entry)if(c->dev==d)return -EEXIST;c=kzalloc(sizeof(*c),GFP_KERNEL);if(!c)return -ENOMEM;c->dev=d;c->ops=ops;list_add_tail(&c->entry,&components);return 0;}
void component_del(struct device *d,const struct component_ops *ops){struct component_registration *c,*n;list_for_each_entry_safe(c,n,&components,entry)if(c->dev==d&&c->ops==ops){list_del(&c->entry);kfree(c);return;}}
int dev_pm_qos_add_request(struct device *d,struct dev_pm_qos_request *r,int type,int value){(void)d;if(!r||type!=DEV_PM_QOS_FLAGS)return -EINVAL;r->value=value;return 0;}
int dev_pm_qos_remove_request(struct dev_pm_qos_request *r){if(!r)return -EINVAL;r->value=0;return 0;}
int dev_pm_qos_expose_flags(struct device *d,int f){(void)d;(void)f;return -EOPNOTSUPP;/* no userspace PM policy API */}
/* PCI controllers have no separately described DT/ACPI PHY in this port. A
 * NULL optional PHY follows upstream absent-PHY semantics; non-NULL fails. */
struct usb_phy_roothub *usb_phy_roothub_alloc(struct device *d){return d->of_node?ERR_PTR(-EOPNOTSUPP):NULL;}
struct usb_phy_roothub *usb_phy_roothub_alloc_usb3_phy(struct device *d){return usb_phy_roothub_alloc(d);}
int usb_phy_roothub_init(struct usb_phy_roothub *p){return p?-EOPNOTSUPP:0;}
int usb_phy_roothub_exit(struct usb_phy_roothub *p){return p?-EOPNOTSUPP:0;}
int usb_phy_roothub_set_mode(struct usb_phy_roothub *p,enum phy_mode m){(void)m;return p?-EOPNOTSUPP:0;}
int usb_phy_roothub_calibrate(struct usb_phy_roothub *p){return p?-EOPNOTSUPP:0;}
int usb_phy_roothub_power_on(struct usb_phy_roothub *p){return p?-EOPNOTSUPP:0;}
void usb_phy_roothub_power_off(struct usb_phy_roothub *p){BUG_ON(p!=NULL);}
int usb_phy_roothub_notify_connect(struct usb_phy_roothub *p,int port){(void)port;return p?-EOPNOTSUPP:0;}
int usb_phy_roothub_notify_disconnect(struct usb_phy_roothub *p,int port){(void)port;return p?-EOPNOTSUPP:0;}
static struct notifier_block *usb_notifiers;
void usb_register_notify(struct notifier_block *n){struct atomic_notifier_head h={usb_notifiers};int r=atomic_notifier_chain_register(&h,n);BUG_ON(r);usb_notifiers=h.head;}
void usb_unregister_notify(struct notifier_block *n){struct atomic_notifier_head h={usb_notifiers};atomic_notifier_chain_unregister(&h,n);usb_notifiers=h.head;}
static void usb_notify(unsigned e,void *d){for(struct notifier_block *n=usb_notifiers;n;n=n->next)n->notifier_call(n,e,d);}
void usb_notify_add_device(struct usb_device *d){usb_notify(USB_DEVICE_ADD,d);}
void usb_notify_remove_device(struct usb_device *d){usb_notify(USB_DEVICE_REMOVE,d);}
void usb_notify_add_bus(struct usb_bus *b){usb_notify(USB_BUS_ADD,b);}
void usb_notify_remove_bus(struct usb_bus *b){usb_notify(USB_BUS_REMOVE,b);}
extern void tmc_usbcore_os_entropy(const void *,unsigned);
void add_device_randomness(const void *p,unsigned n){tmc_usbcore_os_entropy(p,n);}
int printk_ratelimit(void){static u64 until;static unsigned count;u64 now=k_clock_ms();if(now>=until){until=now+1000;count=0;}return count++<10;}
/* Metadata reads refer to the REAL canonical descriptor/interface objects. */
static ssize_t vendor_show(struct device *d,struct device_attribute *a,char *p){(void)a;return sysfs_emit(p,"%04x\n",le16_to_cpu(to_usb_device(d)->descriptor.idVendor));}
static ssize_t product_show(struct device *d,struct device_attribute *a,char *p){(void)a;return sysfs_emit(p,"%04x\n",le16_to_cpu(to_usb_device(d)->descriptor.idProduct));}
static DEVICE_ATTR(idVendor,0444,vendor_show,NULL);static DEVICE_ATTR(idProduct,0444,product_show,NULL);
static struct attribute *identity_attrs[]={&dev_attr_idVendor.attr,&dev_attr_idProduct.attr,NULL};static const struct attribute_group identity_group={.attrs=identity_attrs};
const struct attribute_group *usb_device_groups[]={&identity_group,NULL};
static ssize_t interface_show(struct device *d,struct device_attribute *a,char *p){(void)a;return sysfs_emit(p,"%02x\n",to_usb_interface(d)->cur_altsetting->desc.bInterfaceClass);}
static DEVICE_ATTR(bInterfaceClass,0444,interface_show,NULL);
static struct attribute *interface_attrs[]={&dev_attr_bInterfaceClass.attr,NULL};static const struct attribute_group interface_group={.attrs=interface_attrs};
const struct attribute_group *usb_interface_groups[]={&interface_group,NULL};
static ssize_t interface_string_show(struct device *d,struct device_attribute *a,char *p){(void)a;const char *s=to_usb_interface(d)->cur_altsetting->string;return sysfs_emit(p,"%s\n",s?s:"");}
static DEVICE_ATTR(interface,0444,interface_string_show,NULL);
void usb_create_sysfs_intf_files(struct usb_interface *i){if(i->cur_altsetting->string&&!tmc_usbcore_attribute_exists(&i->dev,"interface")){int r=device_create_file(&i->dev,&dev_attr_interface);if(r)dev_warn(&i->dev,"interface metadata allocation: %d",r);}}
void usb_remove_sysfs_intf_files(struct usb_interface *i){device_remove_file(&i->dev,&dev_attr_interface);}
int usb_update_wireless_status_attr(struct usb_interface *i){if(!i||!i->dev.registered)return -ENODEV;return -EOPNOTSUPP;}
