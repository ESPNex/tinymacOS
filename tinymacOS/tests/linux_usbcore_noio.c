/* NO-I/O test tripwires. These are NOT production services. Any unexpected
 * transfer, notification or endpoint operation aborts rather than succeeding.
 * Data-only metadata symbols exist to link the uncalled whole upstream code.
 */
#include <tmc_kernel.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <usb.h>
#include <phy.h>
#include <assert.h>
#include <stdio.h>
static void unexpected(const char *what) __attribute__((noreturn));
static void unexpected(const char *what){fprintf(stderr,"UNEXPECTED no-I/O USB binding dependency: %s\n",what);k_fatal();}
#define UNEXPECTED() unexpected(__func__)
const struct attribute_group *usb_interface_groups[]={NULL};
void usb_create_sysfs_intf_files(struct usb_interface *i){(void)i;UNEXPECTED();}
void usb_remove_sysfs_intf_files(struct usb_interface *i){(void)i;UNEXPECTED();}
int usb_update_wireless_status_attr(struct usb_interface *i){(void)i;UNEXPECTED();}
void sysfs_notify_dirent(struct kernfs_node *n){(void)n;UNEXPECTED();}
unsigned int usb_decode_interval(const struct usb_endpoint_descriptor *d,enum usb_device_speed s){(void)d;(void)s;UNEXPECTED();}
const char *usb_speed_string(enum usb_device_speed s){(void)s;UNEXPECTED();}
int usb_free_streams(struct usb_interface *i,struct usb_host_endpoint **e,unsigned n,gfp_t f){(void)i;(void)e;(void)n;(void)f;UNEXPECTED();}
int usb_hcd_submit_urb(struct urb *u,gfp_t f){(void)u;(void)f;UNEXPECTED();}
int usb_hcd_unlink_urb(struct urb *u,int s){(void)u;(void)s;UNEXPECTED();}
void usb_hcd_flush_endpoint(struct usb_device *d,struct usb_host_endpoint *e){(void)d;(void)e;UNEXPECTED();}
void usb_hcd_disable_endpoint(struct usb_device *d,struct usb_host_endpoint *e){(void)d;(void)e;UNEXPECTED();}
void usb_hcd_reset_endpoint(struct usb_device *d,struct usb_host_endpoint *e){(void)d;(void)e;UNEXPECTED();}
int usb_hcd_alloc_bandwidth(struct usb_device *d,struct usb_host_config *c,struct usb_host_interface *a,struct usb_host_interface *b){(void)d;(void)c;(void)a;(void)b;UNEXPECTED();}
void usb_put_hcd(struct usb_hcd *h){(void)h;UNEXPECTED();}
void usb_notify_add_device(struct usb_device *d){(void)d;UNEXPECTED();}
void usb_notify_remove_device(struct usb_device *d){(void)d;UNEXPECTED();}
int usb_phy_roothub_notify_connect(struct usb_phy_roothub *p,int port){(void)p;(void)port;UNEXPECTED();}
int usb_phy_roothub_notify_disconnect(struct usb_phy_roothub *p,int port){(void)p;(void)port;UNEXPECTED();}
