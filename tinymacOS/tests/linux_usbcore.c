/* SPDX-License-Identifier: GPL-2.0-only */
#include <tmc_kernel.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <hub.h>
#include <assert.h>
#include <stdio.h>
extern unsigned host_live_allocations(void);extern void host_fail_after(long);
static unsigned probes,removes,releases,managed_releases,notifications;
static void release_dev(struct device *d){(void)d;releases++;}
static int match(struct device *d,const struct device_driver *r){return d->authorized&&!strcmp(r->name,"test-driver");}
static void managed_release(void *p){managed_releases++;kfree(p);}
static int probe(struct device *d){probes++;void *p=kzalloc(16,GFP_KERNEL);if(!p)return -ENOMEM;return devm_add_action_or_reset(d,managed_release,p);}
static int unbind(struct device *d){assert(d->driver);removes++;return 0;}
static const struct bus_type testbus={.name="testbus",.match=match};
static struct device_driver testdrv={.name="test-driver",.bus=&testbus,.probe=probe,.remove=unbind};
static int notify(struct notifier_block *n,unsigned long ev,void *p){(void)n;struct device *d=p;assert(d->registered);assert(ev==BUS_NOTIFY_ADD_DEVICE||ev==BUS_NOTIFY_DEL_DEVICE);notifications++;return 0;}
static ssize_t attr_show(struct device *d,struct device_attribute *a,char *p){(void)d;(void)a;return sprintf(p,"test-value\n");}
static DEVICE_ATTR(value,0444,attr_show,NULL);
static struct attribute *metadata_attrs[]={&dev_attr_value.attr,NULL};
static const struct attribute_group metadata_group={.attrs=metadata_attrs};
static const struct attribute_group *metadata_groups[]={&metadata_group,NULL};
static void new_device(struct device *d,const char *name,struct device *parent){memset(d,0,sizeof(*d));device_initialize(d);d->parent=parent;d->bus=&testbus;d->release=release_dev;d->authorized=1;assert(!dev_set_name(d,"%s",name));}
static int delete_children(struct device *d,void *p){unsigned *n=p;(*n)++;device_unregister(d);return 0;}
static void device_tests(void){unsigned baseline=host_live_allocations();assert(!bus_register(&testbus));assert(bus_register(&testbus)==-EEXIST);struct notifier_block nb={.notifier_call=notify};assert(!bus_register_notifier(&testbus,&nb));struct device parent,a,b;new_device(&parent,"parent",NULL);assert(!device_add(&parent));new_device(&a,"a",&parent);a.groups=metadata_groups;new_device(&b,"b",&parent);assert(!device_add(&a));assert(!device_add(&b));assert(refcount_read(&parent.refs.refcount)==3);assert(!a.driver);assert(!driver_register(&testdrv));assert(probes==3);assert(a.driver==&testdrv);assert(device_create_file(&a,&dev_attr_value)==-EEXIST);char page[4096];assert(tmc_usbcore_attribute_read(&a,"value",page)==11);assert(!strcmp(page,"test-value\n"));device_remove_file(&a,&dev_attr_value);assert(tmc_usbcore_attribute_read(&a,"value",page)==-ENOENT);
 driver_unregister(&testdrv);assert(removes==3&&managed_releases==3);assert(!a.driver&&!parent.driver);assert(!driver_register(&testdrv));assert(probes==6);unsigned n=0;assert(!device_for_each_child(&parent,&n,delete_children));assert(n==2&&releases==2);assert(refcount_read(&parent.refs.refcount)==1);device_unregister(&parent);assert(releases==3&&removes==6&&managed_releases==6);driver_unregister(&testdrv);assert(notifications==6);assert(!bus_unregister_notifier(&testbus,&nb));bus_unregister(&testbus);assert(host_live_allocations()==baseline);
 /* Failure paths do not register or leak partial state. */
 host_fail_after(0);assert(bus_register(&testbus)==-ENOMEM);host_fail_after(-1);assert(!bus_register(&testbus));new_device(&a,"oom",NULL);assert(!device_add(&a));host_fail_after(0);assert(driver_register(&testdrv)==-ENOMEM);host_fail_after(-1);assert(!a.driver);assert(!driver_register(&testdrv));assert(a.driver);driver_unregister(&testdrv);device_unregister(&a);bus_unregister(&testbus);assert(host_live_allocations()==baseline);
}
static unsigned work_count,timer_count,delayed_count;static struct work_struct work;static struct timer_list timer;static struct delayed_work delayed;
static void run_work(struct work_struct *w){work_count++;if(work_count<5){assert(schedule_work(w));k_progress();assert(work_count<5);}}
static void run_timer(struct timer_list *t){timer_count++;if(timer_count<3)mod_timer(t,jiffies+2);}
static void run_delayed(struct work_struct *w){(void)w;delayed_count++;}
static void free_work(struct work_struct *w){kfree(w);}
static void free_timer(struct timer_list *t){kfree(t);}
static void scheduler_tests(void){unsigned baseline=host_live_allocations();INIT_WORK(&work,run_work);timer_setup(&timer,run_timer,0);INIT_DELAYED_WORK(&delayed,run_delayed);assert(schedule_work(&work));assert(!schedule_work(&work));flush_work(&work);while(work_count<5)tmc_usbcore_dispatch();assert(work_count==5);assert(!cancel_work_sync(&work));mod_timer(&timer,jiffies+2);assert(schedule_delayed_work(&delayed,3));assert(!schedule_delayed_work(&delayed,2));msleep(10);assert(timer_count==3&&delayed_count==1);mod_timer(&timer,jiffies+100);assert(del_timer_sync(&timer));assert(schedule_delayed_work(&delayed,3));assert(cancel_delayed_work_sync(&delayed));msleep(4);assert(delayed_count==1);
 struct work_struct *w=kzalloc(sizeof(*w),GFP_KERNEL);INIT_WORK(w,free_work);schedule_work(w);struct timer_list *t=kzalloc(sizeof(*t),GFP_KERNEL);timer_setup(t,free_timer,0);mod_timer(t,jiffies+1);msleep(3);assert(host_live_allocations()==baseline);struct completion c;init_completion(&c);assert(!wait_for_completion_timeout(&c,2));complete(&c);assert(wait_for_completion_timeout(&c,2));complete_all(&c);assert(wait_for_completion_timeout(&c,0));assert(wait_for_completion_timeout(&c,0));
}
static void original_matching(void){struct usb_device d={0};struct usb_interface i={0};struct usb_host_interface alt={0};i.dev.parent=&d.dev;i.cur_altsetting=&alt;d.descriptor.idVendor=cpu_to_le16(0x1234);d.descriptor.idProduct=cpu_to_le16(0x5678);d.descriptor.bcdDevice=cpu_to_le16(0x102);alt.desc.bInterfaceClass=3;alt.desc.bInterfaceSubClass=1;alt.desc.bInterfaceProtocol=1;static const struct usb_device_id ids[]={ {USB_DEVICE_AND_INTERFACE_INFO(0x1234,0x5678,3,1,1)},{} };assert(usb_match_id(&i,ids)==ids);d.descriptor.idVendor=0;assert(!usb_match_id(&i,ids));d.descriptor.idVendor=0x1234;alt.desc.bInterfaceProtocol=2;assert(!usb_match_id(&i,ids));alt.desc.bInterfaceProtocol=1;static const struct usb_device_id versions[]={{USB_DEVICE_VER(0x1234,0x5678,0x100,0x105)}, {}};assert(usb_match_id(&i,versions)==versions);d.descriptor.bcdDevice=0x106;assert(!usb_match_id(&i,versions));}
static const u8 configuration[]={9,2,34,0,1,1,0,0x80,50,9,4,0,0,1,3,1,1,0,9,0x21,0x11,1,0,1,0x22,63,0,7,5,0x81,3,8,0,10};
static unsigned malformed;
/* Explicit host descriptor transport model. This tests the actual original
 * parser; it is NOT enumeration evidence or production USB service code. */
