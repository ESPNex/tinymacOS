/* SPDX-License-Identifier: GPL-2.0-only */
/* Whole original dmapool.c, real allocations/locks/registered attributes and
 * managed cleanup. Host DMA addresses are deliberately NOT virtual pointers. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "../compat/linux/dma_pool.c"
static long heap_alive,dma_alive,fail_after=-1;
static u64 next_dma=0x100000;
static int diagnostics;
static int fail(void){if(fail_after==0)return 1;if(fail_after>0)fail_after--;return 0;}
void *tmc_dma_os_alloc(size_t n){if(fail())return NULL;void *p=malloc(n);if(p)__atomic_add_fetch(&heap_alive,1,__ATOMIC_RELAXED);return p;}
void tmc_dma_os_free(void *p){if(p){__atomic_sub_fetch(&heap_alive,1,__ATOMIC_RELAXED);free(p);}}
struct HostDma {void *raw;size_t n;u64 dma;};
void *tmc_dma_os_coherent(size_t n,u64 *dma)
{
 if(fail())return NULL;size_t align=4096;while(align<n)align<<=1;void *raw=NULL;if(posix_memalign(&raw,align,n+align))return NULL;
 void *p=(u8 *)raw+align;struct HostDma *h=(struct HostDma *)p-1;h->raw=raw;h->n=n;
 h->dma=(__atomic_fetch_add(&next_dma,align*2,__ATOMIC_RELAXED)+align-1)&~(align-1);*dma=h->dma;
 memset(p,0x56,n);__atomic_add_fetch(&dma_alive,1,__ATOMIC_RELAXED);return p;
}
void tmc_dma_os_coherent_free(size_t n,void *p,u64 dma){struct HostDma *h=(struct HostDma *)p-1;assert(h->n==n&&h->dma==dma);void *raw=h->raw;__atomic_sub_fetch(&dma_alive,1,__ATOMIC_RELAXED);free(raw);}
u32 tmc_lx_os_irq_save(void){return 0x200;}
void tmc_lx_os_irq_restore(u32 f){assert(f==0x200);}
void tmc_lx_os_spin_wait(void){__asm__ volatile("pause");}
void tmc_lx_os_error(const char *p){assert(p&&*p);__atomic_add_fetch(&diagnostics,1,__ATOMIC_RELAXED);}
static void zeroed(void *p,size_t n){for(size_t i=0;i<n;i++)assert(!((u8 *)p)[i]);}
static void release_marker(struct device *d,void *p){(void)d;unsigned *marker=p;assert(*marker==0x1234);diagnostics++;}
static void *worker(void *arg)
{
 TmcLinuxDmaPool *p=arg;for(unsigned i=0;i<2000;i++){u64 dma;u8 *v=tmc_linux_dma_pool_alloc(p,&dma);assert(v);zeroed(v,64);memset(v,i,64);assert(!tmc_linux_dma_pool_free(p,v,dma));}return NULL;
}
int main(void)
{
 assert(!tmc_linux_dma_device_create(NULL,~0ull));assert(!tmc_linux_dma_device_create("bad",0));
 TmcLinuxDmaDevice *d=tmc_linux_dma_device_create("host-controller",0xffffffffu);assert(d);
 assert(!tmc_linux_dma_pool_create(d,"zero",0,16,0));assert(!tmc_linux_dma_pool_create(d,"bad-align",32,3,0));assert(!tmc_linux_dma_pool_create(d,"bad-boundary",64,16,32));
 TmcLinuxDmaPool *p=tmc_linux_dma_pool_create(d,"descriptor",64,64,4096);assert(p);assert(tmc_linux_dma_device_destroy(d)==-EBUSY);
 char info[4096];assert(tmc_linux_dma_device_snapshot(d,info,sizeof info)>0);assert(strstr(info,"poolinfo - 0.1\n")&&strstr(info,"descriptor"));
 void *blocks[260];u64 handles[260];
 for(unsigned i=0;i<260;i++){
  blocks[i]=tmc_linux_dma_pool_alloc(p,handles+i);assert(blocks[i]);assert(!((u64)blocks[i]&63)&&!(handles[i]&63));assert(handles[i]!=(u64)blocks[i]);
  assert((handles[i]&4095)+64<=4096);zeroed(blocks[i],64);memset(blocks[i],i,64);
  for(unsigned j=0;j<i;j++)assert(blocks[i]!=blocks[j]&&handles[i]!=handles[j]);
 }
 assert(p->core->nr_pages==5&&p->core->nr_active==260);assert(tmc_linux_dma_pool_destroy(p)==-EBUSY);
 assert(tmc_linux_dma_pool_free(p,(u8 *)blocks[0]+1,handles[0]+1)==-EINVAL);assert(tmc_linux_dma_pool_free(p,blocks[0],handles[0]+64)==-EINVAL);
 assert(!tmc_linux_dma_pool_free(p,blocks[0],handles[0]));assert(tmc_linux_dma_pool_free(p,blocks[0],handles[0])==-EINVAL);
 for(unsigned i=1;i<260;i++)assert(!tmc_linux_dma_pool_free(p,blocks[i],handles[i]));
 assert(tmc_linux_dma_device_snapshot(d,info,sizeof info)>0);assert(strstr(info,"   0  320   64  5"));
 pthread_t a[4];for(unsigned i=0;i<4;i++)assert(!pthread_create(a+i,NULL,worker,p));for(unsigned i=0;i<4;i++)assert(!pthread_join(a[i],NULL));
 assert(!p->core->nr_active&&!tmc_linux_dma_pool_destroy(p));assert(tmc_linux_dma_device_snapshot(d,info,sizeof info)==-ENOENT);
 /* Boundary holes and minimum block-size rounding use ORIGINAL algorithm. */
 p=tmc_linux_dma_pool_create(d,"holes",300,16,1024);assert(p);
 for(unsigned i=0;i<30;i++){blocks[i]=tmc_linux_dma_pool_alloc(p,handles+i);assert(blocks[i]&&(handles[i]&1023)+304<=1024);}
 for(unsigned i=0;i<30;i++)assert(!tmc_linux_dma_pool_free(p,blocks[i],handles[i]));assert(!tmc_linux_dma_pool_destroy(p));
 p=tmc_linux_dma_pool_create(d,"large-control-chain",8448,64,0);assert(p);blocks[0]=tmc_linux_dma_pool_alloc(p,handles);assert(blocks[0]);zeroed(blocks[0],8448);assert(!tmc_linux_dma_pool_free(p,blocks[0],handles[0])&&!tmc_linux_dma_pool_destroy(p));
 /* Full original managed entry points, real devres failure/match/destruction. */
 struct dma_pool *managed=dmam_pool_create("managed",&d->core,32,16,0);assert(managed&&d->core.resources);u64 handle;void *v=dma_pool_alloc(managed,__GFP_ZERO,&handle);assert(v);dma_pool_free(managed,v,handle);dmam_pool_destroy(managed);assert(!d->core.resources&&list_empty(&d->core.dma_pools));
 managed=dmam_pool_create("detach-release",&d->core,64,64,0);assert(managed);unsigned *marker=devres_alloc(release_marker,sizeof(*marker),GFP_KERNEL);assert(marker);*marker=0x1234;devres_add(&d->core,marker);int before=diagnostics;dma_release_resources(&d->core);assert(diagnostics==before+1&&list_empty(&d->core.dma_pools));
 assert(!tmc_linux_dma_device_destroy(d)&&!heap_alive&&!dma_alive);
 /* Every allocation failure during create + first page expansion rolls back. */
 for(long at=0;at<6;at++){
  fail_after=at;d=tmc_linux_dma_device_create("failure",0xffffffffu);if(d){p=tmc_linux_dma_pool_create(d,"fault",64,64,4096);if(p){v=tmc_linux_dma_pool_alloc(p,&handle);if(v)assert(!tmc_linux_dma_pool_free(p,v,handle));assert(!tmc_linux_dma_pool_destroy(p));}assert(!tmc_linux_dma_device_destroy(d));}
  fail_after=-1;assert(!heap_alive&&!dma_alive);
 }
 d=tmc_linux_dma_device_create("mask",4095);assert(d);p=tmc_linux_dma_pool_create(d,"unreachable",32,16,0);assert(p&&!tmc_linux_dma_pool_alloc(p,&handle));assert(!tmc_linux_dma_pool_destroy(p)&&!tmc_linux_dma_device_destroy(d)&&!heap_alive&&!dma_alive);
 /* Original poison detection catches a write after free, not a success stub. */
 d=tmc_linux_dma_device_create("poison",0xffffffffu);p=tmc_linux_dma_pool_create(d,"check",64,64,0);assert(p);v=tmc_linux_dma_pool_alloc(p,&handle);assert(v);assert(!tmc_linux_dma_pool_free(p,v,handle));((u8 *)v)[32]=0;before=diagnostics;v=tmc_linux_dma_pool_alloc(p,&handle);assert(v&&diagnostics>before);assert(!tmc_linux_dma_pool_free(p,v,handle));assert(!tmc_linux_dma_pool_destroy(p)&&!tmc_linux_dma_device_destroy(d));
 for(unsigned i=0;i<1024;i++){d=tmc_linux_dma_device_create("lifetime",0xffffffffu);p=tmc_linux_dma_pool_create(d,"pool",16,16,4096);assert(p);v=tmc_linux_dma_pool_alloc(p,&handle);assert(v);assert(!tmc_linux_dma_pool_free(p,v,handle)&&!tmc_linux_dma_pool_destroy(p)&&!tmc_linux_dma_device_destroy(d));}
 TmcLinuxDmaStats stats;tmc_linux_dma_pool_stats(&stats);assert(stats.allocations==stats.frees&&stats.coherent_allocations==stats.coherent_frees&&!stats.live_bytes&&!heap_alive&&!dma_alive);
 puts("PASS whole original Linux v6.12 mm/dmapool.c: real independent CPU/bus mappings, 260-block multi-page alignment/boundaries, zeroing, boundary holes, >page control chains, busy teardown/invalid-free protection, registered original attribute, full managed devres cleanup, every allocation-failure rollback, DMA-mask rejection, original poison checks, four-thread 8000 allocations and 1024 leak-free lifecycles; NOT a complete Linux HCD");
 return 0;
}
