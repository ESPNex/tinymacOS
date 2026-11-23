/* Host register contract: this does not emulate Bay Trail electrical routing. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "base.h"
#define TMC_USBROUTE_TEST 1
static int has_ehci;
static u32 pci_find_after(u32 cls,u32 after){assert(cls==0x0c0320&&!after);return has_ehci?0x8000e800u:0;}
static u32 regs[64],selected,writes[8],values[8];static unsigned nw,delay,logs;
static int locked,inaccessible_after_write;
static u32 pci_reg(u32 dev,int off){(void)dev;return regs[off/4];}
static void outl(unsigned port,u32 v){if(port==0xcf8)selected=v&255;else {assert(port==0xcfc&&nw<8);writes[nw]=selected;values[nw++]=v;if(!locked)regs[selected/4]=inaccessible_after_write?~0u:v;}}
static void usb_delay_ms(unsigned ms){delay+=ms;}
static void logstr(const char*s){(void)s;logs++;}
#include "../drivers/usbroute.c"
static void init(u32 id){memset(regs,0,sizeof regs);nw=delay=logs=0;locked=inaccessible_after_write=0;regs[0]=id;regs[2]=0x0c033000;regs[0xd4/4]=0x15;regs[0xdc/4]=2;regs[0xd0/4]=0x80;regs[0xd8/4]=4;}
int main(void)
{
 init(0x0f358086);usb_route_intel_xhci(0x8000a000);
 assert(nw==2&&writes[0]==0xd8&&values[0]==6&&writes[1]==0xd0&&values[1]==0x95&&delay==100);
 has_ehci=1;init(0x0f358086);usb_route_intel_xhci(0x8000a000);
 assert(nw==2&&writes[0]==0xd8&&writes[1]==0xd0&&regs[0xd0/4]==0x95&&delay==100);
 puts("PASS 21B9 regression: shared Intel USB2 routed before enumeration even when EHCI exists; only documented mask bits changed");
 has_ehci=0;
 puts("PASS Intel Bay Trail: mask-limited USB3 then USB2 routing, preserves preexisting ports, bounded settling");
 const u32 families[]={0x0f35,0x1e31,0x8c31,0x8cb1,0x9c31,0x9cb1};
 for(unsigned i=0;i<sizeof families/sizeof families[0];i++){
  init(families[i]<<16|0x8086);usb_route_intel_xhci(0x8000a000);
  assert(nw==2&&writes[0]==0xd8&&writes[1]==0xd0&&logs==1);
  nw=delay=logs=0;usb_route_intel_xhci(0x8000a000);assert(!nw&&logs==1);
 }
 init(0x0f358086);regs[0xd0/4]=~0u;usb_route_intel_xhci(0x8000a000);assert(!nw&&!delay&&!logs);
 init(0x0f358086);regs[0xd8/4]=~0u;usb_route_intel_xhci(0x8000a000);assert(!nw&&!delay&&!logs);
 init(0x0f358086);regs[0xdc/4]=0;regs[0xd8/4]=~0u;usb_route_intel_xhci(0x8000a000);assert(nw==1&&writes[0]==0xd0&&logs==1);
 init(0x0f358086);locked=1;usb_route_intel_xhci(0x8000a000);assert(nw==2&&regs[0xd0/4]==0x80&&!logs);
 init(0x0f358086);inaccessible_after_write=1;usb_route_intel_xhci(0x8000a000);assert(nw==2&&!logs);
 puts("PASS six shared-mux controller families; already-routed no-op; inaccessible current registers cause zero writes; locked/failed readback never reports success");
 const u32 skip[]={0x00011b36,0x0f351234,0xffff8086};
 for(unsigned i=0;i<3;i++){init(skip[i]);usb_route_intel_xhci(0x8000a000);assert(!nw&&!delay);}
 init(0x0f358086);regs[0xd4/4]=~0u;usb_route_intel_xhci(0x8000a000);assert(!nw);
 init(0x0f358086);regs[2]=0x0c032000;usb_route_intel_xhci(0x8000a000);assert(!nw);
 init(0x1e318086);regs[0x2c/4]=0x90a8104d;usb_route_intel_xhci(0x8000a000);assert(!nw);
 has_ehci=1;init(0x0f358086);assert(!usb_route_intel_xhci_recover(0x8000a000,1,0)&&!nw);
 assert(!usb_route_intel_xhci_recover(0x8000a000,0,1)&&!nw);
 assert(usb_route_intel_xhci_recover(0x8000a000,0,0));
 assert(nw==1&&writes[0]==0xd0&&values[0]==0x95&&delay==100&&regs[0xd8/4]==4);
 assert(!usb_route_intel_xhci_recover(0x8000a000,0,0)&&nw==1);
 init(0x0f351234);assert(!usb_route_intel_xhci_recover(0x8000a000,0,0)&&!nw);
 init(0x0f358086);regs[0xd4/4]=~0u;assert(!usb_route_intel_xhci_recover(0x8000a000,0,0)&&!nw);
 init(0x0f358086);regs[0xd0/4]=~0u;assert(!usb_route_intel_xhci_recover(0x8000a000,0,0)&&!nw);
 init(0x0f358086);regs[0xd4/4]=0;assert(!usb_route_intel_xhci_recover(0x8000a000,0,0)&&!nw);
 has_ehci=0;init(0x0f358086);assert(!usb_route_intel_xhci_recover(0x8000a000,0,0)&&!nw);
 puts("PASS empty-HID fallback: only switchable masked USB2 bits, one retry; any live pointer or keyboard blocks remux; unknown/all-ones/zero mask/no companion rejected");
 puts("PASS routing guard: unknown/foreign/non-xHCI/all-ones registers and known non-switchable subsystem receive no writes");
}
