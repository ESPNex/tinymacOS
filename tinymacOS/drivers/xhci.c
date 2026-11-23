#ifdef TMC_INPUT_TEST
#include "../tests/input_test_support.h"
#else
#include "../kernel/k.h"
#endif
#include "hidtablet.h"
#include "hidconfig.h"
#include "linux_usb.h"
#include "usbroute.h"
#ifndef TMC_INPUT_TEST
#include "usbhost.h"
#endif
// Native HID at root ports and behind USB2 hubs (including onboard hubs).
// Supervisor-only DMA memory, producer-cycle rings, bounded waits, no firmware/PS2 input.
#define NS 8
#define NR 64
#define NE 256
#define TY(t) ((u32)(t)<<10)
typedef struct ALIGNED(16)
{
    u64 ptr;
    u32 len, ctl;
}
TRB;
typedef struct {
    TRB tr[NR] ALIGNED(1024);
    u32 at, cycle;
} Ring;
#define NHID 4
#define NCONTROLLERS 4
struct Hid {
    Ring in;
    u8 report[1024] ALIGNED(1024); /* a Normal TRB must not cross a 64 KiB boundary */
    TabletLayout tablet;
    int ep,proto,mps,armed,iface,report_len,failures,pending,linux_mouse,linux_keyboard,linux_generic;
    u64 retry_at;
};
struct Usb {
    Ring ctrl;
    u8 outctx[32*64] ALIGNED(64),ic[33*64] ALIGNED(64);
    u8 desc[1024] ALIGNED(1024); /* Data Stage TRBs have the same 64 KiB boundary rule */
    struct Hid hid[NHID];
    struct Xhci *controller;
    int id,port,speed,nhid,diag,ctrl_dead,ctrl_bytes;
    unsigned route,depth,tt_slot,tt_port,tt_think,hub_ports,hub_think;
};
static struct Xhci {
    u64 dcbaa[256] ALIGNED(64),sptr[32] ALIGNED(64);
    TRB events[NE] ALIGNED(4096);
    struct {u64 base;u32 size,res;} erst ALIGNED(64);
    Ring commands;
    int evpos,evcycle,cs,connected,active,lastcode,version,ports,ppc;
    u32 pci;
    volatile u32 *op,*db,*ir;
    struct Usb usb[NS];
} controllers[NCONTROLLERS],*hc;
static unsigned ncontrollers;
/* Selected only during pre-IRQ initialization or the CPU0 PIT event drain. */
#define dcbaa (hc->dcbaa)
#define sptr (hc->sptr)
#define events (hc->events)
#define erst (hc->erst)
#define commands (hc->commands)
#define evpos (hc->evpos)
#define evcycle (hc->evcycle)
#define cs (hc->cs)
#define connected (hc->connected)
#define op (hc->op)
#define db (hc->db)
#define ir (hc->ir)
#define usb (hc->usb)
static void transferred(const TRB *);
static void w64(volatile u32*p, u64 v)
{
    p[0] = (u32)v;
    p[1] = v >> 32;
}
static void ringinit(Ring *r)
{
    memset(r, 0, sizeof * r);
    r->cycle = 1;
    r->tr[NR - 1] = (TRB) {
        (u64)r->tr, 0, TY(6) | 2
    };
}
static TRB* enqueue(Ring*r, u64 addr, u32 len, u32 ctrl,int publish)
{
    TRB*t = r->tr + r->at;
    t->ptr = addr;
    t->len = len;
    asm volatile("sfence":::"memory");
    t->ctl=ctrl|(r->cycle^!publish);
    if (++r->at == NR - 1) {
        r->tr[NR - 1].ctl = TY(6) | 2 | r->cycle;
        r->cycle ^= 1;
        r->at = 0;
    }
    return t;
}
static TRB *push(Ring *r,u64 addr,u32 len,u32 ctrl){return enqueue(r,addr,len,ctrl,1);}
static int event(TRB *e)
{
    volatile TRB *t = events + evpos;
    if ((t->ctl & 1) != (u32)evcycle)
        return 0;
    asm volatile("lfence":::"memory");
    *e=*t;
    if (++evpos==NE) {
        evpos=0;
        evcycle^=1;
    }
    w64(ir+6, (u64)(events+evpos)|8);
    return 1;
}
static void ringbell(int s, int ep)
{
    asm volatile("mfence":::"memory");
    db[s]=ep;
}
static int waitresult(TRB*t, int ty, int slot,TRB *setup,TRB *data,int control_slot)
{
    TRB e;
    hc->lastcode=0;
    for (int i = 0; i < 500000; i++) {
        if(i && !(i%1000))usb_delay_ms(1);
        if (!event(&e)) {
            asm volatile("pause");
            continue;
        }
        /* Linux process_ctrl_td: a short Data event sets actual_length but
         * must NOT complete the request before its Status event. */
        if(ty==32 && data && ((e.ctl>>10)&63)==32 && e.ptr==(u64)data &&
           (e.ctl>>24)==(u32)control_slot && ((e.ctl>>16)&31)==1 && (e.len>>24)==13){
            unsigned remain=e.len&0xffffff,asked=data->len&0x1ffff;
            for(int j=0;j<connected;j++)if(usb[j].id==control_slot)
                usb[j].ctrl_bytes=remain<=asked?(int)(asked-remain):0;
            continue;
        }
        int control_error=ty==32 && (e.len>>24)!=1 && (e.len>>24)!=13 &&
            (e.ctl>>24)==(u32)control_slot && ((e.ctl>>16)&31)==1 &&
            (e.ptr==(u64)setup || (data && e.ptr==(u64)data));
        if (((e.ctl>>10)&63)!=(u32)ty || (e.ptr!=(u64)t && !control_error)) {
            if(((e.ctl>>10)&63)==32)transferred(&e);
            continue;
        }
        int c=e.len>>24;
        hc->lastcode=c;
        if (c!=1&&c!=13) {
            logstr("[usb] completion ");
            lognum(c);
            logstr("\n");
            return -1;
        }
        return slot?(int)(e.ctl>>24):0;
    }
    return -1;
}
static int result(TRB *t,int ty,int slot)
{
    return waitresult(t,ty,slot,0,0,0);
}
static int command(int ty, u64 ptr, int slot)
{
    TRB*t = push(&commands, ptr, 0, TY(ty) | ((u32)slot << 24));
    ringbell(0, 0);
    return result(t, 33, 1);
}
static int epcommand(int ty,struct Usb *u,int ep,u64 ptr)
{
    TRB *t=push(&commands,ptr,0,TY(ty)|((u32)ep<<16)|((u32)u->id<<24));
    ringbell(0,0);return result(t,33,0);
}
static int control(struct Usb*u, int rt, int rq, int val, int ix, int n)
{
    if(u->ctrl_dead)return -1;
    u64 s = (u64)rt | ((u64)rq << 8) | ((u64)val << 16) | ((u64)ix << 32) | ((u64)n << 48);
    int in = rt & 128;u->ctrl_bytes=n;
    if(in&&n)memset(u->desc,0,n);
    /* Publish the Setup cycle LAST: a running controller may prefetch before
     * the next doorbell. Linux giveback_first_trb follows this TD invariant. */
    TRB *setup=enqueue(&u->ctrl, s, 8, TY(2) | 64 | ((n ? (in ? 3u : 2u) : 0u) << 16),0);
    TRB *data=0;
    if (n)
        data=push(&u->ctrl, (u64)u->desc, n, TY(3) | (in ? (1 << 16)|4 : 0));
    TRB*t = push(&u->ctrl, 0, 0, TY(4) | 32 | ((n && in) ? 0 : 1 << 16));
    asm volatile("sfence":::"memory");setup->ctl^=1;
    ringbell(u->id, 1);
    int ret=waitresult(t,32,0,setup,data,u->id);
    if(ret<0) {
        int code=hc->lastcode;input_diag.control_errors++;
        /* A rejected optional HID request stalls EP0. Recover the halted
         * control endpoint before issuing any fallback descriptor request.
         * A timeout has unknown ownership: do NOT recycle DMA still in use. */
        if(code!=6 || epcommand(14,u,1,0)<0)u->ctrl_dead=1;
        else {
            ringinit(&u->ctrl);
            if(epcommand(16,u,1,(u64)u->ctrl.tr|1)<0)u->ctrl_dead=1;
        }
    }
    return ret;
}
static int linux_keyboard_control(void *context,const u8 setup[8],u8 *data,unsigned n)
{
    struct Usb *u=context;struct Xhci *saved=hc;
    if(!u||!u->controller||!data||n>sizeof u->desc||(setup[0]&128)||n!=(unsigned)(setup[6]|setup[7]<<8))return -22;
    hc=u->controller;memcpy(u->desc,data,n);
    int result=control(u,setup[0],setup[1],setup[2]|setup[3]<<8,setup[4]|setup[5]<<8,n);
    int status=result?(hc->lastcode==6?-32:-5):0;hc=saved;return status;
}
static int linux_generic_control(void *context,unsigned rt,unsigned req,unsigned value,unsigned iface,u8 *data,unsigned n)
{
 struct Usb *u=context;struct Xhci *saved=hc;
 if(!u||!u->controller||n>sizeof u->desc||(n&&!data)||rt>255||req>255||value>65535||iface>255)return -22;
 hc=u->controller;if(!(rt&128)&&n)memcpy(u->desc,data,n);
 int result=control(u,rt,req,value,iface,n),actual=n;
 if(!result&&(rt&128)){if(u->ctrl_bytes>(int)n)result=-1;else{actual=u->ctrl_bytes;memcpy(data,u->desc,actual);}}
 int status=result?(hc->lastcode==6?-32:-5):actual;hc=saved;return status;
}
/* A successful SET status must not override a contradictory GET_PROTOCOL.
 * Some boot mice acknowledge SET without changing their packet mode. Keep
 * descriptor offsets only when Report really is selected. Unsupported GET is
 * optional after a successful SET; STALL/short replies never confirm stale data. */
