#include <tmc_kernel.h>
#include <linux/usb.h>
#undef module_usb_driver
#define module_usb_driver(d) int tmc_usb_bootkbd_init(void){return usb_register_driver(&(d),NULL,"usbkbd");}
#include "../../vendor/linux-v6.12/usbkbd.c"
unsigned tmc_usb_usage(unsigned code){if(!code)return 0;for(unsigned n=4;n<256;n++)if(usb_kbd_keycode[n]==code)return n;return 0;}
