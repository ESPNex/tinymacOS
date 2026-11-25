/* Shipping kernel/log.c + real splash rasterizer, only IRQ-safe lock imports
 * are replaced for the host. Deterministically exercise late BSP progress. */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "handoff.h"
#include "../kernel/log.c"
struct Boot boot;
static u32 pixels[800*600];
static void reset(int verbose){
 boot=(struct Boot){.fb=(u64)pixels,.w=800,.h=600,.stride=800,.boot_flags=verbose};
 console_live=0;log_capture=1;splash_live=1;boot_loginit();
}
static void*late_progress(void*x){(void)x;for(int j=0;j<2000;j++)boot_progress(j%101);return 0;}
int main(void){
 for(int verbose=0;verbose<2;verbose++){
  reset(verbose);boot_progress(94);
  if(!verbose)assert(pixels[389*800+400]==0xeeeeee);
  /* First-present ownership transfer happens before the actual scanout. */
  boot_console_stop();for(int j=0;j<800*600;j++)pixels[j]=0x2d2d2d;
  pthread_t workers[4];for(int k=0;k<4;k++)assert(!pthread_create(&workers[k],0,late_progress,0));
  boot_logstr("BSP services ready after Recovery first frame\n");
  for(int k=0;k<4;k++)assert(!pthread_join(workers[k],0));
  for(int j=0;j<800*600;j++)assert(pixels[j]==0x2d2d2d);
 }
 puts("PASS actual kernel boot display: late BSP/4-thread progress and verbose logs cannot repaint a userspace frame after first-present handoff");
 /* Writer already holding the shared lock completes before handoff returns. */
 reset(0);pthread_t worker;assert(!pthread_create(&worker,0,late_progress,0));boot_console_stop();
 for(int j=0;j<800*600;j++)pixels[j]=0x2d2d2d;
 assert(!pthread_join(worker,0));for(int j=0;j<800*600;j++)assert(pixels[j]==0x2d2d2d);
 puts("PASS concurrent splash writer drains before ownership transfer; no delayed write survives first full repaint");return 0;
}
