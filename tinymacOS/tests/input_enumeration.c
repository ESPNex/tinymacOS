/* Actual configure()/control() path, with emulated MMIO doorbells/descriptors.
 * This is a fault-injection contract, not evidence about a physical mouse. */
#define TMC_INPUT_TEST 1
#include "input_test_support.h"
static void test_doorbell(int,int);
#include "../drivers/input.c"
#include TMC_XHCI_TEST_SOURCE
u64 ticks;
void usb_delay_ms(unsigned ms){(void)ms;}
static u32 fakeir[16],fakedb[256];
static int acknowledge_without_switch;
static int boot_rejected,report_rejected,protocol,malformed,boot_requests,report_requests;
static const u8 report_desc[]={
 5,1,9,2,0xa1,1,0x85,4,9,1,0xa1,0,
 5,9,0x19,1,0x29,5,0x15,0,0x25,1,0x75,1,0x95,5,0x81,2,
 0x75,3,0x95,1,0x81,1,
 5,1,9,0x30,9,0x31,0x16,0x01,0x80,0x26,0xff,0x7f,0x75,16,0x95,2,0x81,6,
 9,0x38,0x15,0x81,0x25,0x7f,0x75,8,0x95,1,0x81,6,0xc0,0xc0
};
static const u8 cfg[]={9,2,34,0,1,1,0,0x80,50,
 9,4,0,0,1,3,1,2,0,9,0x21,0x11,1,0,1,0x22,sizeof report_desc,0,7,5,0x81,3,8,0,10};
static unsigned prev(unsigned i){return i?i-1:NR-2;}
static void complete(TRB *t,int type,int code,int ep,int slot)
{
    events[evpos]=(TRB){(u64)t,(u32)code<<24,TY(type)|((u32)ep<<16)|((u32)slot<<24)|(u32)evcycle};
}
static void test_doorbell(int slot,int ep)
{
    if(!slot){TRB *t=&commands.tr[prev(commands.at)];complete(t,33,1,0,1);return;}
    if(slot==1&&ep==3)return; /* Interrupt-IN rearm doorbell. */
    assert(slot==1&&ep==1);
    struct Usb *u=usb;unsigned at=prev(u->ctrl.at);TRB *t=&u->ctrl.tr[at];
    at=prev(at);if((u->ctrl.tr[at].ctl>>10&63)==3)at=prev(at);
    TRB *setup=&u->ctrl.tr[at];assert((setup->ctl>>10&63)==2);
    u64 v=setup->ptr;int rq=v>>8&255,val=v>>16&65535,n=v>>48;
    int code=1;
    if(rq==6) {
        if(val==0x100){memset(u->desc,0,18);u->desc[7]=64;u->desc[8]=0x34;u->desc[9]=0x12;u->desc[10]=0x78;u->desc[11]=0x56;}
        else if(val==0x200)memcpy(u->desc,cfg,n);
        else if(val==0x2200){memcpy(u->desc,report_desc,n);if(malformed)memset(u->desc,0,n);}
        else assert(0);
    } else if(rq==11) {
        if(!val){boot_requests++;if(boot_rejected)code=6;else protocol=0;}
        else {report_requests++;if(report_rejected)code=6;else if(!acknowledge_without_switch)protocol=1;}
    } else if(rq==3)u->desc[0]=protocol; /* GET_PROTOCOL */
    else assert(rq==9);
    complete(code==6?setup:t,32,code,1,slot);
}
static int run(int reject_boot,int reject_report,int current,int bad)
{
    memset(controllers,0,sizeof controllers);memset(&input_diag,0,sizeof input_diag);
    hc=controllers;connected=1;ir=fakeir;db=fakedb;evcycle=1;cs=32;ringinit(&commands);
    boot_rejected=reject_boot;report_rejected=reject_report;protocol=current;malformed=bad;
    boot_requests=report_requests=0;
    return configure(usb,1,3);
}
int main(void)
{
 Event e;TabletState s;u8 horizontal[]={4,0,64,0,0,0,0};
 /* Real Boot path; a valid Report descriptor must not make us switch formats. */
 assert(run(0,0,1,0));struct Hid *h=&usb[0].hid[0];
 assert(h->proto==2&&protocol==0&&boot_requests==1&&!report_requests);
 TRB ev={0,(13u<<24)|5,TY(32)|(1u<<24)|(3u<<16)};
 memcpy(h->report,(u8[]){1,37,0},3);h->armed=1;transferred(&ev);
 assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==37&&e.b==0&&e.c==1);
 memcpy(h->report,(u8[]){0,0,251},3);transferred(&ev);
 assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==0&&e.b==-5&&!e.c);
 puts("PASS xHCI Boot-first: valid Report descriptor does not change standard mouse format; X/Y and release");
 /* Report-only device / rejected Boot: retain ID, wide signed axes, wheel. */
 assert(run(1,0,1,0));h=&usb[0].hid[0];assert(h->proto==3&&boot_requests==1&&report_requests==1);
 assert(pointer_read(&h->tablet,horizontal,7,1280,800,&s)&&s.x==64&&!s.y);
 memcpy(h->report,horizontal,7);h->armed=1;ev.len=(13u<<24)|1;transferred(&ev);
 assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==64&&!e.b);
 horizontal[2]=0;horizontal[4]=64;memcpy(h->report,horizontal,7);transferred(&ev);
 assert(input_pop(&e)&&e.type==EV_MOUSE&&!e.a&&e.b==64);
 assert(!input_pop(&e));
 assert(run(1,1,1,0)&&usb[0].hid[0].proto==3);
 assert(run(1,1,0,0)&&usb[0].hid[0].proto==2);
 assert(!run(1,1,2,0)&&!input_diag.pointers);
 assert(!run(1,0,1,1)&&!input_diag.pointers);
 assert(run(0,0,1,1)&&usb[0].hid[0].proto==2);
 acknowledge_without_switch=1;assert(run(0,0,0,0)&&usb[0].hid[0].proto==2);
 acknowledge_without_switch=0;
 puts("PASS xHCI Report fallback verified, EP0 STALL recovery, malformed/unknown rejected, actual ID/16-bit X/Y completions retained");
 assert(run(0,0,1,0));hc->active=1;hc->ports=1;op=0;
 unsigned before=commands.at;int slots=connected;scan_root_ports();assert(commands.at==before&&connected==slots);
 puts("PASS owned root ports preserve DMA contexts without MMIO reset");
}
