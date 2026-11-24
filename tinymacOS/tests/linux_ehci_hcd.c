/* SPDX-License-Identifier: GPL-2.0-only */
/* Host hardware model follows original DMA frame/QH/TD links. It never calls
 * completion or fabricates Linux queue state; only device-visible registers
 * and descriptor status/payload are changed, as hardware would do. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../compat/linux/ehci_runtime.c"

static unsigned cf_locked;
void tmc_linux_usb_cf_begin(void){assert(!cf_locked);cf_locked=1;}
void tmc_linux_usb_cf_end(void){assert(cf_locked);cf_locked=0;}
static unsigned long heap_alive,dma_alive,clock_ms,irq_flags=0x200;
static long fail_after=-1;
static unsigned present=1,hw_interrupt_ready,hw_packets,hw_stall,hw_short,hw_never;
static unsigned int_callbacks,int_status,int_bytes,control_requests;
static u32 registers[1024];
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
static u32 *op(void){return registers+8;}
static unsigned output_bytes;
static void execute_qh(u32 address)
{
 struct ehci_qh_hw *qh=cpu(address&~31u,sizeof(*qh));
 u32 link=qh->hw_qtd_next;
 for(unsigned k=0;k<256&&!(link&1);k++){
  struct ehci_qtd *td=cpu(link&~31u,sizeof(*td));u32 token=td->hw_token;
  if(!(token&0x80)||token&0x40)break;
  unsigned n=(token>>16)&0x7fff,pid=(token>>8)&3,ep=(qh->hw_info1>>8)&15;
  if(hw_never||(ep&&!hw_interrupt_ready))break;
  unsigned remaining=0;
  if(ep&&hw_stall){td->hw_token=(token&~0x80)|0x40;qh->hw_token=td->hw_token;op()[1]|=2;hw_stall=0;break;}
  if(pid==2){assert(n==8);memcpy(setup_bytes,cpu(td->hw_buf[0],8),8);control_requests++;}
  else if(pid==0&&ep&&n){assert(n==1&&*(u8 *)cpu(td->hw_buf[0],n)==0x5a);output_bytes+=n;hw_interrupt_ready--;hw_packets++;}
  else if(pid==1&&n){u8 *data=cpu(td->hw_buf[0],n);unsigned actual=n;if(ep){actual=hw_short?min_t(unsigned,3,n):n;hw_interrupt_ready--;hw_packets++;}for(unsigned i=0;i<actual;i++)data[i]=ep?0x40+i:0x5a;remaining=n-actual;}
  td->hw_token=(token&~(0x80u|0x7fff0000u))|(remaining<<16);
  qh->hw_current=link;qh->hw_token=td->hw_token^(1u<<31); /* one packet toggle */
  if(token&0x8000)op()[1]|=1;
  link=remaining?td->hw_alt_next:td->hw_next;qh->hw_qtd_next=link;qh->hw_alt_next=td->hw_alt_next;
 }
}
static void walk_schedule(u32 link)
{
 for(unsigned n=0;n<256&&!(link&1);n++){
  if((link&6)!=2)break;struct ehci_qh_hw *qh=cpu(link&~31u,sizeof(*qh));u32 next=qh->hw_next;
  execute_qh(link);if(next==link)break;link=next;
 }
}
static void hardware_frame(void)
{
 if(!(op()[0]&1))return;op()[3]=(op()[3]+8)&0x3fff;
 if(op()[0]&(1u<<5))walk_schedule(op()[6]|2);
 if(op()[0]&(1u<<4)){u32 *frames=cpu(op()[5],512*4);walk_schedule(frames[(op()[3]>>3)&511]);}
}
u32 tmc_lx_os_irq_save(void){u32 f=irq_flags;irq_flags=0;return f;}
void tmc_lx_os_irq_restore(u32 f){irq_flags=f;}
void tmc_lx_os_spin_wait(void){clock_ms++;hardware_frame();}
u64 tmc_lx_os_clock_ms(void){return clock_ms;}
void tmc_lx_os_error(const char *s){assert(s&&*s);}
u32 tmc_ehci_os_read32(const volatile void *p){return *(const volatile u32 *)p;}
void tmc_ehci_os_write32(volatile void *p,u32 v)
{
 unsigned index=((volatile u32 *)p-registers);assert(index<1024);
 if(index==8){
  if(v&2){op()[0]=0;op()[1]=1u<<12;return;}
  op()[0]=v&~(1u<<6);op()[1]=(op()[1]&~((1u<<12)|(1u<<14)|(1u<<15)))|((v&1)?0:1u<<12)|((v&(1u<<4))?1u<<14:0)|((v&(1u<<5))?1u<<15:0);if(v&(1u<<6))op()[1]|=1u<<5;
 }else if(index==9)op()[1]&=~(v&63);
 else if(index>=25&&index<=26){u32 old=registers[index],result=(old&~(0x1000u|0x100u|0x2000u|0x80u|0x40u))|(v&(0x1000u|0x100u|0x2000u|0x80u|0x40u));result&=~(v&0x2a);if((old&0x100)&&!(v&0x100)){if(index==25)result|=4;else result|=0x2000;}registers[index]=result;}
 else registers[index]=v;
}
void *tmc_ehci_os_mmio(u32 address,unsigned bar){assert(address==0x80000800&&!bar);return registers;}
u32 tmc_ehci_os_pci_next(u32 class,u32 after){assert(class==0x0c0320);return present&&!after?0x80000800:0;}
u32 tmc_ehci_os_pci_read(u32 address,unsigned off)
{assert(address==0x80000800);switch(off){case 0:return 0x24cd8086;case 4:return pci_command;case 8:return 0x0c032001;case 0x10:return 0xf0000000;case 0x3c:return 11;default:return 0;}}
void tmc_ehci_os_pci_write16(u32 address,unsigned off,u16 value){assert(address==0x80000800&&off==4);pci_command=value;}
void tmc_ehci_os_pci_write8(u32 address,unsigned off,u8 value){(void)address;(void)off;(void)value;assert(0);}
void tmc_ehci_os_pci_write(u32 address,unsigned off,u32 value){(void)address;(void)off;(void)value;assert(0);}
void tmc_ehci_os_delay_us(unsigned us){clock_ms+=DIV_ROUND_UP(us,1000);hardware_frame();}
u64 tmc_ehci_os_clock_ns(void){return clock_ms*1000000ull;}
const char *tmc_ehci_os_board_name(void){return NULL;}
void tmc_ehci_os_fatal(void){abort();}
static TmcEhciDevice *rearm_device;
static unsigned rearm_remaining;
static u8 rearm_payload[8];
static void done(void *ctx,int status,unsigned bytes)
{
 assert(ctx==(void *)0x1234);
 struct usb_hcd *h=ehci_devices[0]->hcd;
 /* Linux EHCI requires HCD_BH: inline callback/requeue under its schedule
  * lock deadlocks on hardware. The model must exercise immediate requeue. */
 assert(hcd_giveback_urb_in_bh(h)&&!hcd_to_ehci(h)->lock.word);
 assert(h->high_prio_bh.completing_ep);
 int_callbacks++;int_status=status;int_bytes=bytes;
 if(rearm_remaining){assert(!status&&bytes==8);if(--rearm_remaining)assert(!tmc_linux_ehci_interrupt(rearm_device,1,8,8,rearm_payload,done,(void *)0x1234));}
}
static void init_hardware(void)
{memset(registers,0,sizeof registers);registers[0]=0x1000020;registers[1]=0x112;registers[2]=0;op()[1]=1u<<12;registers[25]=1;registers[26]=1|1u<<10;clock_ms=0;hw_never=0;hw_interrupt_ready=0;hw_short=hw_stall=0;}
int main(void)
{
 init_hardware();assert(!tmc_linux_ehci_module_init());assert(tmc_linux_ehci_controllers()==1&&tmc_linux_ehci_ports(0)==2);assert(pci_command&0x400);
 struct usb_hcd *h=ehci_devices[0]->hcd;struct ehci_hcd *e=hcd_to_ehci(h);assert(e->periodic_dma!=(u64)e->periodic);
 u8 descriptor[32]={0};assert(tmc_linux_ehci_hub(0,GetHubDescriptor,0,0,descriptor,sizeof descriptor)==9);assert(descriptor[1]==0x29&&descriptor[2]==2);
 assert(!tmc_linux_ehci_hub(0,SetPortFeature,USB_PORT_FEAT_RESET,1,NULL,0));clock_ms+=60;
 assert(tmc_linux_ehci_hub(0,GetPortStatus,0,1,descriptor,4)==4);assert(descriptor[0]&USB_PORT_STAT_ENABLE);
 TmcEhciDevice *d=tmc_linux_ehci_device(0,USB_SPEED_HIGH);assert(d);tmc_linux_ehci_device_state(d,0,64,0);
 u8 payload[128]={0};int rc=tmc_linux_ehci_control(d,128,6,0x100,0,payload,18,300);fprintf(stderr,"control=%d requests=%u\n",rc,control_requests);assert(rc==18&&payload[0]==0x5a);
 assert(!tmc_linux_ehci_control(d,0,5,1,0,NULL,0,300));tmc_linux_ehci_device_state(d,1,64,1);
 assert(!tmc_linux_ehci_interrupt(d,1,8,8,payload,done,(void *)0x1234));hw_interrupt_ready=1;
 for(unsigned i=0;i<30&&!int_callbacks;i++)tmc_linux_ehci_poll();fprintf(stderr,"int callbacks=%u status=%d bytes=%u\n",int_callbacks,int_status,int_bytes);assert(int_callbacks==1&&!int_status&&int_bytes==8&&payload[0]==0x40);
 hw_short=1;assert(!tmc_linux_ehci_interrupt(d,1,8,8,payload,done,(void *)0x1234));hw_interrupt_ready=1;
 for(unsigned i=0;i<30&&int_callbacks<2;i++)tmc_linux_ehci_poll();assert(int_callbacks==2&&!int_status&&int_bytes==3);hw_short=0;
 assert(!tmc_linux_ehci_interrupt(d,1,8,8,payload,done,(void *)0x1234));hw_interrupt_ready=1;hw_stall=1;
 for(unsigned i=0;i<30&&int_callbacks<3;i++)tmc_linux_ehci_poll();assert(int_callbacks==3&&(int)int_status==-EPIPE);
 tmc_linux_ehci_reset_toggle(d,1);assert(!tmc_linux_ehci_interrupt(d,1,8,8,payload,done,(void *)0x1234));assert(!tmc_linux_ehci_interrupt_stop(d,1));assert(int_callbacks==4&&(int)int_status==-ENOENT);
 hw_never=1;assert(tmc_linux_ehci_control(d,128,6,0x100,0,payload,18,10)==-ETIMEDOUT);hw_never=0;
 rearm_device=d;rearm_remaining=16;unsigned callbacks_before=int_callbacks;
 assert(!tmc_linux_ehci_interrupt(d,1,8,8,rearm_payload,done,(void *)0x1234));
 for(unsigned i=0;i<64&&rearm_remaining;i++){hw_interrupt_ready=1;tmc_linux_ehci_poll();}
 assert(!rearm_remaining&&int_callbacks==callbacks_before+16);
 /* Exact interrupt-OUT payload/length and distinct endpoint direction. */
 payload[0]=0x5a;unsigned before_output=int_callbacks;assert(!tmc_linux_ehci_interrupt_report(d,0x101,8,8,1,payload,done,(void *)0x1234));hw_interrupt_ready=1;
 for(unsigned i=0;i<40&&int_callbacks==before_output;i++)tmc_linux_ehci_poll();assert(int_callbacks==before_output+1&&!int_status&&int_bytes==1&&output_bytes==1&&payload[0]==0x5a);
 assert(!tmc_linux_ehci_device_destroy(d));
 TmcEhciDevice *hub=tmc_linux_ehci_device(0,USB_SPEED_HIGH);d=tmc_linux_ehci_device(0,USB_SPEED_FULL);assert(hub&&d);
 tmc_linux_ehci_device_state(hub,1,64,1);tmc_linux_ehci_device_state(d,2,64,1);
 assert(!tmc_linux_ehci_translator(d,hub,2,0,666));assert(tmc_linux_ehci_device_destroy(hub)==-EBUSY);
 assert(tmc_linux_ehci_control(d,128,6,0x100,0,payload,18,300)==18);
 struct urb clear={.dev=&d->core,.pipe=(PIPE_CONTROL<<30)|(2<<8)|128,.ep=&d->ep0};unsigned before=control_requests;
 assert(!usb_hub_clear_tt_buffer(&clear));tmc_linux_ehci_poll();assert(control_requests==before+2);assert(!hub->hub.tt.clear_work.pending&&list_empty(&hub->hub.tt.clear_list));
 assert(!tmc_linux_ehci_device_destroy(d));assert(!tmc_linux_ehci_device_destroy(hub));tmc_linux_ehci_module_exit();assert(!heap_alive&&!dma_alive&&!timers&&!hrtimers&&!registered_driver&&!work_head&&!bh_head[0]&&!bh_head[1]);assert(irq_flags==0x200);
 for(long at=0;at<38;at++){init_hardware();fail_after=at;int init=tmc_linux_ehci_module_init();fail_after=-1;if(!init)tmc_linux_ehci_module_exit();assert(!heap_alive&&!dma_alive&&!timers&&!hrtimers&&!registered_driver&&!work_head&&!bh_head[0]&&!bh_head[1]);}
#ifdef TMC_EHCI_DMA_STRESS
 for(unsigned cycle=0;cycle<TMC_EHCI_DMA_STRESS;cycle++){
  init_hardware();assert(!tmc_linux_ehci_module_init());d=tmc_linux_ehci_device(0,USB_SPEED_HIGH);assert(d);tmc_linux_ehci_device_state(d,1,64,1);
  assert(tmc_linux_ehci_control(d,128,6,0x100,0,payload,18,300)==18);
  unsigned before=int_callbacks;assert(!tmc_linux_ehci_interrupt(d,1,8,8,payload,done,(void *)0x1234));hw_interrupt_ready=1;
  for(unsigned i=0;i<30&&int_callbacks==before;i++)tmc_linux_ehci_poll();assert(int_callbacks==before+1&&!int_status);
  assert(!tmc_linux_ehci_device_destroy(d));tmc_linux_ehci_module_exit();assert(!heap_alive&&!dma_alive&&!timers&&!hrtimers&&!registered_driver&&!work_head&&!bh_head[0]&&!bh_head[1]);
 }
 puts("PASS 128 complete original EHCI controller/device DMA lifetimes: real frame/QH/qTD bus links, control, interrupt, shutdown and no leaks");
#endif
 puts("PASS complete original Linux EHCI core/queue/memory/schedule/root-hub/timer/PCI on nonidentity DMA hardware model: control, full/short/STALL/cancel/timeout, 16 immediate callback resubmissions, HCD_BH lock isolation, TT-worker/control-BH progress, real hrtimers, sysfs-method registry and failure rollback. Not full USB/usbhid port or physical-board proof.");
}
