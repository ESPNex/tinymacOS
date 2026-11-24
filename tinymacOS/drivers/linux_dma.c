/* SPDX-License-Identifier: GPL-2.0-only */
#ifdef TMC_DMA_NATIVE_TEST
#include "base.h"
#define PAGE 4096ull
void *kmalloc(size_t);
void kfree(void *);
#else
#include "../kernel/k.h"
#endif
#include "linux_dma_pool.h"
/* Current kernel heap is physically contiguous identity-mapped low memory.
 * Align each coherent allocation to its page order; retain the true heap base.
 * This is NOT an IOMMU mapper or a DMA API for non-coherent architectures. */
void *tmc_dma_os_alloc(size_t n){return n&&n<=65536?kmalloc(n):0;}
void tmc_dma_os_free(void *p){kfree(p);}
void *tmc_dma_os_coherent(size_t n,u64 *dma)
{
 if(!n||n>65536||!dma)return 0;size_t alignment=PAGE;while(alignment<n)alignment<<=1;
 void *raw=kmalloc(n+alignment-1+sizeof(void *));if(!raw)return 0;
 u64 address=((u64)raw+sizeof(void *)+alignment-1)&~(alignment-1);
 if(address>0xffffffffull||n-1>0xffffffffull-address){kfree(raw);return 0;}
 void *p=(void *)address;((void **)p)[-1]=raw;memset(p,0,n);*dma=address;return p;
}
void tmc_dma_os_coherent_free(size_t n,void *p,u64 dma)
{if(p&&n&&n<=65536&&(u64)p==dma)kfree(((void **)p)[-1]);}
