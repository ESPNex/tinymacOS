/* SPDX-License-Identifier: MIT
 * PCIe native NVMe adapter. Polled queues; no firmware calls, MMIO passthrough
 * or userspace DMA. Lifetime-owned DMA buffers survive controller timeouts and are never freed.
 */
#include "../kernel/k.h"
#include "nvme_native.h"
#define NV_CONTROLLERS 4
static NvController *controllers[NV_CONTROLLERS];
static void *dma_owner[NV_CONTROLLERS];
static volatile u8*bars[NV_CONTROLLERS];
static Spin locks[NV_CONTROLLERS];
static struct {unsigned controller,ns;} disks[SETUP_MAX_DISKS];
static int count,used;
extern u64 tmc_lx_os_clock_ms(void); /* calibrated TSC, advances with IF clear */
static u32 read32(void*x,u32 r){return *(volatile u32*)((volatile u8*)x+r);}
static void write32(void*x,u32 r,u32 v){*(volatile u32*)((volatile u8*)x+r)=v;}
static u64 clock_ms(void*x){(void)x;return tmc_lx_os_clock_ms();}
static void poll(void*x){(void)x;tlb_shoot_poll();asm volatile("pause");}
void nvme_init(void){
 u32 dev=0;
 while(used<NV_CONTROLLERS&&(dev=pci_find_after(0x010802,dev))){
  unsigned id=used++;bars[id]=pci_mmio(dev,0);if(!bars[id])continue;
  dma_owner[id]=kmalloc(sizeof(NvController)+4095);
  if(!dma_owner[id]){boot_logstr("[nvme] no contiguous DMA memory\n");continue;}
  controllers[id]=(void*)(((u64)dma_owner[id]+4095)&~4095ull);
  memset(controllers[id],0,sizeof(NvController));
  NvPlatform os={(void*)bars[id],read32,write32,clock_ms,poll};
  if(nvme_controller_start(controllers[id],os)){boot_logstr("[nvme] controller refused or timed out\n");continue;}
  NvController*c=controllers[id];device_add("NVMe / PCIe", "storage",0);
  for(unsigned j=0;j<c->count;j++){
   NvNamespace*n=c->ns+j;

   if(n->sectors512>TMC_DISK_MAX_SECTORS){boot_logstr("[nvme] namespace exceeds current 1TiB block ABI; no truncation\n");continue;}
   if(count==SETUP_MAX_DISKS){boot_logstr("[nvme] device table full\n");break;}
   disks[count].controller=id;disks[count].ns=j;
   boot_logstr("[nvme] disk ");boot_lognum(count);boot_logstr(" nsid=");boot_lognum(n->id);boot_logstr(" sectors=");boot_lognum(n->sectors512);boot_logstr("\n");count++;
  }
  if(c->namespace_limit_reached)boot_logstr("[nvme] active namespace enumeration reached the explicit table limit\n");
 }
 if(!used)boot_logstr("[nvme] absent\n");
}
int nvme_count(void){return count;}
u32 nvme_size(int d){if(d<0||d>=count)return 0;return controllers[disks[d].controller]->ns[disks[d].ns].sectors512;}
const char*nvme_model(int d){return d>=0&&d<count?controllers[disks[d].controller]->model:"";}
int nvme_io(int d,u32 l,u32 n,void*p,int w){
 if(d<0||d>=count)return -1;unsigned k=disks[d].controller;u32 flags;spin_lock(locks+k,&flags);
 int r=nvme_namespace_io(controllers[k],disks[d].ns,l,n,p,w);spin_unlock(locks+k,flags);return r;
}
int nvme_flush(int d){
 if(d<0||d>=count)return -1;unsigned k=disks[d].controller;u32 flags;spin_lock(locks+k,&flags);
 int r=nvme_namespace_flush(controllers[k],disks[d].ns);spin_unlock(locks+k,flags);return r;
}

u32 nvme_logical_bytes(int d){return d<0||d>=count?0:controllers[disks[d].controller]->ns[disks[d].ns].block_bytes;}
