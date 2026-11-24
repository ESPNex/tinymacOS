#ifdef TMC_CONTROLLER_DMA_TEST
#include "../tests/controller_dma_support.h"
#else
#include "../kernel/k.h"
#endif
#include "usbhost.h"
#include "linux_usb.h"
#include "linux_dma_pool.h"
#include "linux_ehci.h"
/* Native EHCI controller/scheduler with the upstream Linux qTD DMA primitive.
 * This is not yet the complete Linux EHCI HCD or general USB core.
 * Native EHCI USB 2.0: periodic HID, asynchronous control, ownership handoff,
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
    struct Etd *td;u64 td_dma;
    unsigned port;
};
static struct Ehci {
    u32 frames[1024] ALIGNED(4096);
    struct Eqh anchors[10],async,control;
    struct Etd *td;u64 td_dma;
    TmcLinuxDmaDevice *dma_device;
    TmcLinuxDmaPool *control_pool,*pipe_pool;
    u8 setup[8] ALIGNED(4096);
    struct Edev dev[EN];
    volatile u32 *op;
    unsigned active,used,ports;
} ehcis[EC];
static unsigned nehci;
/* Whole original Linux dmapool.c supplies real DMA descriptors. The physical
 * controller/scheduler is still native. CPU pointers and bus handles are kept
 * separate even on the present identity-DMA target. Only release while halted. */
static void ehci_dma_release(struct Ehci *h)
{
 for(unsigned i=0;i<EN;i++)if(h->dev[i].td){(void)tmc_linux_dma_pool_free(h->pipe_pool,h->dev[i].td,h->dev[i].td_dma);h->dev[i].td=0;}
 if(h->td){(void)tmc_linux_dma_pool_free(h->control_pool,h->td,h->td_dma);h->td=0;}
 if(h->pipe_pool){(void)tmc_linux_dma_pool_destroy(h->pipe_pool);h->pipe_pool=0;}
 if(h->control_pool){(void)tmc_linux_dma_pool_destroy(h->control_pool);h->control_pool=0;}
 if(h->dma_device){(void)tmc_linux_dma_device_destroy(h->dma_device);h->dma_device=0;}
}
static int ehci_dma_init(struct Ehci *h)
{
 h->dma_device=tmc_linux_dma_device_create("ehci-native",0xffffffffull);if(!h->dma_device)return 0;
 h->control_pool=tmc_linux_dma_pool_create(h->dma_device,"ehci-control-td",sizeof(struct Etd)*ET,sizeof(struct Etd),0);
 h->pipe_pool=tmc_linux_dma_pool_create(h->dma_device,"ehci-interrupt-td",sizeof(struct Etd)*USB_INPUT_INTERFACES,sizeof(struct Etd),4096);
 if(!h->control_pool||!h->pipe_pool)goto fail;
 h->td=tmc_linux_dma_pool_alloc(h->control_pool,&h->td_dma);if(!h->td)goto fail;return 1;
fail:ehci_dma_release(h);return 0;
}
static int ehci_pipe_dma(struct Ehci *h,struct Edev *d)
{
 if(d->td)return 1;d->td=tmc_linux_dma_pool_alloc(h->pipe_pool,&d->td_dma);return d->td!=0;
}

