/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* Original Linux hcd.c endpoint/submission/unlink/direct-giveback functions.
 * This ABI exposes a COPYING native input transport, NOT a physical Linux
 * HCD/root hub, DMA mapping API, or a complete port of hcd.c. */
#define EHOSTUNREACH 113
struct usb_hcd;
struct hc_driver {
 int (*urb_enqueue)(struct usb_hcd *,struct urb *,gfp_t);
 int (*urb_dequeue)(struct usb_hcd *,struct urb *,int);
};
struct usb_hcd {
 struct usb_bus self;const struct hc_driver *driver;
 int running; /* set only for the configured native input transport */
};
static inline struct usb_hcd *bus_to_hcd(struct usb_bus *bus){return container_of(bus,struct usb_hcd,self);}
#define HCD_RH_RUNNING(h) ((h)->running)
/* Regular native interfaces never masquerade as virtual Linux root hubs. */
static inline int is_root_hub(struct usb_device *d){return d&&d->bus&&d->bus->root_hub==d;}
static inline void list_del_init(struct list_head *p){list_del(p);INIT_LIST_HEAD(p);}
#define list_for_each(p,h) for((p)=(h)->next;(p)!=(h);(p)=(p)->next)
int usb_hcd_link_urb_to_ep(struct usb_hcd *,struct urb *);
int usb_hcd_check_unlink_urb(struct usb_hcd *,struct urb *,int);
void usb_hcd_unlink_urb_from_ep(struct usb_hcd *,struct urb *);
void tmc_lx_hcd_queue_lock(spinlock_t *);
void tmc_lx_hcd_queue_unlock(spinlock_t *);
/* Called only under the outer native IRQ-disabled transport lock. These
 * nested spinlocks must preserve, not enable, the existing IRQ state. */
#define spin_lock(l) tmc_lx_hcd_queue_lock(l)
#define spin_unlock(l) tmc_lx_hcd_queue_unlock(l)
