#include "../kernel/k.h"
#include "usbhost.h"
#include "linux_usb.h"
/* Native UHCI companions: low/full-speed HID from EHCI-owned USB2 ports,
 * plus standalone UHCI root ports. Periodic frame tree and independent
 * control QH; no reliance on USB-legacy SMM emulation. */
#define UC 8
#define UT 132
#define UTERM 1u
#define UQH 2u
#define UACTIVE (1u<<23)
#define UERROR ((1u<<22)|(1u<<21)|(1u<<20)|(1u<<18)|(1u<<17))
struct ALIGNED(16) Utd {u32 link,status,token,buffer;};
struct ALIGNED(16) Uqh {u32 link,element,pad[2];};
struct Udev {UsbInput u;struct Uqh qh[USB_INPUT_INTERFACES];struct Utd td[USB_INPUT_INTERFACES];};
static struct Uhci {
    u32 frames[1024] ALIGNED(4096);
    struct Uqh anchors[10],ctrl;
    struct Utd td[UT];
    u8 setup[8] ALIGNED(4096);
    struct Udev dev[USB_INPUT_DEVICES];
    unsigned used,active;u16 io;
} uhcis[UC];
static unsigned nuhci;
static int ustop(struct Uhci *h)
{
    outw(h->io,inw(h->io)&~1u);
    for(unsigned i=0;i<100;i++){if(inw(h->io+2)&32)return 1;usb_delay_ms(1);}return 0;
}
static void ustart(struct Uhci *h){asm volatile("mfence":::"memory");outw(h->io,0xc1);}
static void utd(struct Utd *t,u64 buffer,unsigned n,unsigned pid,UsbInput *u,unsigned ep,unsigned toggle)
{
    *t=(struct Utd){.link=UTERM,.status=UACTIVE|3u<<27|(u->speed==2?1u<<26:0)|1u<<29,
        .token=((n?n-1:2047)<<21)|(toggle<<19)|(ep<<15)|(u->address<<8)|pid,.buffer=(u32)buffer};
}
static unsigned actual(struct Utd *t){return (t->status+1)&2047;}
static int ucontrol(UsbInput *u,int rt,int rq,int val,int ix,int n)
{
    struct Uhci *h=u->host;if(!ustop(h))return -1;
    u64 request=(u64)rt|((u64)rq<<8)|((u64)val<<16)|((u64)ix<<32)|((u64)n<<48);
    memcpy(h->setup,&request,8);unsigned count=1,off=0,toggle=1;int in=rt&128;
    utd(h->td,(u64)h->setup,8,0x2d,u,0,0);
    while(off<(unsigned)n){unsigned z=MIN((unsigned)n-off,u->max_packet0);utd(h->td+count,(u64)u->desc+off,z,in?0x69:0xe1,u,0,toggle);off+=z;toggle^=1;count++;}
    unsigned status=count++;utd(h->td+status,0,0,in?0xe1:0x69,u,0,1);
    for(unsigned i=0;i+1<count;i++)h->td[i].link=(u32)(u64)(h->td+i+1)|4;
    h->ctrl.element=(u32)(u64)h->td;ustart(h);int result=-1,short_done=0;
    for(unsigned wait=0;wait<300;wait++) {
        asm volatile("lfence":::"memory");u32 st=*(volatile u32 *)&h->td[status].status;
        if(!(st&UACTIVE)){result=st&UERROR?(st&1u<<22?-2:-1):0;break;}
        int failed=0,short_packet=0;
        for(unsigned i=0;i<status;i++) {
            u32 s=*(volatile u32 *)&h->td[i].status;if(s&UACTIVE)continue;
            if(s&UERROR){result=s&1u<<22?-2:-1;failed=1;break;}
            unsigned want=((h->td[i].token>>21)+1)&2047;
            if(i&&actual(h->td+i)<want)short_packet=1;
        }
        if(failed)break;
        if(short_packet&&!short_done) {
            if(!ustop(h))return -1;h->ctrl.element=(u32)(u64)(h->td+status);short_done=1;ustart(h);
        }
        usb_delay_ms(1);
    }
    if(!ustop(h))return -1;h->ctrl.element=UTERM;u->ctrl_bytes=0;
    if(!result)for(unsigned i=1;i<status;i++) {
        if(h->td[i].status&UACTIVE)break;u->ctrl_bytes+=actual(h->td+i);
    }
    if(result)input_diag.control_errors++;
    ustart(h);return result;
}
static void upipe_arm(struct Udev *d,unsigned j)
{
    UsbInputPipe *p=d->u.pipe+j;memset(p->report,0,sizeof p->report);
    utd(d->td+j,(u64)p->report,p->packet,0x69,&d->u,p->ep,p->toggle);
    asm volatile("mfence":::"memory");d->qh[j].element=(u32)(u64)(d->td+j);p->armed=1;
}
static void upipes(struct Uhci *h,struct Udev *d)
{
    for(unsigned j=0;j<d->u.npipe;j++) {
        UsbInputPipe *p=d->u.pipe+j;unsigned l=0;
        while(l<9&&(2u<<l)<=p->interval)l++;
        d->qh[j].link=h->anchors[l].link;upipe_arm(d,j);
        asm volatile("mfence":::"memory");h->anchors[l].link=(u32)(u64)(d->qh+j)|UQH;
    }
}
static void ucontroller(u32 pci)
{
    u32 bar=pci_reg(pci,0x20);if(!(bar&1)||!(bar&0xffe0))return;
    struct Uhci *h=uhcis+nuhci;h->io=bar&0xffe0;
    outl(0xcf8,pci|4);outw(0xcfc,pci_reg(pci,4)|5|0x400);
    /* Disable chipset USB legacy/SMM traps before claiming the controller. */
    outl(0xcf8,pci|0xc0);outw(0xcfc,0x8f00);
    outw(h->io,0);usb_delay_ms(2);outw(h->io,2);usb_delay_ms(10);
    if(inw(h->io)&2)return;
    outw(h->io+4,0);outw(h->io+6,0);outb(h->io+12,64);
    h->ctrl.link=h->ctrl.element=UTERM;
    for(unsigned l=0;l<10;l++){h->anchors[l].link=l?(u32)(u64)(h->anchors+l-1)|UQH:(u32)(u64)&h->ctrl|UQH;h->anchors[l].element=UTERM;}
    for(unsigned f=0;f<1024;f++){unsigned l=0;if(!f)l=9;else while(l<9&&!(f&(1u<<l)))l++;h->frames[f]=(u32)(u64)(h->anchors+l)|UQH;}
    outl(h->io+8,(u32)(u64)h->frames);outw(h->io+2,0x3f);ustart(h);usb_delay_ms(10);
    for(unsigned port=0;port<2&&h->used<USB_INPUT_DEVICES;port++) {
        u16 reg=h->io+16+2*port,v=inw(reg);if(!(v&1))continue;
        outw(reg,(v&~0xau)|0x200);usb_delay_ms(55);outw(reg,inw(reg)&~0x20au);usb_delay_ms(12);
        v=inw(reg);outw(reg,(v&~0xau)|4);usb_delay_ms(10);v=inw(reg);
        if(!(v&1)||!(v&4))continue;
        struct Udev *d=h->dev+h->used++;d->u.host=h;d->u.control=ucontrol;d->u.speed=v&0x100?2:1;
        if(usb_input_enumerate(&d->u,h->used)==1)upipes(h,d);
    }
    h->active=1;nuhci++;device_add("USB UHCI companion controller","usb",0);
}
void uhci_init(void)
{
    u32 pci=0;while(nuhci<UC&&(pci=pci_find_after(0x0c0300,pci)))ucontroller(pci);
}
void uhci_poll(void)
{
    for(unsigned c=0;c<nuhci;c++) {
        struct Uhci *h=uhcis+c;if(!h->active)continue;
        for(unsigned i=0;i<h->used;i++)for(unsigned j=0;j<h->dev[i].u.npipe;j++) {
            struct Udev *d=h->dev+i;UsbInputPipe *p=d->u.pipe+j;if(!p->armed)continue;
            if(p->armed<0) {
                if(ticks<p->retry_at)continue;p->armed=0;
                if(!ucontrol(&d->u,2,1,0,0x80|p->ep,0)){p->toggle=0;upipe_arm(d,j);}
                continue;
            }
            u32 st=*(volatile u32 *)&d->td[j].status;if(st&UACTIVE)continue;p->armed=0;
            if(st&UERROR){
                input_diag.transfer_errors++;
                if(p->proto==1){const u8 released[8]={0};
                    if(p->linux_keyboard)(void)tmc_linux_keyboard_report(p,released,8);else hid_keys(released);}
                if(p->linux_mouse)(void)tmc_linux_mouse_error(p,p->failures<3?-32:-108);
                if(p->linux_generic)tmc_linux_generic_error(p,p->failures>=3);
                if(p->failures++<3){p->armed=-1;p->retry_at=ticks+100;}
                continue;
            }
            p->failures=0; /* successful traffic resets the consecutive-error budget */
            unsigned bytes=actual(d->td+j);if(bytes<=p->packet)usb_input_report(&d->u,j,bytes);
            p->toggle^=1;upipe_arm(d,j);
        }
    }
}