static int select_protocol(struct Usb *u,int iface,unsigned expected)
{
    int set=!control(u,0x21,11,expected,iface,0);
    if(u->ctrl_dead)return 0;
    u->desc[0]=255;
    int get=!control(u,0xa1,3,0,iface,1);
    if(u->ctrl_dead)return 0;
    return get ? u->ctrl_bytes==1 && u->desc[0]==expected : set;
}
static u32 *ctx(struct Usb*u, int n)
{
    return (u32*)(u->ic + n * cs);
}
static void endpoint(u32 *p, Ring*r, int type, int pkt, int interval)
{
    p[0] = (u32)interval << 16;
    p[1] = (u32)pkt << 16 | type << 3 | 3 << 1;
    p[2] = (u32)(u64)r->tr | 1;
    p[3] = 0;
    p[4] = type == 4 ? 8 : (u32)pkt | ((u32)pkt << 16);
}
/* USB2 hub interface, alternate zero. A firmware-visible motherboard socket
 * may be downstream of such a hub: "directly connected" is not root-port-only. */
static int hub_config(struct Usb *u,int cfg)
{
    if(u->speed>3 || control(u,0,9,cfg,0,0))return 0;
    memset(u->desc,0,sizeof u->desc);
    if(control(u,0xa0,6,0x2900,0,9))return 0;
    if(u->desc[0]<7 || u->desc[1]!=0x29 || !u->desc[2] || u->desc[2]>15)return 0;
    u->hub_ports=u->desc[2];u->hub_think=(u->desc[3]>>5)&3;
    unsigned delay=MAX(20u,(unsigned)u->desc[5]*2);
    memset(u->ic,0,sizeof u->ic);ctx(u,0)[1]=1;
    u32 *s=ctx(u,1);memcpy(s,u->outctx,cs);
    /* Default hub alternate is single-TT, including multi-TT capable hubs. */
    s[0]=(s[0]&~(1u<<25))|1u<<26;
    s[1]=(s[1]&0x00ffffffu)|(u->hub_ports<<24);
    if(u->speed==3)s[2]=(s[2]&~(3u<<16))|(u->hub_think<<16);
    s[3]=0;
    if(command(hc->version>0x95?12:13,(u64)u->ic,u->id)<0)return 0;
    for(unsigned p=1;p<=u->hub_ports;p++)if(control(u,0x23,3,8,p,0))return 0;
    usb_delay_ms(delay);
    device_add("USB2 hub (xHCI)","usb",0);
    return 2;
}
static int configure(struct Usb*u, int port, int speed)
{
    u->controller=hc;
    int id = command(9, 0, 0);
    if (id <= 0 || id > NS)
        return 0;
    u->id = id;
    u->port = port;
    u->speed = speed;
    dcbaa[id] = (u64)u->outctx;
    ringinit(&u->ctrl);
    ctx(u, 0)[1] = 3;
    u32 *s = ctx(u, 1);
    s[0] = u->route | ((u32)speed << 20) | (1u << 27);
    s[1] = (u32)port << 16;
    s[2] = u->tt_slot | (u->tt_port<<8); /* TTT is reserved except in HS hub contexts */
    int pkt = speed >= 4 ? 512 : speed == 3 ? 64 : 8;
    endpoint(ctx(u, 2), &u->ctrl, 4, pkt, 0);
    if (command(11, (u64)u->ic, id) < 0 || control(u, 128, 6, 0x100, 0, 8) || u->ctrl_bytes<8)
        return 0;
    if (speed <= 2 && u->desc[7] != pkt) {
        pkt = u->desc[7];
        if (pkt != 8 && pkt != 16 && pkt != 32 && pkt != 64)
            return 0;
        memset(u->ic, 0, sizeof u->ic);
        ctx(u, 0)[1] = 2;
        memcpy(ctx(u, 2), u->outctx + cs, cs);
        ctx(u, 2)[1] = (ctx(u, 2)[1] & 65535) | ((u32)pkt << 16);
        if (command(13, (u64)u->ic, id) < 0)
            return 0;
    }
    if(control(u,128,6,0x100,0,18)||u->ctrl_bytes<18)return 0;
    if(input_diag.device_count<TMC_INPUTDIAG_DEVICES) {
        u->diag=++input_diag.device_count;
        InputDeviceDiag *d=&input_diag.device[u->diag-1];
        *d=(InputDeviceDiag){.pci=hc->pci,.port=(u32)port,
            .vid=u->desc[8]|u->desc[9]<<8,.pid=u->desc[10]|u->desc[11]<<8,.status=1};
    }
    if (control(u, 128, 6, 0x200, 0, 9)||u->ctrl_bytes<9)
        return 0;
    int len = u->desc[2] | (u->desc[3] << 8);
    if (len < 9 || len > 1024 || control(u, 128, 6, 0x200, 0, len)||u->ctrl_bytes<len)
        return 0;
    unsigned cfg=0;int hub=0;HidInterface choices[NHID];
    int count=hid_configuration_at_speed(u->desc,len,speed==1?2:speed==2?1:speed==3?3:speed==4?5:6,choices,NHID,&cfg,&hub);
    if(count<0)return 0;if(hub)return hub_config(u,cfg);
    if(count<=0)return 0;
    memset(u->ic,0,sizeof u->ic);
    ctx(u,0)[1]=1;
    s=ctx(u,1);memcpy(s,u->outctx,cs);
    unsigned highest=1;
    for(int k=0;k<count;k++) {
        HidInterface *f=&choices[k];struct Hid *h=&u->hid[k];
        if(tmc_linux_generic_detach(h))return 0;
        tmc_linux_mouse_detach(h);tmc_linux_keyboard_detach(h);
        *h=(struct Hid){.ep=f->ep,.proto=f->proto,.mps=f->packet,.iface=f->iface,.report_len=f->report_length};
        ringinit(&h->in);
        ctx(u,0)[1]|=1u<<h->ep;
        if((unsigned)h->ep>highest)highest=h->ep;
        int iv=f->interval;
        int interval=speed>=3?MIN(15,iv-1):3;
        if(speed<3)while((iv>>=1)>0&&interval<15)interval++;
        endpoint(ctx(u,h->ep+1),&h->in,7,h->mps,interval);
    }
    s[0]=(s[0]&0x07ffffffu)|(highest<<27);s[3]=0;
    if(command(12,(u64)u->ic,id)<0||control(u,0,9,cfg,0,0))return 0;
    u->nhid=count;
    int usable=0;
    for(int k=0;k<count;k++) {
        struct Hid *h=&u->hid[k];int ok=1;
        /* Boot-capable mice start in the mandatory Boot format. A parsed
         * descriptor is retained but is not permission to force Report mode.
         * Report SET/GET success alone did not prove a usable IN stream. */
        if(h->proto==2){
            int boot_ok=select_protocol(u,h->iface,0);
            int parsed=h->report_len>0 && h->report_len<=(int)sizeof u->desc &&
                !control(u,0x81,6,0x2200,h->iface,h->report_len) && u->ctrl_bytes==h->report_len &&
                tmc_linux_hid_pointer_layout(u->desc,h->report_len,MIN(h->mps,64),&h->tablet);
            if(!boot_ok){
                if(parsed && select_protocol(u,h->iface,1)){h->proto=3;}
                else ok=0;
            }
        }
        if(h->proto==3){
            if((!h->report_len||h->report_len>(int)sizeof u->desc||
               control(u,0x81,6,0x2200,h->iface,h->report_len)||u->ctrl_bytes<h->report_len))ok=0;
            if(ok){
                h->linux_generic=tmc_linux_generic_attach(h,u->desc,h->report_len,h->mps,h->iface,linux_generic_control,u,boot.w,boot.h,hid_absolute);
                if(!h->linux_generic)ok=0;
            }
        }
        if(h->proto==1){
            if(!select_protocol(u,h->iface,0))ok=0;
            if(ok){u->desc[0]=0;control(u,0x21,9,0x0200,h->iface,1);}
        }
        if(!ok){h->proto=0;continue;}
        if(h->proto==2&&tmc_linux_mouse_boot_layout(&h->tablet)){
            unsigned vid=u->diag?input_diag.device[u->diag-1].vid:0;
            unsigned pid=u->diag?input_diag.device[u->diag-1].pid:0;
            h->linux_mouse=tmc_linux_mouse_attach(h,vid,pid,h->mps,10);
        }
        if(h->proto==1){
            unsigned vid=u->diag?input_diag.device[u->diag-1].vid:0;
            unsigned pid=u->diag?input_diag.device[u->diag-1].pid:0;
            h->linux_keyboard=tmc_linux_keyboard_attach(h,vid,pid,h->mps,10,h->iface,linux_keyboard_control,u);
        }
        const char *name=h->proto==1?"USB HID keyboard":h->proto==2?"USB HID mouse":"Linux generic HID input";
        device_add(name,"input",0);usable++;
        if(h->proto==1||(h->linux_generic&1))input_diag.keyboards++;
        if(h->proto==2||(h->linux_generic&2))input_diag.pointers++;
        if(u->diag)input_diag.device[u->diag-1].protocols|=1u<<h->proto;
    }
    if(u->diag)input_diag.device[u->diag-1].status=usable?2:1;
    return usable!=0;
}
static void arm(struct Usb *u,struct Hid *h)
{
    memset(h->report,0,sizeof h->report);
    push(&h->in,(u64)h->report,h->mps,TY(1)|32|4);
    ringbell(u->id,h->ep);h->armed=1;
}
static int waitbit(volatile u32*p, u32 m, int set)
{
    for(unsigned ms=0;ms<1000;ms++) {
        if(!!(*p&m)==set)return 1;
        usb_delay_ms(1);
    }
    return !!(*p&m)==set;
}
static void attach(struct Usb *,unsigned,unsigned,unsigned);
static int hub_status(struct Usb *u,unsigned p,unsigned *status)
{
    memset(u->desc,0,4);
    if(control(u,0xa3,0,0,p,4))return 0;
    *status=u->desc[0]|u->desc[1]<<8;return 1;
}
static void hub_children(struct Usb *u)
{
    if(u->depth>=5)return;
    for(unsigned p=1;p<=u->hub_ports && connected<NS;p++) {
        unsigned st;
        if(!hub_status(u,p,&st)||!(st&1))continue;
        usb_delay_ms(100); /* connect debounce, independent of CPU speed */
        if(!hub_status(u,p,&st)||!(st&1)||(st&8))continue;
        if(control(u,0x23,3,4,p,0))continue;
        int ready=0;
        for(unsigned ms=0;ms<500;ms+=10) {
            usb_delay_ms(10);
            if(!hub_status(u,p,&st)||!(st&1))break;
            if(!(st&16)&&(st&2)){ready=1;break;}
        }
        if(!ready)continue;
        for(unsigned f=16;f<=20;f++)(void)control(u,0x23,1,f,p,0);
        usb_delay_ms(10); /* reset recovery before Address Device */
        attach(u,p,st&1024?3:st&512?2:1,u->depth+1);
    }
}
static void attach(struct Usb *parent,unsigned port,unsigned speed,unsigned depth)
{
    if(connected>=NS||depth>5||!port||port>255)return;
    struct Usb *u=usb+connected;memset(u,0,sizeof *u);u->depth=depth;
    unsigned root=port;
    if(parent) {
        root=parent->port;u->route=parent->route|(MIN(port,15u)<<(4*(depth-1)));
        if(speed<3) {
            if(parent->speed==3){u->tt_slot=parent->id;u->tt_port=port;u->tt_think=parent->hub_think;}
            else {u->tt_slot=parent->tt_slot;u->tt_port=parent->tt_port;u->tt_think=parent->tt_think;}
        }
    }
    connected++; /* reserve parent slot before descending; never overwrite its DMA */
    int kind=configure(u,root,speed);
    if(kind==2)hub_children(u);
    else if(!kind) {
        if(u->id>0){
            if(command(10,0,u->id)<0){u->nhid=0;logstr("[usb] quarantined disabled-slot failure\n");return;}
            dcbaa[u->id]=0;
        }
        memset(u,0,sizeof *u);connected--;
    }
}
static void scan_root_ports(void)
{
    int first=connected;
    for (int i = 0; i < hc->ports && connected < NS; i++) {
        int owned=0;
        for(int j=0;j<connected;j++)if(usb[j].id>0 && !usb[j].depth && usb[j].port==i+1)owned=1;
        if(owned)continue; /* do not reset an enumerated hub/interface */
        volatile u32*p = op + 0x100 + 4 * i;
        u32 v = *p;
        if(hc->ppc && !(v&(1u<<9))) {*p=(v&0x0e000200u)|(1u<<9);usb_delay_ms(20);v=*p;}
        if (!(v & 1))
            continue;
        input_diag.root_devices++;
        int speed = (v >> 10) & 15;
        if (speed <= 3 || !(v & 2)) {
            *p = (v & (1u << 9)) | (1u << 4);
            if (!waitbit(p, 1 << 4, 0) || !(*p & 2))
                continue;
        }
        usb_delay_ms(10);
        attach(0,i+1,(*p>>10)&15,0);
    }
    /* Initial scan arms once at controller activation; recovery arms only
     * newly-enumerated devices, preserving existing DMA rings and contexts. */
    if(hc->active)for(int i=first;i<connected;i++)for(int j=0;j<usb[i].nhid;j++)
        if(usb[i].hid[j].proto)arm(usb+i,&usb[i].hid[j]);
}
static void controller_init(u32 dev)
{
    hc->pci=dev;
    volatile u32*c = dev ? pci_mmio(dev, 0) : 0;
    if (!c) {
        logstr("[usb] xHCI absent\n");
        return;
    }
    u32 h1 = c[1], h2 = c[2], cap = c[4];
    hc->ports=h1>>24;hc->ppc=!!(cap&8);
    hc->version=c[0]>>16;
    cs = (cap & 4) ? 64 : 32;
    op = (void*)((u8*)c + (c[0] & 255));
    db = (void*)((u8*)c + (c[5] & ~3u));
    ir = (void*)((u8*)c + (c[6] & ~31u) + 32);
    unsigned eoff = (cap >> 16);
    for (int k = 0; eoff && eoff < 0x10000 && k < 64; k++) {
        volatile u32 *ec = c + eoff;
        u32 h = *ec;
        if ((h & 255) == 1) {
            *ec = h | (1u << 24);
            if (!waitbit(ec, 1 << 16, 0))
                return;
            ec[1] = 0;
        }
        if (!(h & 0xff00))
            break;
        eoff += (h >> 8) & 255;
    }
    op[0] &= ~1u;
    if (!waitbit(op + 1, 1, 1))
        return;
    op[0] |= 2;
    usb_delay_ms(1); /* Intel reset recovery: do not immediately touch HC registers */
    if (!waitbit(op, 2, 0) || !waitbit(op + 1, 1 << 11, 0))
        return;
    int scratch = ((h2 >> 27) & 31) | ((h2 >> 16) & 0x3e0);
    if (scratch > 32)
        return;
    for (int i = 0; i < scratch; i++)
        if (!(sptr[i] = page_get()))
            return;
    if (scratch)
        dcbaa[0] = (u64)sptr;
    op[14] = MIN(NS, h1 & 255);
    w64(op + 12, (u64)dcbaa);
    ringinit(&commands);
    w64(op + 6, (u64)commands.tr | 1);
    evcycle = 1;
    erst.base = (u64)events;
    erst.size = NE;
    asm volatile("mfence":::"memory");
    ir[2]=1;
    w64(ir+4, (u64)&erst);
    w64(ir+6, (u64)events);
    op[0]=1;
    if (!waitbit(op+1, 1, 0))
        return;
    device_add("USB xHCI controller", "usb", 0);
    /* Only after a working xHC exists: switch known Intel shared ports before
     * probing EHCI. No board name matching and no writes to unknown vendors. */
    usb_route_intel_xhci(dev);
    usb_delay_ms(100);
    scan_root_ports();
    hc->active=1;input_diag.active_xhci++;
    for(int i=0;i<connected;i++)for(int j=0;j<usb[i].nhid;j++)
        if(usb[i].hid[j].proto)arm(usb+i,&usb[i].hid[j]);
}
void input_init(void)
{
    u32 dev=0;
    while((dev=pci_find_after(0x0c0330,dev))) {
        input_diag.xhci++;
        if(ncontrollers<NCONTROLLERS){hc=&controllers[ncontrollers++];controller_init(dev);}
    }
    const u32 classes[]={0x0c0320,0x0c0310,0x0c0300};
    u32 *counts[]={&input_diag.ehci,&input_diag.ohci,&input_diag.uhci};
    for(int i=0;i<3;i++){dev=0;while((dev=pci_find_after(classes[i],dev)))(*counts[i])++;}
    ehci_init(); /* route low/full-speed USB2 ports to their companions first */
    uhci_init();
    for(unsigned c=0;c<ncontrollers;c++) {
        hc=&controllers[c];
        if(hc->active && usb_route_intel_xhci_recover(hc->pci,input_diag.pointers,input_diag.keyboards))scan_root_ports();
    }
    ps2_init();
    logstr("[usb] native pointers ");lognum(input_diag.pointers);
    logstr("; active xHCI ");lognum(input_diag.active_xhci);logstr("\n");
}
// Control waits also dispatch unrelated input completions instead of losing them.
void kbd_setleds(int on)
{
    tmc_linux_keyboard_leds((unsigned)on);tmc_linux_generic_leds((unsigned)on);
    struct Xhci *saved=hc;
    for(unsigned c=0;c<ncontrollers;c++) {
        hc=&controllers[c];if(!hc->active)continue;
        for(int i=0;i<connected;i++)for(int j=0;j<usb[i].nhid;j++) {
            struct Hid *h=&usb[i].hid[j];
            if(h->proto==1&&!h->linux_keyboard){usb[i].desc[0]=on&31;control(usb+i,0x21,9,0x0200,h->iface,1);}
        }
    }
    hc=saved;
}
static void transferred(const TRB *e)
{
    int id=e->ctl>>24,ep=(e->ctl>>16)&31,code=e->len>>24;
    for(int i=0;i<connected;i++) {
        struct Usb *u=usb+i;if(u->id!=id)continue;
        for(int j=0;j<u->nhid;j++) {
            struct Hid *h=&u->hid[j];
            if(h->ep!=ep||!h->armed)continue;
            h->armed=0;
            if(code!=1&&code!=13) {
                input_diag.transfer_errors++;
                if(h->proto==1){const u8 release[8]={0};
                    if(h->linux_keyboard)(void)tmc_linux_keyboard_report(h,release,8);else hid_keys(release);}

                if(h->linux_generic)tmc_linux_generic_error(h,!((code==3||code==4||code==6)&&h->failures<3));
                if(h->linux_mouse){
                    int recoverable=(code==3||code==4||code==6)&&h->failures<3;
                    (void)tmc_linux_mouse_error(h,recoverable?(code==6?-32:-5):-108);
                }
                /* Only known halted states are recoverable here. Defer the
                 * bounded reset to the drain, never recurse inside result(). */
                if((code==3||code==4||code==6) && h->failures<3) {
                    h->pending=1;h->retry_at=ticks+100;h->failures++;
                }
                return;
            }
            h->failures=0; /* recovery budget is consecutive errors, not lifetime */
            input_diag.reports++;
            int n=h->mps-(int)(e->len&0xffffff);
            if(n<0||n>h->mps){input_diag.bad_reports++;arm(u,h);return;}
            if(h->proto==1&&n>=8){
                if(h->linux_keyboard)(void)tmc_linux_keyboard_report(h,h->report,n);
                else hid_keys(h->report);
            }
            if(h->proto==2&&n>=3){TabletState pt;
                if(!(h->linux_mouse&&tmc_linux_mouse_report(h,h->report,n))&&
                   pointer_boot_read(&h->tablet,h->report,n,boot.w,boot.h,&pt))hid_relative(pt.x,pt.y,pt.buttons,pt.wheel);
            }
            if(h->proto==3&&n>0&&h->linux_generic) {
                if(tmc_linux_generic_report(h,h->report,n))input_diag.bad_reports++;
            }
            arm(u,h);return;
        }
    }
}
static void recover_pending(void)
{
    for(int i=0;i<connected;i++)for(int j=0;j<usb[i].nhid;j++) {
        struct Usb *u=usb+i;struct Hid *h=&u->hid[j];
        if(!h->pending || ticks<h->retry_at)continue;
        h->pending=0;
        if(epcommand(14,u,h->ep,0)<0)continue;
        /* CLEAR_FEATURE(ENDPOINT_HALT), then discard the failed ring and
         * program its new dequeue/cycle before accepting further reports. */
        int addr=0x80|((unsigned)h->ep>>1);
        if(control(u,2,1,0,addr,0))continue;
        ringinit(&h->in);
        if(epcommand(16,u,h->ep,(u64)h->in.tr|1)<0)continue;
        arm(u,h);
    }
}
void input_irq(void)
{
    for(unsigned c=0;c<ncontrollers;c++) {
        hc=&controllers[c];if(!hc->active)continue;
        TRB e;
        for(int k=0;k<128&&event(&e);k++)if(((e.ctl>>10)&63)==32)transferred(&e);
        recover_pending();
    }
    ehci_poll();uhci_poll();ps2_poll();
    input_repeat();int leds=input_led_take();if(leds>=0){kbd_setleds(leds);ps2_setleds(leds&2);}
    tmc_linux_usb_service();tmc_linux_generic_service();
}
