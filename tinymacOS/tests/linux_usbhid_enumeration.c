/* Real original core/HCD/hub lifecycle on an explicit, asynchronous USB
 * controller MODEL. This is not hardware or production transport evidence. */
#include <tmc_kernel.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/ch11.h>
#include <usb.h>
#include <assert.h>
#include <stdio.h>
static struct usb_hcd *controller;
static struct urb *interrupt_urb;
static unsigned port_status=USB_PORT_STAT_POWER|USB_PORT_STAT_CONNECTION|USB_PORT_STAT_HIGH_SPEED;
static unsigned port_change=USB_PORT_STAT_C_CONNECTION;
static u64 reset_end;
static unsigned controls,packets,attached,detached,configured,submitted;
static int inject;
static const u8 device_descriptor[]={18,1,0,2,0,0,0,64,0x34,0x12,0x78,0x56,0,1,0,0,0,1};
static const u8 hid_report[]={5,1,9,6,0xa1,1,5,7,0x19,0xe0,0x29,0xe7,0x15,0,0x25,1,0x75,1,0x95,8,0x81,2,0x95,1,0x75,8,0x81,1,0x95,5,0x75,1,5,8,0x19,1,0x29,5,0x91,2,0x95,1,0x75,3,0x91,1,0x95,6,0x75,8,0x15,0,0x25,0x65,5,7,0x19,0,0x29,0x65,0x81,0,0xc0};
static const u8 config_descriptor[]={9,2,34,0,1,1,0,0x80,50,9,4,0,0,1,3,1,1,0,9,0x21,0x11,1,0,1,0x22,sizeof(hid_report),0,7,5,0x81,3,8,0,10};
static void giveback(struct usb_hcd *h,struct urb *u,int status,unsigned actual){usb_hcd_unlink_urb_from_ep(h,u);u->actual_length=actual;usb_hcd_giveback_urb(h,u,status);}
static int enqueue(struct usb_hcd *h,struct urb *u,gfp_t f){(void)f;int r=usb_hcd_link_urb_to_ep(h,u);if(r)return r;submitted++;
 if(usb_pipeint(u->pipe)){assert(!interrupt_urb);interrupt_urb=u;return 0;}
 assert(usb_pipecontrol(u->pipe));controls++;struct usb_ctrlrequest *s=(void *)u->setup_packet;int status=0;unsigned n=0;
 if(s->bRequest==USB_REQ_GET_DESCRIPTOR){const u8 *src=NULL;unsigned bytes=0;if((le16_to_cpu(s->wValue)>>8)==USB_DT_DEVICE){src=device_descriptor;bytes=sizeof(device_descriptor);}else if((le16_to_cpu(s->wValue)>>8)==USB_DT_CONFIG){src=config_descriptor;bytes=sizeof(config_descriptor);}else if((le16_to_cpu(s->wValue)>>8)==0x22){src=hid_report;bytes=sizeof(hid_report);}else status=-EPIPE;if(src){n=min_t(unsigned,bytes,u->transfer_buffer_length);memcpy(u->transfer_buffer,src,n);}}
 else if(s->bRequestType==0&&s->bRequest==USB_REQ_SET_CONFIGURATION){assert(le16_to_cpu(s->wValue)<=1);configured=le16_to_cpu(s->wValue);}
 else if(s->bRequest==USB_REQ_GET_STATUS){memset(u->transfer_buffer,0,min_t(int,2,u->transfer_buffer_length));n=min_t(int,2,u->transfer_buffer_length);}
 else if(s->bRequestType==0x21&&(s->bRequest==9||s->bRequest==10)){n=u->transfer_buffer_length;}
 else if(s->bRequest!=USB_REQ_SET_ADDRESS&&s->bRequest!=USB_REQ_SET_INTERFACE)status=-EPIPE;
 giveback(h,u,status,n);return 0;
}
static int dequeue(struct usb_hcd *h,struct urb *u,int status){int r=usb_hcd_check_unlink_urb(h,u,status);if(r)return r;if(u==interrupt_urb)interrupt_urb=NULL;giveback(h,u,status,0);return 0;}
static int root_control(struct usb_hcd *h,u16 request,u16 value,u16 index,char *buf,u16 len){(void)h;
 if(request==GetHubDescriptor){assert(len>=9);u8 hub[]={9,USB_DT_HUB,1,0x09,0,10,0,0,0xff};memcpy(buf,hub,min_t(unsigned,len,sizeof(hub)));return 0;}
 if(request==GetHubStatus){memset(buf,0,min_t(unsigned,len,4));return 0;}
 if(index!=1)return -EPIPE;
 if(reset_end&&k_clock_ms()>=reset_end){reset_end=0;port_status&=~USB_PORT_STAT_RESET;port_status|=USB_PORT_STAT_ENABLE;port_change|=USB_PORT_STAT_C_RESET;}
 if(request==GetPortStatus){u8 p[]={port_status,port_status>>8,port_change,port_change>>8};memcpy(buf,p,min_t(unsigned,len,4));return 0;}
 if(request==SetPortFeature){if(value==USB_PORT_FEAT_RESET){port_status|=USB_PORT_STAT_RESET;reset_end=k_clock_ms()+20;}else if(value==USB_PORT_FEAT_POWER)port_status|=USB_PORT_STAT_POWER;else return -EPIPE;return 0;}
 if(request==ClearPortFeature){if(value>=16&&value<=20)port_change&=~(1u<<(value-16));else if(value==USB_PORT_FEAT_ENABLE)port_status&=~USB_PORT_STAT_ENABLE;else if(value==USB_PORT_FEAT_POWER)port_status&=~USB_PORT_STAT_POWER;else return -EPIPE;return 0;}return -EPIPE;
}
static int root_changes(struct usb_hcd *h,char *data){(void)h;if(port_change){data[0]=2;return 1;}return 0;}
static irqreturn_t irq(struct usb_hcd *h){if(interrupt_urb&&inject){struct urb *u=interrupt_urb;interrupt_urb=NULL;inject=0;const u8 packet[]={packets==0?2:0,0,packets==0?4:packets==2?5:0,0,0,0,0,0};packets++;memcpy(u->transfer_buffer,packet,min_t(unsigned,sizeof(packet),u->transfer_buffer_length));giveback(h,u,0,min_t(unsigned,sizeof(packet),u->transfer_buffer_length));return IRQ_HANDLED;}return IRQ_NONE;}
static int start(struct usb_hcd *h){h->uses_new_polling=1;set_bit(HCD_FLAG_POLL_RH,&h->flags);return 0;}
static void stop(struct usb_hcd *h){(void)h;assert(!interrupt_urb);}
static int alloc(struct usb_hcd *h,struct usb_device *d){(void)h;(void)d;return 1;}
static void free_dev(struct usb_hcd *h,struct usb_device *d){(void)h;(void)d;}
static void disable(struct usb_hcd *h,struct usb_host_endpoint *e){(void)h;assert(list_empty(&e->urb_list));}
static const struct hc_driver driver={.description="explicit-usb-model",.product_desc="USB controller integration MODEL",.flags=HCD_MEMORY|HCD_USB2|HCD_BH,.start=start,.stop=stop,.irq=irq,.urb_enqueue=enqueue,.urb_dequeue=dequeue,.hub_control=root_control,.hub_status_data=root_changes,.alloc_dev=alloc,.free_dev=free_dev,.endpoint_disable=disable};
void tmc_usbcore_os_poll(void){if(controller)usb_hcd_irq(0,controller);}
void tmc_usbcore_os_entropy(const void *p,unsigned n){(void)p;(void)n;}
extern int tmc_canonical_hid_init(void),tmc_canonical_usbhid_init(void),tmc_generic_init(void);
extern void tmc_canonical_hid_exit(void),tmc_canonical_usbhid_exit(void),tmc_generic_exit(void),tmc_usb_input_leds(unsigned);
static unsigned bound,events;static u8 last_keys[32];static unsigned last_mods;
void tmc_usbcore_os_keys(const u8 k[32],unsigned mods){memcpy(last_keys,k,32);last_mods=mods;events++;}
void tmc_usbcore_os_absolute(int x,int y,unsigned b,int w,int hw){assert(0);}
void tmc_usbcore_os_pointer(int x,int y,unsigned b,int w,int hw){assert(0);}
void tmc_usbcore_os_input_registered(unsigned kind,int add){assert(kind==1);if(add)bound++;else{assert(bound);bound--;}}
static struct device_driver controller_identity={.name="model-controller"};
static void release_controller(struct device *d){(void)d;}
extern unsigned host_live_allocations(void);
int main(void){unsigned base=host_live_allocations();struct device dev={0};device_initialize(&dev);dev.release=release_controller;dev.driver=&controller_identity;assert(!dev_set_name(&dev,"model-controller"));assert(!device_add(&dev));usb_init_pool_max();assert(!bus_register(&usb_bus_type));assert(!usb_hub_init());assert(!usb_register_device_driver(&usb_generic_driver,NULL));assert(!tmc_canonical_hid_init());assert(!tmc_generic_init());assert(!tmc_canonical_usbhid_init());controller=usb_create_hcd(&driver,&dev,"model-controller");assert(controller);controller->skip_phy_initialization=1;controller->power_budget=500;assert(!usb_add_hcd(controller,0,0));u64 deadline=k_clock_ms()+5000;while(!bound&&k_clock_ms()<deadline)k_progress();assert(bound==1&&configured==1&&interrupt_urb);for(unsigned n=0;n<3;n++){inject=1;k_progress();if(n==0)assert((last_keys[0]&(1u<<4))&&last_mods==2);if(n==1)assert(!last_keys[0]&&!last_mods);if(n==2)assert((last_keys[0]&(1u<<5))&&!last_mods);}assert(packets==3&&interrupt_urb&&events>=3);tmc_usb_input_leds(2);for(unsigned n=0;n<10;n++)k_progress();port_status&=~USB_PORT_STAT_CONNECTION;port_change|=USB_PORT_STAT_C_CONNECTION;deadline=k_clock_ms()+1000;while(bound&&k_clock_ms()<deadline)k_progress();assert(!bound&&!last_keys[0]&&!last_mods);usb_remove_hcd(controller);usb_put_hcd(controller);controller=NULL;tmc_canonical_usbhid_exit();tmc_generic_exit();tmc_canonical_hid_exit();usb_deregister_device_driver(&usb_generic_driver);usb_hub_cleanup();bus_unregister(&usb_bus_type);device_unregister(&dev);assert(host_live_allocations()==base);printf("PASS original USB core/HCD/hub/HID-core/usbhid/hid-input/generic MODEL, %u real URB reports, hot-unplug; controls=%u submitted=%u\n",packets,controls,submitted);return 0;}