int usb_get_descriptor(struct usb_device *d,unsigned char type,unsigned char index,void *out,int size){(void)d;if(type!=USB_DT_CONFIG||index)return -EPIPE;size=min_t(int,size,sizeof(configuration));memcpy(out,configuration,size);if(malformed&&size>=9)((u8 *)out)[0]=2;return size;}
static void original_config(void){unsigned baseline=host_live_allocations();struct usb_device d={0};d.speed=USB_SPEED_FULL;d.descriptor.bNumConfigurations=1;assert(!usb_get_configuration(&d));assert(d.config&&d.config[0].desc.bNumInterfaces==1);struct usb_host_interface *a=d.config[0].intf_cache[0]->altsetting;assert(a->desc.bInterfaceClass==3);assert(a->endpoint[0].desc.bEndpointAddress==0x81);assert(usb_endpoint_maxp(&a->endpoint[0].desc)==8);d.actconfig=d.config;struct usb_interface intf={.cur_altsetting=a,.altsetting=a,.num_altsetting=1};d.config->interface[0]=&intf;assert(usb_ifnum_to_if(&d,0)==&intf);assert(!usb_ifnum_to_if(&d,1));assert(usb_altnum_to_altsetting(&intf,0)==a);assert(!usb_altnum_to_altsetting(&intf,1));assert(usb_find_alt_setting(d.config,0,0)==a);d.config->interface[0]=NULL;usb_destroy_configuration(&d);assert(host_live_allocations()==baseline);
 /* Every allocation failure must unwind safely. */
 for(long n=0;n<12;n++){memset(&d,0,sizeof(d));d.speed=USB_SPEED_FULL;d.descriptor.bNumConfigurations=1;host_fail_after(n);int r=usb_get_configuration(&d);host_fail_after(-1);assert(r==0||r<0);usb_destroy_configuration(&d);assert(host_live_allocations()==baseline);}
}
static void original_urb(void){unsigned baseline=host_live_allocations();assert(!usb_alloc_urb(-1,GFP_KERNEL));struct urb *u=usb_alloc_urb(2,GFP_KERNEL);assert(u&&list_empty(&u->urb_list));assert(refcount_read(&u->kref.refcount)==1);assert(usb_get_urb(u)==u);usb_free_urb(u);assert(refcount_read(&u->kref.refcount)==1);usb_free_urb(u);assert(host_live_allocations()==baseline);}
static void original_hub(void){struct usb_device parent={0},child={.parent=&parent,.portnum=1,.state=USB_STATE_CONFIGURED};struct usb_host_config cfg={0};struct usb_interface intf={0};struct usb_hub hub={0};struct usb_port port={0};struct usb_port *ports[]={&port};hub.ports=ports;parent.actconfig=&cfg;parent.maxchild=1;cfg.interface[0]=&intf;usb_set_intfdata(&intf,&hub);assert(usb_hub_to_struct_hub(&parent)==&hub);assert(!usb_device_is_owned(&child));port.port_owner=(void *)&port;assert(usb_device_is_owned(&child));child.state=USB_STATE_NOTATTACHED;assert(!usb_device_is_owned(&child));parent.maxchild=0;assert(!usb_hub_to_struct_hub(&parent));}
static unsigned bh_runs,ordinary_runs;
static struct work_struct bh_work,ordinary_work;
static void run_bh(struct work_struct *w){(void)w;bh_runs++;}
static void run_ordinary(struct work_struct *w){(void)w;ordinary_runs++;}
static void sleeping_worker(struct work_struct *w){(void)w;assert(queue_work(system_bh_wq,&bh_work));assert(schedule_work(&ordinary_work));msleep(2);assert(bh_runs==1&&!ordinary_runs);}
static void bh_isolation_test(void){struct work_struct sleeper;INIT_WORK(&sleeper,sleeping_worker);INIT_WORK(&bh_work,run_bh);INIT_WORK(&ordinary_work,run_ordinary);assert(schedule_work(&sleeper));flush_work(&sleeper);flush_work(&ordinary_work);assert(bh_runs==1&&ordinary_runs==1);}
static unsigned first_queue,second_queue;
static void first_work(struct work_struct *w){first_queue++;if(first_queue==1)assert(schedule_work(w));}
static void second_work(struct work_struct *w){(void)w;second_queue++;}
static void allocation_and_queue_tests(void){unsigned baseline=host_live_allocations();char *p=kzalloc(32,GFP_KERNEL);assert(p&&!((uintptr_t)p%_Alignof(max_align_t)));for(unsigned i=0;i<32;i++)assert(!p[i]);memset(p,42,32);host_fail_after(0);assert(!krealloc(p,100,GFP_KERNEL));host_fail_after(-1);assert(p[31]==42);p=krealloc(p,100,GFP_KERNEL);assert(p&&p[31]==42);kfree(p);assert(!kcalloc(SIZE_MAX,2,GFP_KERNEL));struct workqueue_struct *q=alloc_workqueue("test",0,1);assert(q);struct work_struct a,b;INIT_WORK(&a,first_work);INIT_WORK(&b,second_work);assert(queue_work(q,&a));assert(schedule_work(&b));flush_workqueue(q);assert(first_queue>=1);while(!list_empty(&a.entry))tmc_usbcore_dispatch();flush_work(&b);assert(first_queue==2&&second_queue==1);destroy_workqueue(q);assert(host_live_allocations()==baseline);}
int main(void){device_tests();scheduler_tests();original_matching();original_config();original_urb();original_hub();allocation_and_queue_tests();bh_isolation_test();u8 utf[]={0x41,0,0xe8,0,0x3d,0xd8,0,0xde},out[16]={0};assert(utf16s_to_utf8s(utf,4,0,out,sizeof(out))==7);assert(!memcmp(out,"A\xc3\xa8\xf0\x9f\x98\x80",7));assert(!host_live_allocations());puts("PASS canonical USB ABI: device/driver lifecycle, managed rollback, references, deletion-safe iteration, work/timer ownership, original Linux matching/config/URB");return 0;}
