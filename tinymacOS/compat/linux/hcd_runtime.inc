/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Included in usb_input.c after the real native enqueue/dequeue/copy path.
 * Unmodified complete hcd.c functions replace the previous hand-written core
 * refcount/unlink/giveback logic. IRQ-safe device-driver completion is direct;
 * Linux root hubs, physical hc_driver implementations and DMA remain unported.
 */
static spinlock_t hcd_urb_list_lock,hcd_urb_unlink_lock;
void tmc_lx_hcd_queue_lock(spinlock_t *s)
{
 while(__atomic_exchange_n(&s->word,1,__ATOMIC_ACQUIRE))tmc_lx_os_spin_wait();
}
void tmc_lx_hcd_queue_unlock(spinlock_t *s){__atomic_store_n(&s->word,0,__ATOMIC_RELEASE);}
#include "../../vendor/linux-v6.12/hcd-endpoint.inc"
/* No registered Linux root hub exists on this native transport. Reject an
 * explicit root request; do not emulate successful root enumeration. */
static int rh_urb_enqueue(struct usb_hcd *h,struct urb *u)
{(void)h;(void)u;HCOUNT(root_rejected);return -EOPNOTSUPP;}
static int usb_rh_urb_dequeue(struct usb_hcd *h,struct urb *u,int status)
{(void)h;(void)u;(void)status;HCOUNT(root_rejected);return -EOPNOTSUPP;}
static struct usb_device *usb_get_dev(struct usb_device *d)
{if(d)atomic_inc(&d->references);return d;}
static void usb_put_dev(struct usb_device *d)
{if(d)atomic_dec(&d->references);}
/* Payloads here are copied into controller-owned DMA by the actual native
 * HCDs. Thus there is deliberately no dma_map_single illusion. Validate the
 * declared transport before calling its queue; no new DMA flags are set. */
static int map_urb_for_dma(struct usb_hcd *h,struct urb *u,gfp_t flags)
{
 HCOUNT(map_checks);int result=tmc_native_hcd_validate(h,u,flags);
 if(result){HCOUNT(map_rejected);ustats.rejected++;}return result;
}
static void unmap_urb_for_dma(struct usb_hcd *h,struct urb *u)
{
 /* The copying backend holds no logical-URB DMA mapping or bounce buffer.
  * Ownership of native hardware DMA/rings is never transferred to this ABI. */
 (void)h;(void)u;
}
/* Optional Linux usbmon, LED trigger and KCOV instrumentation are disabled;
 * these macros replace no queue, mapping, hardware or lifecycle operation. */
#define usbmon_urb_submit(bus,u) do {} while(0)
#define usbmon_urb_submit_error(bus,u,status) do {} while(0)
#define usbmon_urb_complete(bus,u,status) do {} while(0)
#define USB_LED_EVENT_HOST 0
#define usb_led_activity(event) do {} while(0)
#define kcov_remote_start_usb_softirq(id) 0UL
#define kcov_remote_stop_softirq(flags) do {(void)(flags);} while(0)
/* hcd.c's original direct giveback function is used; no fake workqueue/BH. */
#include "../../vendor/linux-v6.12/hcd-lifecycle.inc"
#undef usbmon_urb_submit
#undef usbmon_urb_submit_error
#undef usbmon_urb_complete
#undef usb_led_activity
#undef kcov_remote_start_usb_softirq
#undef kcov_remote_stop_softirq
