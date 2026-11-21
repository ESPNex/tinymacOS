#include "../kernel/k.h"
#include "usbhost.h"
static int select_protocol(UsbInput *u,unsigned iface,unsigned expected)
{
    int set=!u->control(u,0x21,11,expected,iface,0);
    u->desc[0]=255;
    int get=!u->control(u,0xa1,3,0,iface,1);
    return get ? u->ctrl_bytes==1 && u->desc[0]==expected : set;
}
/* PIT channel 2, independent of channel-0 scheduler tick and pre-IRQ startup.
 * Bounded fallback on machines without a functioning legacy timer. */
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
int usb_input_enumerate(UsbInput *u,unsigned address)
{
    int (*c)(UsbInput *,int,int,int,int,int)=u->control;
    u->max_packet0=u->speed==3?64:8;
    if(c(u,128,6,0x100,0,8)||u->ctrl_bytes<8)return 0;
    unsigned m=u->desc[7];if(m!=8&&m!=16&&m!=32&&m!=64)return 0;
    if(u->speed==2&&m!=8)return 0;u->max_packet0=m;
    if(!address||address>127||c(u,0,5,address,0,0))return 0;
    usb_delay_ms(3);u->address=address;
    if(c(u,128,6,0x100,0,18)||u->ctrl_bytes<18)return 0;
    u->vid=u->desc[8]|u->desc[9]<<8;u->pid=u->desc[10]|u->desc[11]<<8;
    if(c(u,128,6,0x200,0,9)||u->ctrl_bytes<9)return 0;
    unsigned n=u->desc[2]|u->desc[3]<<8;
    if(n<9||n>sizeof u->desc||c(u,128,6,0x200,0,n)||u->ctrl_bytes<n)return 0;
    unsigned config=u->desc[5];int hub=0;
    for(unsigned at=0;at+2<=n;) {
        unsigned z=u->desc[at];if(z<2||z>n-at)return 0;
        if(u->desc[at+1]==4&&z>=9&&u->desc[at+5]==9)hub=1;
        at+=z;
    }
    if(hub)return c(u,0,9,config,0,0)?0:2;
    HidInterface choices[USB_INPUT_INTERFACES];
    int found=hid_interfaces(u->desc,n,choices,USB_INPUT_INTERFACES);
    if(found<=0||c(u,0,9,config,0,0))return 0;
    unsigned usable=0;
    for(int i=0;i<found;i++) {
        HidInterface *f=choices+i;UsbInputPipe *p=u->pipe+u->npipe;
        *p=(UsbInputPipe){.proto=f->proto,.ep=f->ep>>1,.packet=f->packet,.interval=f->interval,.iface=f->iface,.report_length=f->report_length};
        if((u->speed==2&&p->packet>8)||(u->speed==1&&p->packet>64))continue;
        int report_ready=0;
        /* Boot support is a compatibility promise, not an instruction to
         * switch a working mouse into descriptor Report mode. */
        if(p->proto==2){
            int boot_ok=select_protocol(u,p->iface,0);
            int parsed=p->report_length && p->report_length<=sizeof u->desc &&
                !c(u,0x81,6,0x2200,p->iface,p->report_length) && u->ctrl_bytes==p->report_length &&
                pointer_layout(u->desc,p->report_length,MIN(p->packet,64),&p->layout);
            if(!boot_ok){
                if(parsed && select_protocol(u,p->iface,1)){p->proto=3;report_ready=1;}
                else continue;
            }
        }else if(p->proto==1 && !select_protocol(u,p->iface,0))continue;
        if(p->proto==3 && !report_ready){
            n=p->report_length;
            if(!n||n>sizeof u->desc||c(u,0x81,6,0x2200,p->iface,n)||u->ctrl_bytes<n||
               !pointer_layout(u->desc,n,MIN(p->packet,64),&p->layout))continue;
        }
        /* SET_IDLE is optional; rejecting it must not disable a valid HID. */
        (void)c(u,0x21,10,0,p->iface,0);
        if(p->proto==1){input_diag.keyboards++;device_add("USB HID keyboard (USB2)","input",0);}
        else {input_diag.pointers++;device_add("USB HID pointer (USB2)","input",0);}
        u->npipe++;usable++;
    }
    return usable!=0;
}
void usb_input_report(UsbInput *u,unsigned pipe,unsigned bytes)
{
    if(pipe>=u->npipe)return;UsbInputPipe *p=u->pipe+pipe;
    input_diag.reports++;
    if(bytes>p->packet){input_diag.bad_reports++;return;}
    if(p->proto==1&&bytes>=8)hid_keys(p->report);
    else if(p->proto==2&&bytes>=3){TabletState s;
        if(pointer_boot_read(&p->layout,p->report,bytes,boot.w,boot.h,&s))hid_relative(s.x,s.y,s.buttons,s.wheel);
    }
    else if(p->proto==3) {
        TabletState s;
        if(pointer_read(&p->layout,p->report,bytes,boot.w,boot.h,&s)) {
            if(p->layout.relative)hid_relative(s.x,s.y,s.buttons,s.wheel);
            else hid_absolute(s.x,s.y,s.buttons,s.wheel);
        }else input_diag.bad_reports++;
    }
}
