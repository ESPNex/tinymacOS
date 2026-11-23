/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Actual Linux Boot mouse IRQ + core URB, including native error handoff.
 * This is not a hardware test or a replacement report decoder. */
#define main baseline_mouse_contract
#include "linux_usbmouse.c"
#undef main
int main(void)
{
 assert(!baseline_mouse_contract());
 int left,right;assert(tmc_linux_mouse_attach(&left,1,2,8,10));assert(tmc_linux_mouse_attach(&right,1,3,8,10));
 struct LxBinding *a=binding(&left),*b=binding(&right);
 check(&left,(u8[]){1,127,128,255},4,127,-128,1,-1);
 check(&right,(u8[]){0,128,127,1},4,-128,127,1,1);
 check(&right,(u8[]){2,0,0,0},4,0,0,3,0);
 check(&left,(u8[]){0,0,0,0},4,0,0,2,0);
 check(&left,(u8[]){2,0,0,0},4,0,0,2,0);
 check(&right,(u8[]){0,0,0,0},4,0,0,2,0);
 /* Disconnect a button owner while the other holds the same button. */
 check(&right,(u8[]){2,0,0,0},4,0,0,2,0);
 int count=events;tmc_linux_mouse_detach(&left);assert(events==count+1&&last_buttons==2&&!last_x&&!last_y&&!last_wheel);
 assert(tmc_linux_mouse_attach(&left,1,2,8,10));a=binding(&left);
 check(&left,(u8[]){1,0,0,0},4,0,0,3,0);
 check(&right,(u8[]){0,0,0,0},4,0,0,1,0);
 /* Transient hardware STALL: release only this device's held buttons,
  * no replay of deltas/wheel; the original IRQ resubmits for native reset. */
 count=events;assert(tmc_linux_mouse_error(&left,-EPIPE));assert(events==count+1&&!last_buttons&&!last_x&&!last_y&&!last_wheel&&a->urb->submitted);
 count=events;assert(tmc_linux_mouse_error(&left,-EIO));assert(events==count&&a->urb->submitted);
 check(&left,(u8[]){31,255,1,128},4,-1,1,31,-128);
 check(&right,(u8[]){1,0,0,0},4,0,0,31,0);
 assert(tmc_linux_mouse_error(&left,-EPIPE));assert(last_buttons==1&&!last_x&&!last_y&&!last_wheel&&a->urb->submitted);
 check(&left,(u8[]){16,0,0,0},4,0,0,17,0);
 assert(tmc_linux_mouse_error(&right,-ESHUTDOWN));assert(!b->urb->submitted&&last_buttons==16);
 count=events;assert(!tmc_linux_mouse_report(&right,(u8[]){1,3,4},3));assert(events==count);
 assert(!b->input->open(b->input));check(&right,(u8[]){0,0,0,0},4,0,0,16,0);
 assert(!tmc_linux_mouse_error(&left,0));assert(!tmc_linux_mouse_error(&left,1));assert(!tmc_linux_mouse_error(&left,-4096));
 assert(a->urb->submitted&&events==count+1);
 const int cancelled[]={-ENOENT,-ECONNRESET,-ESHUTDOWN};
 for(unsigned i=0;i<COUNT(cancelled);i++){
  check(&left,(u8[]){1,0,0,0},4,0,0,1,0);
  assert(tmc_linux_mouse_error(&left,cancelled[i]));assert(!a->urb->submitted&&!last_buttons&&!last_x&&!last_y&&!last_wheel);
  assert(!a->input->open(a->input));
 }
 /* Exhaust all five-button combinations while alternating report sources.
  * Every movement/wheel belongs exactly to the reporting device. */
 for(unsigned mask=0;mask<32;mask++)for(unsigned other=0;other<32;other++){
  unsigned held=(unsigned)b->input->buttons;
  check(&left,(u8[]){mask,127,128,255},4,127,-128,mask|held,-1);
  check(&right,(u8[]){other,128,127,1},4,-128,127,mask|other,1);
 }
 count=events;tmc_linux_mouse_detach(&left);assert(events==count+1&&last_buttons==31&&!last_x&&!last_y&&!last_wheel);
 tmc_linux_mouse_detach(&right);assert(!last_buttons&&!last_x&&!last_y&&!last_wheel&&!alive);
 assert(!tmc_linux_mouse_error(&right,-EPIPE));
 TmcLinuxUrbStats core;tmc_linux_urb_stats(&core);assert(!core.active&&core.submits==core.givebacks);
 TmcLinuxUsbStats stats;tmc_linux_mouse_stats(&stats);assert(stats.attached==stats.disconnected&&stats.allocations==stats.frees&&stats.errors==7);
 puts("PASS mouse lifecycle: 1024 paired five-button states, independent signed X/Y/wheel, idle/unplug button union, transient STALL/EIO release/requeue, fatal/unlink/cancel stop/reopen, invalid status rejection, no stale motion or leaks; actual Linux mouse IRQ/core");
 return 0;
}
