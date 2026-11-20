#include "../kernel/k.h"
#include "hidtablet.h"
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
static u64 dcbaa[256] ALIGNED(64), sptr[32] ALIGNED(64);
static TRB events[NE] ALIGNED(4096);
static struct {
    u64 base;
    u32 size, res;
} erst ALIGNED(64);
static Ring commands;
static int evpos, evcycle, cs, connected;
static volatile u32 *op, *db, *ir;
static struct Usb {
    Ring ctrl, in;
    u8 outctx[32 * 64] ALIGNED(64), ic[33 * 64] ALIGNED(64), desc[1024] ALIGNED(64), report[64] ALIGNED(64);
    TabletLayout tablet;
    int id, port, speed, ep, proto, mps, armed, iface;
} usb[NS];
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
static int result(TRB*t, int ty, int slot)
{
    TRB e;
    for (int i = 0; i < 1500000; i++) {
        if (!event(&e)) {
            asm volatile("pause");
            continue;
        }
        if (((e.ctl>>10)&63)!=(u32)ty||e.ptr!=(u64)t)
            continue;
        int c=e.len>>24;
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
static int command(int ty, u64 ptr, int slot)
{
    TRB*t = push(&commands, ptr, 0, TY(ty) | ((u32)slot << 24));
    ringbell(0, 0);
    return result(t, 33, 1);
}
static int control(struct Usb*u, int rt, int rq, int val, int ix, int n)
{
    u64 s = (u64)rt | ((u64)rq << 8) | ((u64)val << 16) | ((u64)ix << 32) | ((u64)n << 48);
    int in = rt & 128;
    push(&u->ctrl, s, 8, TY(2) | 64 | ((n ? (in ? 3u : 2u) : 0u) << 16));
    if (n)
        push(&u->ctrl, (u64)u->desc, n, TY(3) | (in ? 1 << 16 : 0));
    TRB*t = push(&u->ctrl, 0, 0, TY(4) | 32 | ((n && in) ? 0 : 1 << 16));
    ringbell(u->id, 1);
    return result(t, 32, 0);
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
    ringinit(&u->in);
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
    if (control(u, 128, 6, 0x200, 0, 9))
        return 0;
    int len = u->desc[2] | (u->desc[3] << 8);
    if (len < 9 || len > 1024 || control(u, 128, 6, 0x200, 0, len))
        return 0;
    int cfg = u->desc[5], iface = 0, proto = 0, ep = 0, iv = 0, mps = 0, report_len = 0;
    for (int i = 0; i + 2 <= len;) {
        u8*p = u->desc + i;
        int z = p[0];
        if (z < 2 || i + z > len)
            return 0;
        if (p[1] == 4 && z >= 9) {
            iface = p[2];
            proto = 0; report_len = 0;
            if (!p[3] && p[5] == 3) {
                if (p[6] == 1 && (p[7] == 1 || p[7] == 2)) proto = p[7];
                else if (p[7] != 1) proto = 3; // candidate; validate the HID report descriptor below
            }
        }
        if (p[1] == 0x21 && z >= 6 && proto == 3) {
            if (p[5] > (z-6)/3) return 0;
            for (int j=0; j<p[5]; ++j) if (p[6+3*j] == 0x22) report_len = p[7+3*j] | p[8+3*j] << 8;
        }
        if (p[1] == 5 && z >= 7 && proto && (p[2] & 128) && (p[3] & 3) == 3) {
            ep = 2 * (p[2] & 15) + 1;
            mps = (p[4] | p[5] << 8) & 2047;
            iv = p[6];
            break;
        }
        i += z;
    }
    if (ep < 3 || ep > 31 || mps < 3 || mps > 64 || !iv)
        return 0;
    u->ep = ep;
    u->mps = mps;
    u->proto = proto;
    memset(u->ic, 0, sizeof u->ic);
    ctx(u, 0)[1] = 1 | (1u << ep);
    s = ctx(u, 1);
    memcpy(s, u->outctx, cs);
    s[0] = (s[0] & 0x07ffffffu) | ((u32)ep << 27);
    s[3] = 0;
    int interval = speed >= 3 ? MIN(15, iv - 1) : 3;
    if (speed < 3)
        while ((iv >>= 1) > 0 && interval < 15)
            interval++;
    endpoint(ctx(u, ep + 1), &u->in, 7, mps, interval);
    if (command(12, (u64)u->ic, id) < 0 || control(u, 0, 9, cfg, 0, 0)) return 0;
    const char *name;
    if (proto == 3) {
        // Non-boot HID: decode its declared relative/absolute report layout, including report ID.
        if (!report_len || report_len > (int)sizeof u->desc) return 0;
        memset(u->desc, 0, sizeof u->desc);
        if (control(u, 0x81, 6, 0x2200, iface, report_len) ||
            !pointer_layout(u->desc, report_len, mps, &u->tablet)) return 0;
        name=u->tablet.relative ? "USB HID report mouse" : "USB HID tablet";
    } else {
        if (control(u, 0x21, 11, 0, iface, 0)) return 0;
        u->iface = iface;
        if (proto == 1) {
            // Announce the boot LED state (Caps Lock off) so QEMU can keep
            // the host keyboard LED and Caps Lock in sync with the guest.
            u->desc[0] = 0;
            control(u, 0x21, 9, 0x0200, iface, 1);
        }
        name=proto==1 ? "USB HID keyboard" : "USB HID mouse";
    }
    device_add(name, "input", 0);
    return 1;
}
static void arm(struct Usb*u)
{
    memset(u->report, 0, 64);
    push(&u->in, (u64)u->report, u->mps, TY(1) | 32 | 4);
    ringbell(u->id, u->ep);
    u->armed = 1;
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
void input_init(void)
{
    u32 dev = pci_find(0x0c0330);
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
    for (int i = 0; i < connected; i++)
        arm(usb + i);
}
// Push the Caps Lock LED state to the virtual keyboard (HID output report,
// bit 1). QEMU tracks it and resyncs the host Caps Lock with synthetic key
// events, which is what keeps manual typing the right case in the window.
void kbd_setleds(int on)
{
    for (int i = 0; i < connected; i++) {
        struct Usb *u = usb + i;
        if (u->proto == 1 && u->id > 0) {
            u->desc[0] = on ? 2 : 0;
            control(u, 0x21, 9, 0x0200, u->iface, 1);
            return;
        }
    }
}
void input_irq(void)
{
    if (!connected)
        return;
    TRB e;
    for (int k = 0; k < 32 && event(&e); k++)
        if (((e.ctl >> 10) & 63) == 32) {
            int id = e.ctl >> 24, ep = (e.ctl >> 16) & 31, c = e.len >> 24;
            for (int j = 0; j < connected; j++) {
                struct Usb*u = usb + j;
                if (u->id != id || u->ep != ep || !u->armed)
                    continue;
                u->armed = 0;
#ifdef TMC_DEBUG
                {   /* completion forensics: code + residual on every event */
                    char _b[36]; int _o = 0;
                    static const char _h[] = "0123456789abcdef";
                    _b[_o++]='['; _b[_o++]='u'; _b[_o++]='s'; _b[_o++]='b';
                    _b[_o++]='c'; _b[_o++]='=';
                    _b[_o++] = _h[(c >> 4) & 15]; _b[_o++] = _h[c & 15];
                    _b[_o++]='n'; _b[_o++]='=';
                    { int _n = u->mps - (int)(e.len & 0xffffff);
                      if (_n < 0) _n = 0;
                      if (_n > 99) _n = 99;
                      _b[_o++] = '0' + _n / 10; _b[_o++] = '0' + _n % 10; }
                    _b[_o++] = ']'; _b[_o++] = 0;
                    logstr(_b);
                }
#endif
                if (c != 1 && c != 13)
                    break;
                int n = u->mps - (int)(e.len & 0xffffff);
                if (u->proto == 1 && n >= 8) {
#ifdef TMC_DEBUG
                    {   /* raw boot-keyboard report forensics */
                        char _b[40]; int _j, _o = 0;
                        _b[_o++]='['; _b[_o++]='u'; _b[_o++]='s'; _b[_o++]='b';
                        _b[_o++]='r'; _b[_o++]='e'; _b[_o++]='p'; _b[_o++]=' ';
                        for (_j = 0; _j < 8 && _o < 36; _j++) {
                            static const char _h[] = "0123456789abcdef";
                            _b[_o++] = _h[u->report[_j] >> 4];
                            _b[_o++] = _h[u->report[_j] & 15];
                        }
                        _b[_o++] = ']'; _b[_o++] = 0;
                        logstr(_b);
                    }
#endif
                    hid_keys(u->report);
                }
                if (u->proto == 2 && n >= 3)
                    hid_mouse(u->report);
                if (u->proto == 3 && n > 0) {
                    TabletState pt;
                    if (pointer_read(&u->tablet,u->report,(unsigned)n,boot.w,boot.h,&pt)) {
                        if (u->tablet.relative) hid_relative(pt.x, pt.y, pt.buttons, pt.wheel);
                        else hid_absolute(pt.x, pt.y, pt.buttons, pt.wheel);
                    }
                }
                arm(u);
                break;
            }
        }
    input_repeat();
    int lv = input_led_take();
    if (lv >= 0)kbd_setleds(lv);
}
