/* SPDX-License-Identifier: GPL-2.0-only */
/* Execute actual kernel-side DMA allocation arithmetic with a low-address
 * host heap. No fake identity-DMA claim for the independent pool host test. */
#define _GNU_SOURCE
#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#define TMC_DMA_NATIVE_TEST 1
#include "../drivers/linux_dma.c"
static int live,fail_heap,high_heap;
struct HostHeap {size_t bytes;u64 pad;};
void *kmalloc(size_t n)
{
 if(fail_heap)return 0;size_t size=(n+sizeof(struct HostHeap)+4095)&~4095ull;
 void *p=mmap(0,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|(high_heap?0:MAP_32BIT),-1,0);if(p==MAP_FAILED)return 0;
 struct HostHeap *h=p;h->bytes=size;live++;void *out=h+1;if(high_heap)assert((u64)out>0xffffffffull);else assert((u64)out<=0xffffffffull);return out;
}
void kfree(void *p){struct HostHeap *h=(struct HostHeap *)p-1;size_t n=h->bytes;assert(live>0);live--;assert(!munmap(h,n));}
int main(void)
{
 u64 dma=0;assert(!tmc_dma_os_coherent(0,&dma));assert(!tmc_dma_os_coherent(65537,&dma));assert(!tmc_dma_os_coherent(4096,0));assert(!live);
 size_t sizes[]={1,8,16,1024,4096,8448,16384,65536};
 for(unsigned cycle=0;cycle<128;cycle++)for(unsigned i=0;i<sizeof sizes/sizeof sizes[0];i++){
  size_t n=sizes[i],alignment=4096;while(alignment<n)alignment<<=1;u8 *p=tmc_dma_os_coherent(n,&dma);assert(p&&live==1&&dma==(u64)p&&!(dma&(alignment-1))&&dma+n-1<=0xffffffffull);
  for(size_t j=0;j<n;j++)assert(!p[j]);memset(p,0x8e,n);
  tmc_dma_os_coherent_free(n,p,dma+1);assert(live==1);tmc_dma_os_coherent_free(n,p,dma);assert(!live);
 }
 fail_heap=1;assert(!tmc_dma_os_coherent(4096,&dma)&&!live);fail_heap=0;
 high_heap=1;assert(!tmc_dma_os_coherent(4096,&dma)&&!live);high_heap=0;
 puts("PASS actual native coherent-DMA backend: 1024 aligned/page-order/zeroed low-32-bit allocations, 1..65536 bytes, exact heap-base release, heap failure and high-address rejection without leak; identity-DMA x86 only, not IOMMU/noncoherent mapping");
}
