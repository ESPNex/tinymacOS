/* SPDX-License-Identifier: MIT */
#pragma once
#include "usbhost.h"
/* Native adapter recovery, shared by xHCI/EHCI/UHCI. A burst of transport or
 * allocation errors is not a disconnect. One retry per due poll, capped delay;
 * never spin/reset a live controller or remux ports with a working keyboard.
 * Genuine cancellation/removal/invalid endpoint is terminal. */
static inline int usb_input_retry(UsbInputPipe *p,int status,u64 now)
{
    if(status>=0||status==-2||status==-19||status==-22||status==-104||status==-108){p->armed=0;return 0;}
    if(p->failures<10)p->failures++;
    if(status==-32)p->halted=1;
    unsigned shift=p->failures>4?3:p->failures-1;
    p->retry_at=now+(100ull<<shift);p->armed=-1;
    return 1;
}
