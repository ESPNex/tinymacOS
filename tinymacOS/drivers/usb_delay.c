#include "../kernel/k.h"
/* Shared PIT delay; no enumeration or HID class binding. */
void usb_delay_ms(unsigned ms)
{
    while(ms) {
        unsigned chunk=MIN(ms,10u),count=1193*chunk;ms-=chunk;
        u8 old=inb(0x61);outb(0x61,old&~3u);outb(0x43,0xb0);
        outb(0x42,(u8)count);outb(0x42,(u8)(count>>8));outb(0x61,(old&~2u)|1);
        for(unsigned i=0;i<3000000 && !(inb(0x61)&32);i++)asm volatile("pause");
        outb(0x61,old);
    }
}
