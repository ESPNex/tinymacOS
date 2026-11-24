/* SPDX-License-Identifier: GPL-2.0-only */
#define TMC_CONTROLLER_DMA_TEST 1
#include "../drivers/ehci.c"
#define main original_dma_pool_contract
#include "linux_dma_pool.c"
#undef main
int main(void)
{
 assert(!original_dma_pool_contract());struct Ehci *e=ehcis;
 for(long fail_at=0;fail_at<8;fail_at++){
  memset(e,0,sizeof *e);fail_after=fail_at;int ok=ehci_dma_init(e);fail_after=-1;if(ok)ehci_dma_release(e);assert(!heap_alive&&!dma_alive&&!e->td&&!e->dma_device);
 }
 for(unsigned run=0;run<128;run++){
  memset(e,0,sizeof *e);assert(ehci_dma_init(e));
  assert(e->td_dma!=(u64)e->td&&!(e->td_dma&63));
  for(unsigned i=0;i<USB_INPUT_DEVICES;i++){
   struct Edev *d=e->dev+i;
   assert(ehci_pipe_dma(e,d));assert(d->td_dma!=(u64)d->td);
   assert(d->td_dma+sizeof(struct Etd)*USB_INPUT_INTERFACES-1<=0xffffffffu);
   for(unsigned j=0;j<USB_INPUT_INTERFACES;j++){
    d->u.pipe[j].packet=64;d->u.pipe[j].toggle=j&1;d->u.pipe[j].ep=j+1;
    epipe_arm(d,j);assert(d->u.pipe[j].armed);assert(d->td[j].token&EACTIVE);
    assert(d->qh[j].next==d->td_dma+j*sizeof *d->td);
   }
  }
  /* Caller has quiesced its simulated controller before releasing pools. */
  ehci_dma_release(e);assert(!heap_alive&&!dma_alive);
 }
 /* A failed interrupt allocation must not publish a CPU-only descriptor. */
 for(long fail_at=0;fail_at<2;fail_at++){
  memset(e,0,sizeof *e);assert(ehci_dma_init(e));fail_after=fail_at;assert(!ehci_pipe_dma(e,e->dev));fail_after=-1;assert(!e->dev[0].td);ehci_dma_release(e);
 }
 TmcLinuxDmaStats stats;tmc_linux_dma_pool_stats(&stats);assert(stats.allocations==stats.frees&&stats.coherent_allocations==stats.coherent_frees&&!stats.live_bytes);
 puts("PASS actual native EHCI descriptor allocation/rearm/rollback uses whole original Linux dmapool.c: all 8 devices x 4 endpoints, independent non-identity bus handles in hardware links, 128 complete lifetimes and all DMA setup/expansion failures; EHCI scheduler still native; whole original UHCI has its separate hardware-model contract");
}
