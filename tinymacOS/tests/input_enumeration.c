/* Actual configure()/control() path, with emulated MMIO doorbells/descriptors.
 * This is a fault-injection contract, not evidence about a physical mouse. */
#define TMC_INPUT_TEST 1
#include "input_test_support.h"
static void test_doorbell(int,int);
#include "../drivers/input.c"
#include TMC_XHCI_TEST_SOURCE
u64 ticks;
static u32 fakeir[16],fakedb[256];
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
        else {report_requests++;if(report_rejected)code=6;else protocol=1;}
    } else if(rq==3)u->desc[0]=protocol; /* GET_PROTOCOL */
    else assert(rq==9);
    complete(code==6?setup:t,32,code,1,slot);
}
static int run(int reject_boot,int reject_report,int current,int bad)
{
    memset(controllers,0,sizeof controllers);memset(&input_diag,0,sizeof input_diag);
    hc=controllers;ir=fakeir;db=fakedb;evcycle=1;cs=32;ringinit(&commands);
    boot_rejected=reject_boot;report_rejected=reject_report;protocol=current;malformed=bad;
    boot_requests=report_requests=0;
    return configure(usb,1,3);
}
int main(void)
{
    assert(run(0,0,1,0));assert(usb[0].hid[0].proto==2&&protocol==0&&boot_requests==1&&!report_requests);
    assert(input_diag.pointers==1&&!input_diag.report_fallbacks);
    puts("PASS normal boot mouse: explicit Boot selected and boot decoder kept");
    assert(run(1,0,1,0));assert(usb[0].hid[0].proto==3&&usb[0].hid[0].tablet.id==4);
    assert(input_diag.pointers==1&&input_diag.control_errors==1&&input_diag.report_fallbacks==1);
    assert(input_diag.device[0].vid==0x1234&&input_diag.device[0].pid==0x5678);
    assert(run(1,1,1,0));assert(usb[0].hid[0].proto==3&&report_requests==1&&input_diag.control_errors==2);
    puts("PASS boot STALL: EP0 reset, report descriptor parsed, Report protocol set or GET_PROTOCOL verified");
    assert(!run(1,1,0,0));assert(!input_diag.pointers&&!input_diag.report_fallbacks);
    assert(!run(1,0,1,1));assert(!report_requests&&!input_diag.pointers);
    puts("PASS fail closed: unconfirmed Report protocol or malformed descriptor never decoded as boot bytes");
}
