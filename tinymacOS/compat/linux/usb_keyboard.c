/* SPDX-License-Identifier: GPL-2.0-or-later
 * Compile the COMPLETE byte-exact upstream driver, including probe, IRQ,
 * LED event/completion, allocation rollback and disconnect. The accessor
 * below exposes its physical key map to the input compatibility adapter;
 * it is not a replacement decoder or a Unicode/US-layout conversion.
 */
#include "../../vendor/linux-v6.12/usbkbd.c"
unsigned tmc_lx_usbkbd_usage(unsigned code)
{
    if(!code)return 0;
    for(unsigned usage=4;usage<256;usage++)
        if(usb_kbd_keycode[usage]==code)return usage;
    return 0;
}
