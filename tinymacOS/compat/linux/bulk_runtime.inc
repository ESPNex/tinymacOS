/* SPDX-License-Identifier: GPL-2.0-only
 * Additive bulk adapter for the ORIGINAL Linux HCD/URB/DMA path. Included in
 * each isolated private ABI; no ring/qTD implementation or HID fast path here.
 * Endpoint token matches existing transport ABI: 1..15 IN, 0x100|1..15 OUT.
 */
#ifdef TMC_BULK_XHCI
#define BULK_DEVICE TmcXhciDevice
#define BULK_COMPLETE TmcXhciComplete
#define BULK_API(n) tmc_linux_xhci_bulk_##n
#else
#define BULK_DEVICE TmcEhciDevice
#define BULK_COMPLETE TmcEhciComplete
#define BULK_API(n) tmc_linux_ehci_bulk_##n
#endif
struct BulkRequest {
 struct list_head entry;BULK_DEVICE *device;unsigned token,depth;
 struct urb *urb;BULK_COMPLETE complete;void *context;
};
static struct list_head bulk_requests={&bulk_requests,&bulk_requests};
static struct BulkRequest *bulk_find(BULK_DEVICE *d,unsigned ep)
{
 struct BulkRequest *r;list_for_each_entry(r,&bulk_requests,entry)if(r->device==d&&r->token==ep)return r;return NULL;
}
static void bulk_complete(struct urb *u)
{
 struct BulkRequest *r=u->context;r->depth++;
 r->complete(r->context,u->status,u->actual_length);r->depth--;
}
int BULK_API(submit)(BULK_DEVICE *d,unsigned ep,unsigned packet,unsigned burst,unsigned length,void *data,BULK_COMPLETE complete,void *context)
{
 if(!d||!(ep&15)||(ep&~0x10f)||length>65536||(length&&!data)||!complete||burst>15)return -EINVAL;
 unsigned speed=d->core.speed;
 if(speed==USB_SPEED_LOW||!packet||packet>(speed>=USB_SPEED_SUPER?1024:speed==USB_SPEED_HIGH?512:64)||(packet&(packet-1))||(speed<USB_SPEED_SUPER&&burst))return -EINVAL;
 struct usb_host_endpoint *e=&d->eps[(ep&15)-1+((ep&0x100)?15:0)];
 if(e->enabled&&((e->desc.bmAttributes&3)!=2||e->desc.wMaxPacketSize!=packet||
 (speed>=USB_SPEED_SUPER&&e->ss_ep_comp.bMaxBurst!=burst)))return -EINVAL;
 struct BulkRequest *r=bulk_find(d,ep);int fresh=!r;
 if(r&&!list_empty(&e->urb_list))return -EBUSY;
 if(!r){
  r=kzalloc(sizeof(*r),GFP_ATOMIC);if(!r)return -ENOMEM;
  r->urb=usb_alloc_urb(0,GFP_ATOMIC);if(!r->urb){kfree(r);return -ENOMEM;}
  r->device=d;r->token=ep;INIT_LIST_HEAD(&r->entry);
 }
 if(!e->enabled){
  e->enabled=1;e->desc=(struct usb_endpoint_descriptor){.bLength=7,.bDescriptorType=5,.bEndpointAddress=(ep&15)|((ep&0x100)?0:128),.bmAttributes=2,.wMaxPacketSize=packet};
  INIT_LIST_HEAD(&e->urb_list);(ep&0x100?d->core.ep_out:d->core.ep_in)[ep&15]=e;
#ifdef TMC_BULK_XHCI
  if(speed>=USB_SPEED_SUPER)e->ss_ep_comp=(struct usb_ss_ep_comp_descriptor){.bLength=6,.bDescriptorType=USB_DT_SS_ENDPOINT_COMP,.bMaxBurst=burst};
  struct usb_hcd *h=bus_to_hcd(d->core.bus);
  int rc=h->driver->add_endpoint(h,&d->core,e);if(!rc)rc=h->driver->check_bandwidth(h,&d->core);
  if(rc){h->driver->reset_bandwidth(h,&d->core);e->enabled=0;e->hcpriv=NULL;(ep&0x100?d->core.ep_out:d->core.ep_in)[ep&15]=NULL;if(fresh){usb_free_urb(r->urb);kfree(r);}return rc;}
#endif
 }
 if(fresh)list_add_tail(&r->entry,&bulk_requests);
 r->complete=complete;r->context=context;struct urb *u=r->urb;
 u->dev=&d->core;u->pipe=(PIPE_BULK<<30)|(d->core.devnum<<8)|((ep&15)<<15)|((ep&0x100)?0:128);
 u->transfer_buffer=data;u->transfer_buffer_length=length;u->complete=bulk_complete;u->context=r;u->interval=0;
 return usb_submit_urb(u,GFP_ATOMIC);
}
int BULK_API(stop)(BULK_DEVICE *d,unsigned ep)
{
 if(!d||!(ep&15)||(ep&~0x10f))return -EINVAL;
 struct BulkRequest *r=bulk_find(d,ep);if(!r)return 0;if(r->depth)return -EBUSY;
 if(atomic_read(&r->urb->use_count))UCOUNT(unlinks);
 usb_kill_urb(r->urb);usb_free_urb(r->urb);list_del(&r->entry);kfree(r);return 0;
}
static int bulk_release(BULK_DEVICE *d)
{
 struct BulkRequest *r,*next;
 list_for_each_entry(r,&bulk_requests,entry)if(r->device==d&&(r->depth||atomic_read(&r->urb->use_count)))return -EBUSY;
 list_for_each_entry_safe(r,next,&bulk_requests,entry)if(r->device==d){int rc=BULK_API(stop)(d,r->token);if(rc)return rc;}
 return 0;
}
#undef BULK_DEVICE
#undef BULK_COMPLETE
#undef BULK_API
#ifdef TMC_BULK_XHCI
#undef TMC_BULK_XHCI
#endif
