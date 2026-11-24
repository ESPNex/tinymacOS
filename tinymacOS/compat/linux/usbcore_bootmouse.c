#include <tmc_kernel.h>
#include <linux/usb.h>
#undef module_usb_driver
#define module_usb_driver(d) int tmc_usb_bootmouse_init(void){return usb_register_driver(&(d),NULL,"usbmouse");}
#include "../../vendor/linux-v6.12/usbmouse.c"
