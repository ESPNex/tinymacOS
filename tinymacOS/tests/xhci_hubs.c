/* Real attach/configure/hub_children path, mocked command MMIO completions.
 * Tests slot/route/TT invariants; not evidence about a physical controller. */
#define TMC_INPUT_TEST 1
#include "input_test_support.h"
static void test_doorbell(int,int);
#include "../drivers/input.c"
#include TMC_XHCI_TEST_SOURCE
u64 ticks;
void usb_delay_ms(unsigned ms){(void)ms;}
static u32 fakeir[16],fakedb[256];
static unsigned allocated,eval_hub,child_addressed,mode,reset_poll;
static const u8 mousecfg[]={9,2,25,0,1,1,0,0x80,50,9,4,0,0,1,3,1,2,0,7,5,0x81,3,4,0,10};
static const u8 hubcfg[]={9,2,25,0,1,1,0,0xe0,0,9,4,0,0,1,9,0,0,0,7,5,0x81,3,1,0,12};
static unsigned prev(unsigned i){return i?i-1:NR-2;}
static void complete(TRB*t,int type,int code,int ep,int slot)
{events[evpos]=(TRB){(u64)t,(u32)code<<24,TY(type)|((u32)ep<<16)|((u32)slot<<24)|(u32)evcycle};}
static void test_doorbell(int slot,int ep)
{
 if(!slot){
  TRB*t=&commands.tr[prev(commands.at)];unsigned ty=t->ctl>>10&63,id=t->ctl>>24;
  if(ty==9){id=++allocated;assert(id<=NS);}
  if(ty==11||ty==12||ty==13){
   struct Usb*u=0;for(int i=0;i<connected;i++)if(usb[i].id==(int)id)u=usb+i;assert(u);
   if(ty==11 && id>1){
    assert(ctx(u,1)[1]>>16==2);
    if(id==2){assert((ctx(u,1)[0]&0xfffff)==3);assert((ctx(u,1)[2]&0x3ffff)==0x301);}
    if(id==3){assert((ctx(u,1)[0]&0xfffff)==0x33);assert((ctx(u,1)[2]&0x3ffff)==0x301);}
    child_addressed++;
   }
   if((ty==12||ty==13) && (ctx(u,1)[0]&(1u<<26))){assert(ctx(u,1)[1]>>24==4);assert(ty==(hc->version>0x95?12u:13u));eval_hub++;}
   memcpy(u->outctx,u->ic+cs,cs*2);
  }
  complete(t,33,1,0,id);return;
 }
 assert(ep==1);struct Usb*u=0;for(int i=0;i<connected;i++)if(usb[i].id==slot)u=usb+i;assert(u);
 unsigned at=prev(u->ctrl.at);TRB*t=&u->ctrl.tr[at];at=prev(at);if((u->ctrl.tr[at].ctl>>10&63)==3)at=prev(at);
 TRB*setup=&u->ctrl.tr[at];assert((setup->ctl>>10&63)==2);
 u64 v=setup->ptr;int rt=v&255,rq=v>>8&255,val=v>>16&65535,ix=v>>32&65535,n=v>>48;
 int ishub=slot==1||(mode==1&&slot==2);
 if(rq==6){
  if(val==0x100){memset(u->desc,0,18);u->desc[0]=18;u->desc[1]=1;u->desc[7]=ishub?64:8;}
  else if(val==0x200)memcpy(u->desc,ishub?hubcfg:mousecfg,n);
  else if(val==0x2900){const u8 d[]={9,0x29,4,0x69,0,50,0,0,255};memcpy(u->desc,d,n);if(mode==2)u->desc[2]=0;}
  else assert(0);
 }else if(rq==0&&rt==0xa3){
  memset(u->desc,0,4);
  if(ix==3){u->desc[0]=1|((reset_poll++>1)?2:16);if(mode==4)u->desc[1]=2;}
 }else if(rq==3&&rt==0x23){assert(val==8||val==4);if(val==4)reset_poll=0;}
 else if(rq==1&&rt==0x23)assert(val>=16&&val<=20);
 else if(rq==3&&rt==0xa1)u->desc[0]=0; /* Boot-mode readback. */
 else assert(rq==9||rq==11);
 complete(t,32,1,1,slot);
}
static void run(unsigned m)
{memset(controllers,0,sizeof controllers);hc=controllers;ir=fakeir;db=fakedb;evcycle=1;cs=32;hc->version=m==3?0x95:0x100;ringinit(&commands);allocated=eval_hub=child_addressed=0;mode=m;attach(0,2,3,0);}
int main(void)
{
 run(0);assert(connected==2&&eval_hub==1&&child_addressed==1);
 assert(usb[0].hub_ports==4&&usb[1].hid[0].proto==2&&usb[1].tt_slot==1&&usb[1].tt_port==3&&usb[1].tt_think==3);
 puts("PASS xHCI HS hub -> FS mouse: distinct reserved slots, root port, route string, TT slot/port and think-time, delayed hub reset polling");
 run(1);assert(connected==3&&eval_hub==2&&child_addressed==2);
 assert(usb[2].route==0x33&&usb[2].tt_slot==1&&usb[2].tt_port==3);
 puts("PASS nested USB2 hub: route nibbles and nearest high-speed TT inherited through full-speed hub");
 run(3);assert(connected==2&&eval_hub==1);
 run(4);assert(connected==2&&usb[1].speed==2&&usb[1].tt_slot==1);
 puts("PASS xHCI 0.95 Evaluate vs 1.0 Configure hub command; low-speed child with TT, reserved child think-time bits zero");
 run(2);assert(!connected&&!child_addressed);
 puts("PASS malformed zero-port hub rejected; no child enumeration and slot released");
}
