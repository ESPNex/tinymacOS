#include "../kernel/k.h"
#include "usbhost.h"
/* Native EHCI USB 2.0: periodic HID, asynchronous control, ownership handoff,
 * and split transactions for HID behind USB2 hubs. Root low/full speed ports
 * are handed to companion controllers (UHCI is implemented separately).
 * DMA descriptors and buffers stay allocated for controller lifetime. */
#define EC 4
#define EN 8
#define ET 132
#define EEND 1u
#define EQH 2u
#define EACTIVE 0x80u
#define EHALT 0x40u
struct ALIGNED(64) Etd {u32 next,alt,token,buf[5],hi[5];};
struct ALIGNED(128) Eqh {u32 link,ep,cap,current,next,alt,token,buf[5],hi[5];};
struct Edev {
    UsbInput u;
    struct Eqh qh[USB_INPUT_INTERFACES];
    struct Etd td[USB_INPUT_INTERFACES];
    unsigned port;
};
static struct Ehci {
    u32 frames[1024] ALIGNED(4096);
    struct Eqh anchors[10],async,control;
    struct Etd td[ET];
    u8 setup[8] ALIGNED(4096);
    struct Edev dev[EN];
    volatile u32 *op;
    unsigned active,used,ports;
} ehcis[EC];
static unsigned nehci;
static u32 epchar(UsbInput *u,unsigned ep,unsigned packet)
{
    unsigned speed=u->speed==3?2:u->speed==2?1:0;
    return u->address|(ep<<8)|(speed<<12)|(1u<<14)|(packet<<16)|
        ((!ep&&u->speed!=3)?1u<<27:0)|(4u<<28);
}
static u32 epcap(UsbInput *u,unsigned smask)
{
    return smask|((u->speed!=3?0x1cu:0)<<8)|(u->hub<<16)|(u->hub_port<<23)|(1u<<30);
}
static int ewait(volatile u32 *p,u32 mask,int set,unsigned ms)
{
    while(ms--){if(!!(*p&mask)==set)return 1;usb_delay_ms(1);}return !!(*p&mask)==set;
}
static int async_stop(struct Ehci *h)
{
    h->op[0]&=~(1u<<5);return ewait(h->op+1,1u<<15,0,100);
}
static void async_start(struct Ehci *h){asm volatile("mfence":::"memory");h->op[0]|=1u<<5;}
static void etd(struct Etd *t,u64 buf,unsigned n,unsigned pid,unsigned toggle)
{
    memset(t,0,sizeof *t);t->next=t->alt=EEND;
    t->token=(toggle<<31)|(n<<16)|(3u<<10)|(pid<<8)|EACTIVE;
    t->buf[0]=(u32)buf;for(unsigned i=1;i<5;i++)t->buf[i]=(u32)((buf&~4095ull)+4096*i);
}
static int econtrol(UsbInput *u,int rt,int rq,int val,int ix,int n)
{
    struct Ehci *h=u->host;if(!async_stop(h))return -1;
    u64 request=(u64)rt|((u64)rq<<8)|((u64)val<<16)|((u64)ix<<32)|((u64)n<<48);
    memcpy(h->setup,&request,8);int in=rt&128;
    unsigned count=1,off=0,toggle=1;etd(h->td,(u64)h->setup,8,2,0);
    while(off<(unsigned)n){unsigned z=MIN((unsigned)n-off,u->max_packet0);etd(h->td+count,(u64)u->desc+off,z,in?1:0,toggle);off+=z;count++;toggle^=1;}
    unsigned status=count++;etd(h->td+status,0,0,in?0:1,1);
    for(unsigned i=0;i+1<count;i++){h->td[i].next=(u32)(u64)(h->td+i+1);h->td[i].alt=(u32)(u64)(h->td+status);}
    memset(&h->control,0,sizeof h->control);h->control.link=(u32)(u64)&h->async|EQH;
    h->control.ep=epchar(u,0,u->max_packet0);h->control.cap=epcap(u,0);
    h->control.next=(u32)(u64)h->td;h->control.alt=EEND;
    h->async.link=(u32)(u64)&h->control|EQH;async_start(h);
    int result=-1;
    for(unsigned wait=0;wait<300;wait++) {
        asm volatile("lfence":::"memory");
        u32 st=*(volatile u32 *)&h->td[status].token;
        if(!(st&EACTIVE)){result=st&0x7c?(st&EHALT?-2:-1):0;break;}
        u32 q=*(volatile u32 *)&h->control.token;if(q&EHALT){result=-2;break;}
        usb_delay_ms(1);
    }
    /* Quiesce before rewriting any control QH/TD, including timeout paths. */
    if(!async_stop(h))return -1;
    h->async.link=(u32)(u64)&h->async|EQH;
    u->ctrl_bytes=0;
    if(!result)for(unsigned i=1;i<status;i++) {
        u32 st=h->td[i].token;if(st&EACTIVE)continue;
        unsigned requested=MIN((unsigned)n-(i-1)*u->max_packet0,u->max_packet0);
        unsigned remaining=(st>>16)&0x7fff;if(remaining>requested){result=-1;break;}u->ctrl_bytes+=requested-remaining;
    }
    if(result)input_diag.control_errors++;
    async_start(h);return result;
}
static unsigned level_for(unsigned interval)
{
    unsigned l=0;while(l<9 && (2u<<l)<=interval)l++;return l;
}
static void epipe_arm(struct Edev *d,unsigned j)
{
    UsbInputPipe *p=d->u.pipe+j;struct Eqh *q=d->qh+j;struct Etd *t=d->td+j;
    memset(p->report,0,sizeof p->report);etd(t,(u64)p->report,p->packet,1,p->toggle);
    q->current=0;q->alt=EEND;q->token=0;memset(q->buf,0,sizeof q->buf);memset(q->hi,0,sizeof q->hi);
    asm volatile("mfence":::"memory");q->next=(u32)(u64)t;p->armed=1;
}
static void install_pipes(struct Ehci *h,struct Edev *d)
{
    for(unsigned j=0;j<d->u.npipe;j++) {
        UsbInputPipe *p=d->u.pipe+j;struct Eqh *q=d->qh+j;
        unsigned interval=d->u.speed==3?(p->interval>4?1u<<MIN(p->interval-4,9u):1):p->interval;
        unsigned l=level_for(interval),smask=d->u.speed==3&&p->interval<4?(p->interval==1?255:p->interval==2?85:17):1;
        memset(q,0,sizeof *q);q->ep=epchar(&d->u,p->ep,p->packet);q->cap=epcap(&d->u,smask);
        q->link=h->anchors[l].link;epipe_arm(d,j);
        asm volatile("mfence":::"memory");h->anchors[l].link=(u32)(u64)q|EQH;
    }
}
static void eattach(struct Ehci *,unsigned,unsigned,unsigned,unsigned,unsigned);
static void hub_children(struct Ehci *h,struct Edev *hub,unsigned depth)
{
    UsbInput *u=&hub->u;if(depth>=3||econtrol(u,0xa0,6,0x2900,0,9)||u->ctrl_bytes<7)return;
    if(u->desc[0]<7||u->desc[1]!=0x29)return;
    unsigned ports=MIN(u->desc[2],15u),delay=MAX(20u,(unsigned)u->desc[5]*2);
    for(unsigned p=1;p<=ports;p++)(void)econtrol(u,0x23,3,8,p,0);
    usb_delay_ms(delay);
    for(unsigned p=1;p<=ports&&h->used<EN;p++) {
        if(econtrol(u,0xa3,0,0,p,4)||u->ctrl_bytes<4||!(u->desc[0]&1))continue;
        usb_delay_ms(100);
        if(econtrol(u,0x23,3,4,p,0))continue;
        unsigned s=0;int ready=0;
        for(unsigned ms=0;ms<500;ms+=10) {
            usb_delay_ms(10);
            if(econtrol(u,0xa3,0,0,p,4)||u->ctrl_bytes<4)break;
            s=u->desc[0]|u->desc[1]<<8;
            if(!(s&1)||(s&8))break;
            if((s&2)&&!(s&16)){ready=1;break;}
        }
        if(!ready)continue;
        usb_delay_ms(10);
        unsigned speed=s&1024?3:s&512?2:1;
        for(unsigned f=16;f<=20;f++)(void)econtrol(u,0x23,1,f,p,0);
        eattach(h,p,speed,u->speed==3?u->address:u->hub,u->speed==3?p:u->hub_port,depth+1);
    }
}
static void eattach(struct Ehci *h,unsigned port,unsigned speed,unsigned hub,unsigned hub_port,unsigned depth)
{
    if(h->used>=EN)return;struct Edev *d=h->dev+h->used++;d->port=port;
    d->u.host=h;d->u.control=econtrol;d->u.speed=speed;d->u.hub=hub;d->u.hub_port=hub_port;
    int kind=usb_input_enumerate(&d->u,h->used);
    if(kind==1)install_pipes(h,d);else if(kind==2)hub_children(h,d,depth);
}
static void econtroller(u32 pci)
{
    struct Ehci *h=ehcis+nehci;volatile u32 *c=pci_mmio(pci,0);if(!c)return;
    unsigned eecp=(c[2]>>8)&255;
    for(unsigned n=0;eecp>=0x40&&n<32;n++) {
        u32 leg=pci_reg(pci,eecp);
        if((leg&255)==1) {
            outl(0xcf8,pci|eecp);outl(0xcfc,leg|1u<<24);
            unsigned i=1000;while(i--&&(pci_reg(pci,eecp)&1u<<16))usb_delay_ms(1);
            if(pci_reg(pci,eecp)&1u<<16)return;
            outl(0xcf8,pci|(eecp+4));outl(0xcfc,0);break;
        }
        eecp=(leg>>8)&255;
    }
    h->op=(void *)((u8 *)c+(c[0]&255));volatile u32 *op=h->op;
    op[0]&=~1u;if(!ewait(op+1,1u<<12,1,100))return;
    op[0]=2;if(!ewait(op,2,0,100))return;
    for(unsigned l=0;l<10;l++) {
        struct Eqh *q=h->anchors+l;q->link=l?(u32)(u64)(h->anchors+l-1)|EQH:EEND;
        q->ep=2u<<12|64u<<16;q->next=q->alt=EEND;q->token=EHALT;
    }
    for(unsigned f=0;f<1024;f++){unsigned l=0;if(!f)l=9;else while(l<9 && !(f&(1u<<l)))l++;h->frames[f]=(u32)(u64)(h->anchors+l)|EQH;}
    h->async.link=(u32)(u64)&h->async|EQH;h->async.ep=1u<<15|2u<<12|64u<<16;
    h->async.next=h->async.alt=EEND;h->async.token=EHALT;
    op[2]=0;op[4]=0;op[5]=(u32)(u64)h->frames;op[6]=(u32)(u64)&h->async;op[3]=0;op[1]=0x3f;
    asm volatile("mfence":::"memory");op[0]=1|1u<<4|1u<<5|8u<<16;
    if(!ewait(op+1,1u<<12,0,100))return;
    op[16]=1;h->ports=c[1]&15;usb_delay_ms(30);
    for(unsigned p=0;p<h->ports && h->used<EN;p++) {
        volatile u32 *s=op+17+p;u32 v=*s;
        if(c[1]&16){*s=(v&~0x2au)|1u<<12;usb_delay_ms(20);v=*s;}
        if(!(v&1))continue;
        if(((v>>10)&3)==1){*s=(v&~0x2au)|1u<<13;continue;}
        *s=(v&~0x2eu)|1u<<8;usb_delay_ms(55);*s=(*s&~0x12au);
        if(!ewait(s,1u<<8,0,100))continue;usb_delay_ms(10);v=*s;
        if(v&4)eattach(h,p+1,3,0,0,0);
        else *s=(v&~0x2au)|1u<<13;
    }
    h->active=1;nehci++;device_add("USB EHCI 2.0 controller","usb",0);
}
void ehci_init(void)
{
    u32 pci=0;while(nehci<EC&&(pci=pci_find_after(0x0c0320,pci)))econtroller(pci);
}
void ehci_poll(void)
{
    for(unsigned c=0;c<nehci;c++) {
        struct Ehci *h=ehcis+c;if(!h->active)continue;
        for(unsigned i=0;i<h->used;i++)for(unsigned j=0;j<h->dev[i].u.npipe;j++) {
            struct Edev *d=h->dev+i;UsbInputPipe *p=d->u.pipe+j;if(!p->armed)continue;
            if(p->armed<0) {
                if(ticks<p->retry_at)continue;p->armed=0;
                if(!econtrol(&d->u,2,1,0,0x80|p->ep,0)){p->toggle=0;epipe_arm(d,j);}
                continue;
            }
            u32 st=*(volatile u32 *)&d->td[j].token;if(st&EACTIVE)continue;
            p->armed=0;
            if(st&0x7c){
                input_diag.transfer_errors++;
                if(p->proto==1){const u8 released[8]={0};hid_keys(released);}
                if(p->failures++<3){p->armed=-1;p->retry_at=ticks+100;}
                continue;
            }
            unsigned remaining=(st>>16)&0x7fff;
            if(remaining<=p->packet)usb_input_report(&d->u,j,p->packet-remaining);
            p->toggle^=1;epipe_arm(d,j);
        }
    }
}
