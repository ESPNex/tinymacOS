#ifdef TMC_INPUT_TEST
#include "../tests/input_test_support.h"
#else
#include "../kernel/k.h"
#endif
#include "hidtablet.h"
#include "hidconfig.h"
// Root-port HID: boot keyboard, boot relative mouse, and report-protocol absolute tablet.
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
    int ep,proto,mps,armed,iface,report_len,failures,pending;
    u64 retry_at;
};
struct Usb {
    Ring ctrl;
    u8 outctx[32*64] ALIGNED(64),ic[33*64] ALIGNED(64);
    u8 desc[1024] ALIGNED(1024); /* Data Stage TRBs have the same 64 KiB boundary rule */
    struct Hid hid[NHID];
    int id,port,speed,nhid,diag,ctrl_dead;
};
static struct Xhci {
    u64 dcbaa[256] ALIGNED(64),sptr[32] ALIGNED(64);
    TRB events[NE] ALIGNED(4096);
    struct {u64 base;u32 size,res;} erst ALIGNED(64);
    Ring commands;
    int evpos,evcycle,cs,connected,active,lastcode;
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
static TRB* push(Ring*r, u64 addr, u32 len, u32 ctrl)
{
    TRB*t = r->tr + r->at;
    t->ptr = addr;
    t->len = len;
    asm volatile("sfence":::"memory");
    t->ctl=ctrl|r->cycle;
    if (++r->at == NR - 1) {
        r->tr[NR - 1].ctl = TY(6) | 2 | r->cycle;
        r->cycle ^= 1;
        r->at = 0;
    }
    return t;
}
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
    for (int i = 0; i < 1500000; i++) {
        if (!event(&e)) {
            asm volatile("pause");
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
    int in = rt & 128;
    TRB *setup=push(&u->ctrl, s, 8, TY(2) | 64 | ((n ? (in ? 3u : 2u) : 0u) << 16));
    TRB *data=0;
    if (n)
        data=push(&u->ctrl, (u64)u->desc, n, TY(3) | (in ? 1 << 16 : 0));
    TRB*t = push(&u->ctrl, 0, 0, TY(4) | 32 | ((n && in) ? 0 : 1 << 16));
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
static int configure(struct Usb*u, int port, int speed)
{
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
    s[0] = ((u32)speed << 20) | (1u << 27);
    s[1] = (u32)port << 16;
    int pkt = speed >= 4 ? 512 : speed == 3 ? 64 : 8;
    endpoint(ctx(u, 2), &u->ctrl, 4, pkt, 0);
    if (command(11, (u64)u->ic, id) < 0 || control(u, 128, 6, 0x100, 0, 8))
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
    if(control(u,128,6,0x100,0,18))return 0;
    if(input_diag.device_count<TMC_INPUTDIAG_DEVICES) {
        u->diag=++input_diag.device_count;
        InputDeviceDiag *d=&input_diag.device[u->diag-1];
        *d=(InputDeviceDiag){.pci=hc->pci,.port=(u32)port,
            .vid=u->desc[8]|u->desc[9]<<8,.pid=u->desc[10]|u->desc[11]<<8,.status=1};
    }
    if (control(u, 128, 6, 0x200, 0, 9))
        return 0;
    int len = u->desc[2] | (u->desc[3] << 8);
    if (len < 9 || len > 1024 || control(u, 128, 6, 0x200, 0, len))
        return 0;
    int cfg=u->desc[5];
    HidInterface choices[NHID];
    int count=hid_interfaces(u->desc,len,choices,NHID);
    if(count<=0)return 0;
    memset(u->ic,0,sizeof u->ic);
    ctx(u,0)[1]=1;
    s=ctx(u,1);memcpy(s,u->outctx,cs);
    unsigned highest=1;
    for(int k=0;k<count;k++) {
        HidInterface *f=&choices[k];struct Hid *h=&u->hid[k];
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
        if(h->proto==3) {
            if(!h->report_len||h->report_len>(int)sizeof u->desc)ok=0;
            else {
                memset(u->desc,0,sizeof u->desc);
                if(control(u,0x81,6,0x2200,h->iface,h->report_len)||
                   !pointer_layout(u->desc,h->report_len,MIN(h->mps,64),&h->tablet))ok=0;
            }
        } else {
            if(control(u,0x21,11,0,h->iface,0)) {
                ok=0;
                /* Some mice advertise Boot but reject it. Never decode a
                 * report-ID/16-bit layout as three boot bytes. Require a
                 * valid report descriptor AND confirmed Report protocol. */
                if(h->proto==2 && !u->ctrl_dead && h->report_len>0 && h->report_len<=1024 &&
                   !control(u,0x81,6,0x2200,h->iface,h->report_len) &&
                   pointer_layout(u->desc,h->report_len,MIN(h->mps,64),&h->tablet)) {
                    int report=!control(u,0x21,11,1,h->iface,0);
                    if(!report && !u->ctrl_dead) {
                        report=!control(u,0xa1,3,0,h->iface,1) && u->desc[0]==1;
                    }
                    if(report){h->proto=3;ok=1;input_diag.report_fallbacks++;}
                }
            }
            if(ok&&h->proto==1) {u->desc[0]=0;control(u,0x21,9,0x0200,h->iface,1);}
        }
        if(!ok){h->proto=0;continue;}
        const char *name=h->proto==1?"USB HID keyboard":h->proto==2?"USB HID mouse":
            h->tablet.relative?"USB HID report mouse":"USB HID tablet";
        device_add(name,"input",0);usable++;
        if(h->proto==1)input_diag.keyboards++;else input_diag.pointers++;
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
    for (int i = 0; i < 4000000; i++) {
        if (!!(*p & m) == set)
            return 1;
        asm volatile("pause");
    }
    return 0;
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
    for (int i = 0; i < (int)(h1 >> 24) && connected < NS; i++) {
        volatile u32*p = op + 0x100 + 4 * i;
        u32 v = *p;
        if (!(v & 1))
            continue;
        input_diag.root_devices++;
        int speed = (v >> 10) & 15;
        if (speed <= 3 || !(v & 2)) {
            *p = (v & (1u << 9)) | (1u << 4);
            if (!waitbit(p, 1 << 4, 0) || !(*p & 2))
                continue;
        }
        if (configure(usb + connected, i + 1, (*p >> 10) & 15))
            connected++;
        else {
            if (usb[connected].id > 0) {
                command(10, 0, usb[connected].id);
                dcbaa[usb[connected].id] = 0;
            }
            logstr("[usb] skipped root port ");
            lognum(i + 1);
            logstr("\n");
            memset(usb + connected, 0, sizeof usb[0]);
        }
    }
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
    ps2_init();
    logstr("[usb] native pointers ");lognum(input_diag.pointers);
    logstr("; active xHCI ");lognum(input_diag.active_xhci);logstr("\n");
}
// Control waits also dispatch unrelated input completions instead of losing them.
void kbd_setleds(int on)
{
    struct Xhci *saved=hc;
    for(unsigned c=0;c<ncontrollers;c++) {
        hc=&controllers[c];if(!hc->active)continue;
        for(int i=0;i<connected;i++)for(int j=0;j<usb[i].nhid;j++) {
            struct Hid *h=&usb[i].hid[j];
            if(h->proto==1){usb[i].desc[0]=on?2:0;control(usb+i,0x21,9,0x0200,h->iface,1);}
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
                /* Only known halted states are recoverable here. Defer the
                 * bounded reset to the drain, never recurse inside result(). */
                if((code==3||code==4||code==6) && h->failures<3) {
                    h->pending=1;h->retry_at=ticks+100;h->failures++;
                }
                return;
            }
            input_diag.reports++;
            int n=h->mps-(int)(e->len&0xffffff);
            if(n<0||n>h->mps){input_diag.bad_reports++;arm(u,h);return;}
            if(h->proto==1&&n>=8)hid_keys(h->report);
            if(h->proto==2&&n>=3)hid_relative((int8_t)h->report[1],(int8_t)h->report[2],h->report[0]&7,n>3?(int8_t)h->report[3]:0);
            if(h->proto==3&&n>0) {
                TabletState pt;
                if(pointer_read(&h->tablet,h->report,(unsigned)n,boot.w,boot.h,&pt)) {
                    if(h->tablet.relative)hid_relative(pt.x,pt.y,pt.buttons,pt.wheel);
                    else hid_absolute(pt.x,pt.y,pt.buttons,pt.wheel);
                }
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
}