static u32 epchar(UsbInput *u,unsigned ep,unsigned packet)
{
    unsigned speed=u->speed==3?2:u->speed==2?1:0;
    return u->address|(ep<<8)|(speed<<12)|(!ep?1u<<14:0)|(packet<<16)|
        ((!ep&&u->speed!=3)?1u<<27:0)|(!ep?4u<<28:0);
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
static int periodic_stop(struct Ehci *h){h->op[0]&=~(1u<<4);return ewait(h->op+1,1u<<14,0,100);}
static void periodic_start(struct Ehci *h){asm volatile("mfence":::"memory");h->op[0]|=1u<<4;}
static void async_start(struct Ehci *h){asm volatile("mfence":::"memory");h->op[0]|=1u<<5;}
static int etd(struct Etd *t,u64 buf,unsigned n,unsigned pid,unsigned toggle,unsigned maxpacket)
{
    memset(t,0,sizeof *t);
    int count=tmc_linux_ehci_qtd_build(t,sizeof *t,buf,n,
        (toggle<<31)|(3u<<10)|(pid<<8)|EACTIVE,maxpacket);
    t->next=t->alt=EEND;
    if(count<0||(unsigned)count!=n){t->token=EHALT;return 0;}
    return 1;
}
static int econtrol(UsbInput *u,int rt,int rq,int val,int ix,int n)
{
    struct Ehci *h=u->host;if(!async_stop(h))return -1;
    u64 request=(u64)rt|((u64)rq<<8)|((u64)val<<16)|((u64)ix<<32)|((u64)n<<48);
    memcpy(h->setup,&request,8);int in=rt&128;
    unsigned count=1,off=0,toggle=1;
    if(!etd(h->td,(u64)h->setup,8,2,0,u->max_packet0))return -1;
    while(off<(unsigned)n){unsigned z=MIN((unsigned)n-off,u->max_packet0);
        if(count>=ET-1||!etd(h->td+count,(u64)u->desc+off,z,in?1:0,toggle,u->max_packet0))return -1;
        off+=z;count++;toggle^=1;}
    unsigned status=count++;
    if(!etd(h->td+status,0,0,in?0:1,1,u->max_packet0))return -1;
    for(unsigned i=0;i+1<count;i++){h->td[i].next=(u32)(h->td_dma+(i+1)*sizeof *h->td);h->td[i].alt=(u32)(h->td_dma+status*sizeof *h->td);}
    memset(&h->control,0,sizeof h->control);h->control.link=(u32)(u64)&h->async|EQH;
    h->control.ep=epchar(u,0,u->max_packet0);h->control.cap=epcap(u,0);
    h->control.next=(u32)h->td_dma;h->control.alt=EEND;
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
    memset(p->report,0,sizeof p->report);
    if(!etd(t,(u64)p->report,p->packet,1,p->toggle,p->packet)){p->armed=0;return;}
    q->current=0;q->alt=EEND;q->token&=1u<<31;memset(q->buf,0,sizeof q->buf);memset(q->hi,0,sizeof q->hi);
    asm volatile("mfence":::"memory");q->next=(u32)(d->td_dma+j*sizeof *d->td);p->armed=1;
}
static void install_pipes(struct Ehci *h,struct Edev *d)
{
    if(!ehci_pipe_dma(h,d)){d->u.npipe=0;return;}if(!periodic_stop(h))return; /* no active QH overlay writes */
    for(unsigned j=0;j<d->u.npipe;j++) {
        UsbInputPipe *p=d->u.pipe+j;struct Eqh *q=d->qh+j;
        unsigned interval=d->u.speed==3?(p->interval>4?1u<<MIN(p->interval-4,9u):1):p->interval;
        unsigned l=level_for(interval),smask=d->u.speed==3&&p->interval<4?(p->interval==1?255:p->interval==2?85:17):1;
        memset(q,0,sizeof *q);q->ep=epchar(&d->u,p->ep,p->packet);q->cap=epcap(&d->u,smask);
        q->link=h->anchors[l].link;epipe_arm(d,j);
        asm volatile("mfence":::"memory");h->anchors[l].link=(u32)(u64)q|EQH;
    }
    periodic_start(h);
}
static void eattach(struct Ehci *,unsigned,unsigned,unsigned,unsigned,unsigned);
static void hub_children(struct Ehci *h,struct Edev *hub,unsigned depth)
{
    UsbInput *u=&hub->u;if(depth>=3||usb_input_control(u,0xa0,6,0x2900,0,u->desc,9)<7)return;
    if(u->desc[0]<7||u->desc[1]!=0x29)return;
    unsigned ports=MIN(u->desc[2],15u),delay=MAX(20u,(unsigned)u->desc[5]*2);
    for(unsigned p=1;p<=ports;p++)(void)usb_input_control(u,0x23,3,8,p,0,0);
    usb_delay_ms(delay);
    for(unsigned p=1;p<=ports&&h->used<EN;p++) {
        unsigned s;
        if(tmc_linux_hub_port_init(usb_input_control,u,p,u->speed==1?2:u->speed==2?1:3,&s))continue;
        unsigned speed=s&1024?3:s&512?2:1;
        eattach(h,p,speed,u->speed==3?u->address:u->hub,u->speed==3?p:u->hub_port,depth+1);
    }
}
static void eattach(struct Ehci *h,unsigned port,unsigned speed,unsigned hub,unsigned hub_port,unsigned depth)
{
    if(h->used>=EN)return;struct Edev *d=h->dev+h->used++;d->port=port;
    if(!ehci_pipe_dma(h,d))return; /* allocate before class attachment */
    d->u.host=h;d->u.control=econtrol;d->u.speed=speed;d->u.hub=hub;d->u.hub_port=hub_port;
    int kind=usb_input_enumerate(&d->u,h->used);
    if(kind==1)install_pipes(h,d);else if(kind==2)hub_children(h,d,depth);
}
static void econtroller(u32 pci)
{
    struct Ehci *h=ehcis+nehci;volatile u32 *c=pci_mmio(pci,0);if(!c)return;
    nehci++; /* reserve attempted slot: never reuse uncertain live DMA */
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
    if(!ehci_dma_init(h))return; /* reset completed: no descriptor is live yet */
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
    tmc_linux_usb_cf_begin();op[16]=1;
    (void)op[0]; /* flush posted PCI MMIO before the ownership-settle delay */
    usb_delay_ms(30);tmc_linux_usb_cf_end();h->ports=c[1]&15;
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
    h->active=1;device_add("USB EHCI 2.0 controller","usb",0);
}
void ehci_init(void)
{
    u32 pci=0;while(nehci<EC&&(pci=pci_find_after(0x0c0320,pci)))econtroller(pci);
}
void ehci_poll(void)
{
    for(unsigned c=0;c<nehci;c++) {
        struct Ehci *h=ehcis+c;if(!h->active)continue;
        /* Observe DMA first. Stopping/restarting on every idle input poll can
         * starve real periodic endpoints; quiesce only before an actual edit. */
        asm volatile("lfence":::"memory");
        int work=0;
        for(unsigned i=0;i<h->used;i++)for(unsigned j=0;j<h->dev[i].u.npipe;j++){
            struct Edev *d=h->dev+i;UsbInputPipe *p=d->u.pipe+j;
            if((p->armed<0 && ticks>=p->retry_at) ||
               (p->armed>0 && !(*(volatile u32 *)&d->td[j].token&EACTIVE)))work=1;
        }
        if(!work || !periodic_stop(h))continue;
        asm volatile("lfence":::"memory");
        for(unsigned i=0;i<h->used;i++)for(unsigned j=0;j<h->dev[i].u.npipe;j++) {
            struct Edev *d=h->dev+i;UsbInputPipe *p=d->u.pipe+j;if(!p->armed)continue;
            if(p->armed<0) {
                if(ticks<p->retry_at)continue;p->armed=0;
                if(usb_input_control(&d->u,2,1,0,0x80|p->ep,0,0)>=0){p->toggle=0;d->qh[j].token=0;epipe_arm(d,j);}
                continue;
            }
            u32 st=*(volatile u32 *)&d->td[j].token;if(st&EACTIVE)continue;
            p->armed=0;
            if(st&0x7c){
                input_diag.transfer_errors++;
                if(p->proto==1){const u8 released[8]={0};
                    if(p->linux_keyboard)(void)tmc_linux_keyboard_report(p,released,8);else hid_keys(released);}
                if(p->linux_mouse)(void)tmc_linux_mouse_error(p,p->failures<3?-32:-108);
                if(p->linux_generic)tmc_linux_generic_error(p,p->failures>=3);
                if(p->failures++<3){p->armed=-1;p->retry_at=ticks+100;}
                continue;
            }
            p->failures=0; /* successful traffic resets the consecutive-error budget */
            unsigned remaining=(st>>16)&0x7fff;
            if(remaining<=p->packet)usb_input_report(&d->u,j,p->packet-remaining);
            /* QH hardware toggle already accounts for actual USB packets,
             * including short/zero transfers: never flip it per software TD. */
            epipe_arm(d,j);
        }
        periodic_start(h);
    }
}
