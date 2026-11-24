/* SPDX-License-Identifier: GPL-2.0-only */
/* Host hardware model follows original DMA frame/QH/TD links. It never calls
 * completion or fabricates Linux queue state; only device-visible registers
 * and descriptor status/payload are changed, as hardware would do. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../compat/linux/uhci_runtime.c"
#include "../compat/linux/dma_pool.c"
static unsigned long heap_alive,dma_alive,clock_ms,irq_flags=0x200;
static long fail_after=-1;
static unsigned present=1,hw_interrupt_ready,hw_packets,hw_stall,hw_short,hw_never;
static unsigned int_callbacks,int_status,int_bytes,control_requests;
static u8 registers[32];
static u32 pci_command;
static u8 setup_bytes[8];
struct HostDma {void *cpu;size_t n;u64 bus;};
static struct HostDma host_dma[256];
static u64 next_dma=0x100000;
static int fault(void){if(!fail_after)return 1;if(fail_after>0)fail_after--;return 0;}
void *tmc_dma_os_alloc(size_t n){if(fault())return NULL;void *p=malloc(n);if(p)heap_alive++;return p;}
void tmc_dma_os_free(void *p){if(p){assert(heap_alive);heap_alive--;free(p);}}
void *tmc_dma_os_coherent(size_t n,u64 *dma)
{
 if(fault())return NULL;size_t align=4096;while(align<n)align<<=1;
 void *p=NULL;if(posix_memalign(&p,align,n))return NULL;
 unsigned i=0;while(i<256&&host_dma[i].cpu)i++;assert(i<256);
 next_dma=ALIGN(next_dma,align);*dma=next_dma;next_dma+=align;
 host_dma[i]=(struct HostDma){p,n,*dma};dma_alive++;memset(p,0,n);return p;
}
void tmc_dma_os_coherent_free(size_t n,void *p,u64 bus)
{for(unsigned i=0;i<256;i++)if(host_dma[i].cpu==p){assert(host_dma[i].n==n&&host_dma[i].bus==bus);free(p);host_dma[i].cpu=NULL;assert(dma_alive);dma_alive--;return;}assert(0);}
static void *cpu(u64 bus,size_t n)
{for(unsigned i=0;i<256;i++){struct HostDma *d=&host_dma[i];if(d->cpu&&bus>=d->bus&&bus-d->bus<=d->n&&n<=d->n-(bus-d->bus))return (u8 *)d->cpu+bus-d->bus;}fprintf(stderr,"unmapped DMA %llx len %zu\n",(unsigned long long)bus,n);abort();}
static u16 rd16(unsigned off){u16 v;memcpy(&v,registers+off,2);return v;}
static void wr16(unsigned off,u16 v){memcpy(registers+off,&v,2);}
static void hardware_frame(void)
{
 if(!(rd16(0)&1))return;
 unsigned frame=(rd16(6)+1)&1023;wr16(6,frame);if(hw_never)return;u32 address;memcpy(&address,registers+8,4);if(!address)return;
 u32 link=((u32 *)cpu(address,4096))[frame];
 for(unsigned steps=0;steps<2048&&!(link&1);steps++){
  if(!(link&2)){/* isochronous TD before the skeleton */struct uhci_td *td=cpu(link&~15u,16);link=td->link;continue;}
  u32 *qh=cpu(link&~15u,8);u32 next=qh[0],element=qh[1];
  for(unsigned t=0;t<256&&!(element&3);t++){
   u32 *td=cpu(element&~15u,16);u32 status=td[1],token=td[2];
   if(!(status&(1u<<23)))break;
   unsigned pid=token&255,ep=(token>>15)&15,n=((token>>21)+1)&2047;
   if(ep&&!hw_interrupt_ready){td[1]|=1u<<19;break;}
   if(hw_stall&&ep){td[1]=(status&~((1u<<23)|2047u))|(1u<<22)|2047;wr16(2,rd16(2)|3);hw_stall=0;break;}
   unsigned actual=n;
   if(pid==USB_PID_SETUP){assert(n==8);memcpy(setup_bytes,cpu(td[3],8),8);control_requests++;}
   else if(pid==USB_PID_IN&&n){
    u8 *out=cpu(td[3],n);
    if(ep){actual=min_t(unsigned,n,hw_short?3:n);for(unsigned j=0;j<actual;j++)out[j]=(u8)(0x40+j);hw_packets++;hw_interrupt_ready--;}
    else{unsigned wanted=setup_bytes[6]|setup_bytes[7]<<8;actual=min_t(unsigned,n,wanted);memset(out,0x5a,actual);}
   }
   td[1]=(status&~((1u<<23)|(1u<<19)|2047u))|((actual-1)&2047);
   if(status&(1u<<24))wr16(2,rd16(2)|1);
   if(actual<n&&(status&(1u<<29))){wr16(2,rd16(2)|1);break;}
   element=td[0];qh[1]=element;
  }
  link=next;
 }
}
u32 tmc_lx_os_irq_save(void){u32 f=irq_flags;irq_flags=0;return f;}
void tmc_lx_os_irq_restore(u32 f){irq_flags=f;}
void tmc_lx_os_spin_wait(void){clock_ms++;hardware_frame();}
u64 tmc_lx_os_clock_ms(void){return clock_ms;}
void tmc_lx_os_error(const char *s){assert(s&&*s);}
u8 tmc_uhci_os_in8(u16 port){assert(port>=0x1000&&port<0x1020);return registers[port-0x1000];}
u16 tmc_uhci_os_in16(u16 port){assert(port>=0x1000&&port<0x1020);return rd16(port-0x1000);}
u32 tmc_uhci_os_in32(u16 port){u32 v;memcpy(&v,registers+port-0x1000,4);return v;}
void tmc_uhci_os_out8(u16 port,u8 v){assert(port>=0x1000&&port<0x1020);registers[port-0x1000]=v;}
void tmc_uhci_os_out16(u16 port,u16 v)
{
 unsigned off=port-0x1000;assert(off<32);
 if(off==0){if(v&2){wr16(0,0);wr16(2,32);}else{wr16(0,v);wr16(2,(rd16(2)&~32u)|((v&1)?0:32));}}
 else if(off==2)wr16(2,rd16(2)&~(v&31));
 else if(off==16||off==18){unsigned old=rd16(off);unsigned result=(old&~0x120e)|(v&0x1204);result&=~(v&0xa);wr16(off,result);}
 else wr16(off,v);
}
void tmc_uhci_os_out32(u16 port,u32 v){assert(port>=0x1000&&port<=0x101c);memcpy(registers+port-0x1000,&v,4);}
u32 tmc_uhci_os_pci_next(u32 class,u32 after){assert(class==0x0c0300);return present&&!after?0x80000800:0;}
u32 tmc_uhci_os_pci_read(u32 address,unsigned off)
{assert(address==0x80000800);switch(off){case 0:return 0x70208086;case 4:return pci_command;case 0x20:return 0x1001;case 0x3c:return 11;case 0xc0:return 0;default:return 0;}}
void tmc_uhci_os_pci_write16(u32 address,unsigned off,u16 value)
{assert(address==0x80000800);if(off==4)pci_command=value;else assert(off==0xc0);}
void tmc_uhci_os_pci_write8(u32 address,unsigned off,u8 value){assert(address==0x80000800&&off==0xc4&&!value);}
void tmc_uhci_os_pci_write(u32 address,unsigned off,u32 value){(void)address;(void)off;(void)value;assert(0);}
void tmc_uhci_os_delay_us(unsigned us){clock_ms+=DIV_ROUND_UP(us,1000);hardware_frame();}
const char *tmc_uhci_os_board_name(void){return NULL;}
static void done(void *ctx,int status,unsigned bytes)
{assert(ctx==(void *)0x1234);int_callbacks++;int_status=status;int_bytes=bytes;}
static void init_hardware(void)
{memset(registers,0,sizeof registers);wr16(2,32);wr16(16,0x81);wr16(18,0x80);clock_ms=0;hw_never=0;hw_interrupt_ready=0;hw_short=hw_stall=0;}
int main(void)
{
 init_hardware();assert(!tmc_linux_uhci_module_init());assert(tmc_linux_uhci_controllers()==1&&tmc_linux_uhci_ports(0)==2);assert(pci_command&0x400);
 struct usb_hcd *h=uhci_devices[0]->hcd;struct uhci_hcd *u=hcd_to_uhci(h);assert(u->is_initialized&&u->frame_dma_handle!=(u64)u->frame);
 assert(u->td_pool&&u->qh_pool&&u->frame_cpu);for(unsigned i=0;i<1024;i++)assert(!(u->frame[i]&1));
 u8 descriptor[32]={0};assert(tmc_linux_uhci_hub(0,GetHubDescriptor,0,0,descriptor,sizeof descriptor)==9);assert(descriptor[1]==0x29&&descriptor[2]==2);
 assert(!tmc_linux_uhci_hub(0,SetPortFeature,USB_PORT_FEAT_RESET,1,NULL,0));clock_ms+=60;
 assert(tmc_linux_uhci_hub(0,GetPortStatus,0,1,descriptor,4)==4);assert(descriptor[0]&USB_PORT_STAT_ENABLE);
 assert(tmc_linux_uhci_hub(0,GetPortStatus,0,0,descriptor,4)==-EPIPE);
 TmcUhciDevice *d=tmc_linux_uhci_device(0,USB_SPEED_FULL);assert(d);
 u8 payload[128]={0};int rc=tmc_linux_uhci_control(d,128,6,0x100,0,payload,18,300);assert(rc==18&&payload[0]==0x5a);assert(control_requests==1);
 assert(!tmc_linux_uhci_control(d,0,5,1,0,NULL,0,300));tmc_linux_uhci_device_state(d,1,64,1);
 assert(tmc_linux_uhci_control(d,0,9,1,0,payload,64,300)==64);
 assert(!tmc_linux_uhci_interrupt(d,1,8,10,payload,done,(void *)0x1234));assert(!int_callbacks);hw_interrupt_ready=1;
 for(unsigned i=0;i<20&&!int_callbacks;i++)tmc_linux_uhci_poll();assert(int_callbacks==1&&!int_status&&int_bytes==8&&payload[0]==0x40);
 assert(usb_gettoggle(&d->core,1,0)==1);
 hw_short=1;assert(!tmc_linux_uhci_interrupt(d,1,8,10,payload,done,(void *)0x1234));hw_interrupt_ready=1;
 for(unsigned i=0;i<30&&int_callbacks<2;i++)tmc_linux_uhci_poll();assert(int_callbacks==2&&!int_status&&int_bytes==3);hw_short=0;
 assert(!tmc_linux_uhci_interrupt(d,1,8,10,payload,done,(void *)0x1234));hw_interrupt_ready=1;hw_stall=1;
 for(unsigned i=0;i<30&&int_callbacks<3;i++)tmc_linux_uhci_poll();assert(int_callbacks==3&&(int)int_status==-EPIPE);
 assert(!tmc_linux_uhci_interrupt(d,1,8,10,payload,done,(void *)0x1234));assert(!tmc_linux_uhci_interrupt_stop(d,1));assert(int_callbacks==4&&(int)int_status==-ENOENT);
 hw_never=1;assert(tmc_linux_uhci_control(d,128,6,0x100,0,payload,18,10)==-ETIMEDOUT);hw_never=0;
 assert(!atomic_read(&d->core.urbnum));assert(!tmc_linux_uhci_device_destroy(d));
 assert(irq_flags==0x200);TmcUhciStats stats;tmc_linux_uhci_stats(&stats);assert(stats.submissions==stats.givebacks&&stats.schedule_polls&&stats.root_polls&&stats.timer_fires);
 tmc_linux_uhci_module_exit();assert(!heap_alive&&!dma_alive&&!timers&&!registered_driver);
 /* Exhaustive prefix of allocation faults covers cache/PCI/HCD/root/frame,
  * skeleton DMA pools, URB-private allocations and page-population rollback. */
 for(long at=0;at<38;at++){
  init_hardware();fail_after=at;int init=tmc_linux_uhci_module_init();fail_after=-1;
  if(!init)tmc_linux_uhci_module_exit();assert(!heap_alive&&!dma_alive&&!timers);assert(!registered_driver);
 }
 for(unsigned i=0;i<128;i++){
  init_hardware();assert(!tmc_linux_uhci_module_init());d=tmc_linux_uhci_device(0,USB_SPEED_LOW);assert(d);tmc_linux_uhci_device_state(d,1,8,1);
  assert(!tmc_linux_uhci_interrupt(d,1,8,1,payload,done,(void *)0x1234));assert(!tmc_linux_uhci_device_destroy(d));tmc_linux_uhci_module_exit();assert(!heap_alive&&!dma_alive&&!timers);
 }
 puts("PASS whole original Linux UHCI HCD/queue/root-hub/PCI: nonidentity DMA frame traversal, original control and interrupt TD execution, port reset/status, full/short/STALL/cancel/timeout giveback, toggles, real timers/IF preservation, allocation-failure rollback and 128 leak-free controller lifecycles. Hardware is modelled; no physical-board or complete USB-core claim.");
}
