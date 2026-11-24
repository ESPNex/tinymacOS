/* SPDX-License-Identifier: GPL-2.0-only */
#include <tmc_kernel.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <usb.h>
extern int tmc_usb_physical_init(void),tmc_canonical_hid_init(void),tmc_canonical_usbhid_init(void),tmc_generic_init(void);
extern void tmc_usb_physical_poll(void),tmc_usb_input_leds(unsigned);
int tmc_usb_stack_init(void){usb_init_pool_max();int r=bus_register(&usb_bus_type);if(r)return r;r=usb_hub_init();if(r)return r;r=usb_register_device_driver(&usb_generic_driver,NULL);if(r)return r;r=tmc_canonical_hid_init();if(r)return r;r=tmc_generic_init();if(r)return r;r=tmc_canonical_usbhid_init();if(r)return r;return tmc_usb_physical_init();}
void tmc_usb_stack_poll(void){tmc_usb_physical_poll();tmc_usbcore_dispatch();}
void tmc_usb_stack_leds(unsigned leds){tmc_usb_input_leds(leds);}
