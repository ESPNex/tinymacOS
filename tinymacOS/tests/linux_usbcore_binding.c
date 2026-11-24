/* SPDX-License-Identifier: GPL-2.0-only */
/* Original USB driver binding on canonical objects. NO USB I/O: endpoints
 * are empty; tests/linux_usbcore_noio.c aborts on every hardware entry. */
#include <tmc_kernel.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <usb.h>
#include <assert.h>
#include <stdio.h>
extern unsigned host_live_allocations(void);
extern void host_fail_after(long);
static unsigned probed,disconnected,released,managed_released;
static int reject_probe;
static void managed_release(void *p){managed_released++;kfree(p);}
static int probe(struct usb_interface *i,const struct usb_device_id *id)
{
 assert(id&&i->condition==USB_INTERFACE_BINDING);probed++;
 void *p=kzalloc(16,GFP_KERNEL);if(!p)return -ENOMEM;
 if(devm_add_action_or_reset(&i->dev,managed_release,p))return -ENOMEM;
 usb_set_intfdata(i,p);return reject_probe?-EIO:0;
}
static void disconnect(struct usb_interface *i)
{assert(i->condition==USB_INTERFACE_UNBINDING);disconnected++;}
static const struct usb_device_id ids[]={{USB_DEVICE(0x1234,0x5678)}, {}};
static struct usb_driver driver={.name="canonical-host",.probe=probe,.disconnect=disconnect,.id_table=ids,.soft_unbind=1};
static void release(struct device *d){(void)d;released++;}
static void setup(struct usb_device *d,struct usb_interface *i,struct usb_host_interface *a)
{
 memset(d,0,sizeof(*d));memset(i,0,sizeof(*i));memset(a,0,sizeof(*a));
 device_initialize(&d->dev);d->dev.type=&usb_device_type;d->dev.bus=&usb_bus_type;d->dev.release=release;
 d->state=USB_STATE_CONFIGURED;d->authorized=1;d->descriptor.idVendor=0x1234;d->descriptor.idProduct=0x5678;
 assert(!dev_set_name(&d->dev,"host-device"));assert(!device_add(&d->dev));
 device_initialize(&i->dev);i->dev.type=&usb_if_device_type;i->dev.bus=&usb_bus_type;i->dev.parent=&d->dev;i->dev.release=release;
 i->cur_altsetting=i->altsetting=a;i->num_altsetting=1;i->authorized=1;
 assert(!dev_set_name(&i->dev,"host-interface"));assert(!device_add(&i->dev));
}
static void cleanup(struct usb_device *d,struct usb_interface *i)
{device_unregister(&i->dev);device_unregister(&d->dev);}
int main(void)
{
 assert(!host_live_allocations());assert(!bus_register(&usb_bus_type));
 unsigned baseline=host_live_allocations();
 struct usb_device d;struct usb_interface i;struct usb_host_interface alt;
 setup(&d,&i,&alt);i.authorized=0;
 assert(!usb_register_driver(&driver,NULL,"host-test"));assert(!probed&&!i.dev.driver);
 i.authorized=1;assert(device_attach(&i.dev)==1);
 assert(probed==1&&i.condition==USB_INTERFACE_BOUND&&usb_get_intfdata(&i));
 device_release_driver(&i.dev);assert(disconnected==1&&i.condition==USB_INTERFACE_UNBOUND&&!usb_get_intfdata(&i));assert(managed_released==1);
 reject_probe=1;assert(device_attach(&i.dev)==0);assert(probed==2&&!i.dev.driver&&!usb_get_intfdata(&i));assert(i.condition==USB_INTERFACE_UNBOUND&&managed_released==2);
 reject_probe=0;d.authorized=0;assert(device_attach(&i.dev)==0);assert(probed==2);d.authorized=1;
 /* The ORIGINAL dynamic-ID attributes use the actual service registry and
  * original usb_store_new_id -> driver_attach -> usb_probe_interface path. */
 d.descriptor.idVendor=0x4321;d.descriptor.idProduct=0x8765;
 assert(device_attach(&i.dev)==0);char page[4096];
 assert(tmc_usbcore_attribute_write(&driver.driver,"new_id","broken",6)==-EINVAL);
 host_fail_after(0);assert(tmc_usbcore_attribute_write(&driver.driver,"new_id","4321 8765",9)==-ENOMEM);host_fail_after(-1);
 assert(tmc_usbcore_attribute_write(&driver.driver,"new_id","4321 8765",9)==9);
 assert(probed==3&&i.dev.driver==&driver.driver);
 assert(tmc_usbcore_attribute_read(&driver.driver,"new_id",page)==10);assert(!strcmp(page,"4321 8765\n"));
 assert(tmc_usbcore_attribute_write(&driver.driver,"remove_id","4321 8765",9)==9);
 device_release_driver(&i.dev);assert(device_attach(&i.dev)==0);
 assert(!tmc_usbcore_attribute_read(&driver.driver,"new_id",page));
 usb_deregister(&driver);assert(!i.dev.driver&&list_empty(&driver.dynids.list));
 assert(tmc_usbcore_attribute_read(&driver.driver,"new_id",page)==-ENOENT);
 cleanup(&d,&i);assert(host_live_allocations()==baseline);
 /* All registration allocation failures unwind references, dynamic files,
  * original interface state and managed probe data without leaking. */
 for(long n=0;n<14;n++){
  setup(&d,&i,&alt);host_fail_after(n);int r=usb_register_driver(&driver,NULL,"host-test");host_fail_after(-1);
  if(!r)usb_deregister(&driver);else assert(r<0);
  assert(!i.dev.driver&&i.condition==USB_INTERFACE_UNBOUND);
  cleanup(&d,&i);assert(host_live_allocations()==baseline);
 }
 bus_unregister(&usb_bus_type);assert(!host_live_allocations());
 printf("PASS original USB binding NO-I/O: probes=%u disconnects=%u releases=%u managed=%u; authorization, failed-probe rollback, dynamic IDs, registration OOM\n",probed,disconnected,released,managed_released);
 return 0;
}
