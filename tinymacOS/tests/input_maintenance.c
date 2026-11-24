/* Executes real queue/completion/recovery code against host memory, NOT USB hardware.
 * No privileged instructions; DMA completions injected as ordinary xHCI events. */
#define TMC_INPUT_TEST 1
#include "../drivers/input.c"
#include "../drivers/xhci.c"
u64 ticks;
void usb_delay_ms(unsigned ms){(void)ms;}
static u32 fakeir[16],fakedb[256];
static void fixture(void)
{
    if(hc)assert(!tmc_linux_core_control_detach(usb));
    memset(controllers,0,sizeof controllers);hc=controllers;
    ir=fakeir;db=fakedb;evcycle=1;connected=1;
    usb[0].id=1;usb[0].controller=hc;usb[0].packet0=8;usb[0].speed=1;ringinit(&commands);ringinit(&usb[0].ctrl);
    usb[0].nhid=1;usb[0].hid[0]=(struct Hid){.proto=2,.ep=3,.mps=8};
    ringinit(&usb[0].hid[0].in);ticks=0;
}
static void completion(int at,TRB *t,int ty,int code,int ep)
{
    events[at]=(TRB){(u64)t,(u32)code<<24,TY(ty)|(1u<<24)|((u32)ep<<16)|1};
}
static Event take(int type,int x,int y,int btn)
{
    Event e;assert(input_pop(&e));assert(e.type==type&&e.a==x&&e.b==y&&e.c==btn);return e;
}
int main(void)
{
    for(int i=0;i<10000;i++)hid_relative(1,-1,0,0);
    take(EV_MOUSE,10000,-10000,0);Event e;assert(!input_pop(&e));assert(input_diag.merged==9999);
    hid_relative(5,2,0,0);hid_relative(0,0,1,0);hid_relative(9,3,1,0);
    hid_relative(0,0,0,0);hid_relative(7,4,0,1);hid_relative(2,1,0,0);
    take(EV_MOUSE,5,2,0);take(EV_MOUSE,0,0,1);take(EV_MOUSE,9,3,1);
    take(EV_MOUSE,0,0,0);take(EV_MOUSE,7,4,0);
    take(EV_SCROLL,1,0,0);take(EV_MOUSE,2,1,0);assert(!input_pop(&e));
    hid_absolute(10,20,0,0);hid_absolute(30,40,0,0);take(EV_POINTER_ABS,30,40,0);
    hid_absolute(10,20,0,1);hid_absolute(30,40,0,0);
    assert(take(EV_POINTER_ABS,10,20,0).d==1);take(EV_POINTER_ABS,30,40,0);
    for(int i=0;i<130;i++)put((Event){.type=EV_KEY,.a=i});
    assert(input_diag.dropped==3);for(int i=0;i<127;i++)take(EV_KEY,i,0,0);
    puts("PASS queue: 10000 motion reports, exact sum, buttons/wheel/key barriers, absolute newest, full-queue counter");
    fixture();struct Usb *u=usb;struct Hid *h=&u->hid[0];
    assert((u64)h->report%1024==0 && (u64)u->desc%1024==0);arm(u,h);
    h->report[0]=1;h->report[1]=255;h->report[2]=127;
    TRB ok={0,1u<<24|5,TY(32)|(1u<<24)|(3u<<16)};
    transferred(&ok);take(EV_MOUSE,-1,127,1);assert(h->armed);
    ok.len=13u<<24|5;transferred(&ok);take(EV_MOUSE,0,0,0);assert(h->armed);
    puts("PASS completion: signed boot motion, button decoding, success/short packet rearm, aligned DMA");
    TRB stall={0,6u<<24,TY(32)|(1u<<24)|(3u<<16)};
    transferred(&stall);assert(!h->armed&&h->pending&&h->failures==1);
    recover_pending();assert(h->pending);ticks=100;
    completion(0,&commands.tr[0],33,1,0);
    completion(1,&u->ctrl.tr[1],32,1,1);
    completion(2,&commands.tr[1],33,1,0);
    recover_pending();assert(!h->pending&&h->armed&&evpos==3);
    assert((commands.tr[0].ctl>>10&63)==14 && (commands.tr[1].ctl>>10&63)==16);
    assert((commands.tr[0].ctl>>16&31)==3);
    assert((u->ctrl.tr[0].ptr&0xffffffffull)==0x00000102); /* CLEAR_FEATURE, recipient endpoint */
    /* Independent intermittent errors must not exhaust a lifetime budget. */
    for(int episode=0;episode<12;episode++){
        h->failures=3;ok.len=13u<<24|5;transferred(&ok);take(EV_MOUSE,0,0,0);
        assert(h->armed&&!h->failures);
    }
    h->failures=3;transferred(&stall);assert(!h->armed&&!h->pending);
    puts("PASS endpoint halt: deferred reset, clear-halt, new dequeue, rearm; bounded repeated-failure cap");
    /* Same controller completion/reset path, now with the REAL upstream
     * Linux mouse and URB core attached, not a mouse decoder substitute. */
    fixture();u=usb;h=&u->hid[0];h->linux_mouse=tmc_linux_mouse_attach(h,1,2,8,10);assert(h->linux_mouse);
    arm(u,h);h->report[0]=1;h->report[1]=12;h->report[2]=254;
    ok.len=1u<<24|5;transferred(&ok);take(EV_MOUSE,12,-2,1);
    transferred(&stall);take(EV_MOUSE,0,0,0);assert(h->pending&&!h->armed);
    ticks=100;completion(0,&commands.tr[0],33,1,0);completion(1,&u->ctrl.tr[1],32,1,1);completion(2,&commands.tr[1],33,1,0);
    recover_pending();assert(h->armed&&!h->pending);
    h->report[0]=1;h->report[1]=253;h->report[2]=6;transferred(&ok);take(EV_MOUSE,-3,6,1);
    h->failures=3;transferred(&stall);take(EV_MOUSE,0,0,0);assert(!h->pending&&!h->armed);
    TmcLinuxUrbStats core;tmc_linux_urb_stats(&core);assert(!core.active&&core.submits==core.givebacks);
    tmc_linux_mouse_detach(h);assert(!input_pop(&e));
    puts("PASS actual xHCI+Linux mouse: held-button STALL release, upstream error/requeue, native reset/rearm, resumed XY, fatal retirement");
    for(int data=0;data<2;data++) {
        fixture();u=usb;
        completion(0,&u->ctrl.tr[data?1:0],32,6,1); /* error on Setup or Data, not Status */
        completion(1,&commands.tr[0],33,1,0);completion(2,&commands.tr[1],33,1,0);
        assert(control(u,0x81,6,0x2200,0,8)==-1);
        assert(!u->ctrl_dead&&evpos==3&&u->ctrl.at==0);
        completion(3,&u->ctrl.tr[1],32,1,1);
        assert(control(u,0x21,11,1,0,0)==0);
    }
    puts("PASS control stall on Setup/Data: consume error, reset EP0, set dequeue, next protocol request succeeds");
    fixture();u=usb;
    completion(0,&u->ctrl.tr[1],32,13,1);events[0].len|=3;
    completion(1,&u->ctrl.tr[2],32,1,1);
    assert(control(u,0x80,6,0x100,0,8)==0 && u->ctrl_bytes==5 && evpos==2 && !u->ctrl_dead);
    assert(u->ctrl.tr[1].ctl&4); /* ISP: short Data event before Status */
    puts("PASS actual xHCI short control: actual length from Data, completion only after Status, IN ISP set");
    fixture();u=usb;
    assert(control(u,0x21,11,0,0,0)==-1 && u->ctrl_dead);
    unsigned at=u->ctrl.at;assert(control(u,0x21,11,1,0,0)==-1 && u->ctrl.at==at);
    puts("PASS timeout: no unsafe recycling of potentially live control DMA");
    InputDiag d;input_diagnostics(&d);assert(d.version==1&&d.bytes==sizeof d);
    puts("PASS diagnostic snapshot ABI; no keyboard content or DMA pointers exposed");
}
