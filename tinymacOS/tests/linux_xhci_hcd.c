/* SPDX-License-Identifier: GPL-2.0-only */
/* Host hardware model follows original DMA frame/QH/TD links. It never calls
 * completion or fabricates Linux queue state; only device-visible registers
 * and descriptor status/payload are changed, as hardware would do. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../compat/linux/xhci_runtime.c"

static unsigned cf_locked;
void tmc_linux_usb_cf_begin(void){assert(!cf_locked);cf_locked=1;}
void tmc_linux_usb_cf_end(void){assert(cf_locked);cf_locked=0;}
static unsigned long heap_alive,dma_alive,clock_ms,irq_flags=0x200;
static long fail_after=-1;
static unsigned present=1,hw_interrupt_ready,hw_packets,hw_stall,hw_short,hw_never;
static unsigned int_callbacks,int_status,int_bytes,control_requests;
static u32 registers[4096];
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
/* Hardware model consumes device-visible command/transfer rings and produces
 * event TRBs. It never calls an HCD completion or reads software TD lists. */
static u32 *op(void){return registers+8;}
static u64 command_dequeue,event_base;static unsigned command_cycle=1,event_index,event_cycle=1,next_slot;
static unsigned pending[16][32];
static u64 reg64(unsigned off){return registers[off/4]|((u64)registers[off/4+1]<<32);}
static u32 *context(unsigned slot){u64 *dcbaa=cpu(reg64(0x50),16*8);assert(slot<16);return cpu(dcbaa[slot],1024);}
static void event(u64 pointer,u32 status,u32 control)
{
 u64 erst=reg64(0x2030);assert(erst);u32 *table=cpu(erst,16);u64 base=table[0]|((u64)table[1]<<32);
 if(base!=event_base){event_base=base;event_index=0;event_cycle=1;}
 assert(table[2]);u32 *e=cpu(base+16*event_index,16);
 e[0]=pointer;e[1]=pointer>>32;e[2]=status;e[3]=control|event_cycle;
 if(++event_index==table[2]){event_index=0;event_cycle^=1;}
 op()[1]|=8;registers[0x2020/4]|=1;
}
static void command_doorbell(void)
{
 if(!command_dequeue){command_dequeue=reg64(0x38)&~63ull;command_cycle=reg64(0x38)&1;}
 for(unsigned n=0;n<512;n++){
  u32 *t=cpu(command_dequeue,16);u32 flags=t[3];if((flags&1)!=command_cycle)break;
  unsigned type=(flags>>10)&63,slot=flags>>24,ep=(flags>>16)&31;u64 ptr=t[0]|((u64)t[1]<<32);u64 trb=command_dequeue;
  if(type==TRB_LINK){command_dequeue=ptr&~15ull;if(flags&2)command_cycle^=1;continue;}
  command_dequeue+=16;unsigned result=COMP_SUCCESS;
  if(type==TRB_ENABLE_SLOT){slot=++next_slot;assert(slot<16);}
  else if(type==TRB_ADDR_DEV){u32 *in=cpu(ptr,1056),*out=context(slot);memcpy(out,in+8,64);out[3]=(flags&TRB_BSR)?1u<<27:(2u<<27)|slot;out[8]=(out[8]&~7u)|EP_STATE_RUNNING;}
  else if(type==TRB_CONFIG_EP||type==TRB_EVAL_CONTEXT){u32 *in=cpu(ptr,1056),*out=context(slot);for(unsigned i=0;i<32;i++){if(in[0]&(1u<<i))memset(out+8*i,0,32);if(in[1]&(1u<<i)){memcpy(out+8*i,in+8*(i+1),32);if(i)out[8*i]=(out[8*i]&~7u)|EP_STATE_RUNNING;}}out[3]=(out[3]&~(31u<<27))|(3u<<27);}
  else if(type==TRB_STOP_RING){context(slot)[ep*8]=(context(slot)[ep*8]&~7u)|EP_STATE_STOPPED;pending[slot][ep]=0;}
  else if(type==TRB_RESET_EP){context(slot)[ep*8]=(context(slot)[ep*8]&~7u)|EP_STATE_STOPPED;}
  else if(type==TRB_SET_DEQ){u32 *out=context(slot);out[ep*8+2]=ptr;out[ep*8+3]=ptr>>32;}
  else if(type==TRB_DISABLE_SLOT){memset(pending[slot],0,sizeof pending[slot]);}
  else if(type!=TRB_RESET_DEV){fprintf(stderr,"unmodeled command %u\n",type);abort();}
  event(trb,result<<24,(TRB_COMPLETION<<10)|(slot<<24));
 }
}
static unsigned output_bytes;
static void transfer(unsigned slot,unsigned ep)
{
 if(hw_never)return;u32 *e=context(slot)+8*ep;if((e[0]&7)!=EP_STATE_RUNNING)return;
 if(ep>1&&!hw_interrupt_ready)return;
 u64 deq=e[2]|((u64)e[3]<<32);unsigned cycle=deq&1;deq&=~15ull;
 unsigned actual=0;
 for(unsigned n=0;n<512;n++){
  u32 *t=cpu(deq,16);u32 flags=t[3];if((flags&1)!=cycle)break;
  unsigned type=(flags>>10)&63,len=t[2]&0x1ffff;u64 ptr=t[0]|((u64)t[1]<<32),at=deq;
  if(type==TRB_LINK){deq=ptr&~15ull;if(flags&2)cycle^=1;continue;}
  unsigned code=COMP_SUCCESS,remaining=0;
  if(type==TRB_SETUP){memcpy(setup_bytes,t,8);control_requests++;}
  else if(type==TRB_DATA||type==TRB_NORMAL){
   if(ep>1&&hw_stall){code=COMP_STALL_ERROR;e[0]=(e[0]&~7u)|EP_STATE_HALTED;hw_stall=0;}
   else {unsigned got=ep>1&&hw_short?min_t(unsigned,len,3):len;if(got){u8 *data=(flags&TRB_IDT)?(u8 *)t:cpu(ptr,len);if(ep>1&&!(ep&1)){assert(got==1&&data[0]==0x5a);output_bytes+=got;}else for(unsigned i=0;i<got;i++)data[i]=ep>1?0x40+i:0x5a;}
    actual+=got;remaining=len-got;if(remaining)code=COMP_SHORT_PACKET;if(ep>1){hw_interrupt_ready--;hw_packets++;}}
  }else assert(type==TRB_STATUS||type==TRB_TR_NOOP);
  deq+=16;e[2]=(u32)deq|cycle;e[3]=deq>>32;
  if((flags&TRB_IOC)||code!=COMP_SUCCESS){event(at,(code<<24)|remaining,(TRB_TRANSFER<<10)|(ep<<16)|(slot<<24));pending[slot][ep]=0;return;}
 }
}
static void hardware_frame(void)
{if(!(op()[0]&1))return;registers[0x2000/4]+=8;for(unsigned s=1;s<=next_slot;s++)for(unsigned ep=1;ep<32;ep++)if(pending[s][ep])transfer(s,ep);}
u32 tmc_lx_os_irq_save(void){u32 f=irq_flags;irq_flags=0;return f;}
void tmc_lx_os_irq_restore(u32 f){irq_flags=f;}
void tmc_lx_os_spin_wait(void){clock_ms++;hardware_frame();}
u64 tmc_lx_os_clock_ms(void){return clock_ms;}
void tmc_lx_os_error(const char *s){assert(s&&*s);fprintf(stderr,"driver: %s\n",s);}
u32 tmc_xhci_os_read32(const volatile void *p){return *(const volatile u32 *)p;}
void tmc_xhci_os_write32(volatile void *p,u32 v)
{
 unsigned off=(const volatile u8 *)p-(const volatile u8 *)registers;assert(off<sizeof registers);
 if(off==0x20){if(v&2){op()[0]=0;op()[1]=1;command_dequeue=event_base=0;event_index=0;event_cycle=command_cycle=1;next_slot=0;memset(pending,0,sizeof pending);}else{op()[0]=v;op()[1]=(op()[1]&~1u)|((v&1)?0:1);}}
 else if(off==0x24)op()[1]&=~v;
 else if(off==0x2020)registers[off/4]=(v&~1u)|(registers[off/4]&1u&~v);
 else if(off>=0x1000&&off<0x1040){unsigned slot=(off-0x1000)/4;if(!slot)command_doorbell();else{unsigned ep=v&255;assert(ep<32);u32 *ctx=context(slot)+8*ep;if((ctx[0]&7)==EP_STATE_STOPPED)ctx[0]=(ctx[0]&~7u)|EP_STATE_RUNNING;pending[slot][ep]=1;transfer(slot,ep);}}
 else if(off==0x420||off==0x430){u32 old=registers[off/4];if(v&PORT_RESET)registers[off/4]=(old&~PORT_RESET)|PORT_PE|PORT_RC;else registers[off/4]=(old&~(v&(0x7fu<<17))&~PORT_POWER)|(v&PORT_POWER);}
 else registers[off/4]=v;
}
void *tmc_xhci_os_mmio(u32 a,unsigned bar){assert(a==0x80000800&&!bar);return registers;}
u32 tmc_xhci_os_pci_next(u32 c,u32 after){assert(c==0x0c0330);return present&&!after?0x80000800:0;}
u32 tmc_xhci_os_pci_read(u32 a,unsigned off)
{assert(a==0x80000800);switch(off){case 0:return 0x000d1b36;case 4:return pci_command;case 8:return 0x0c033001;case 0x10:return 0xf0000000;case 0x3c:return 11;default:return 0;}}
void tmc_xhci_os_pci_write16(u32 a,unsigned off,u16 v){assert(a==0x80000800&&off==4);pci_command=v;}
void tmc_xhci_os_pci_write8(u32 a,unsigned off,u8 v){(void)a;(void)off;(void)v;abort();}
void tmc_xhci_os_pci_write(u32 a,unsigned off,u32 v){(void)a;(void)off;(void)v;abort();}
void tmc_xhci_os_delay_us(unsigned us){clock_ms+=DIV_ROUND_UP(us,1000);hardware_frame();}
u64 tmc_xhci_os_clock_ns(void){return clock_ms*1000000ull;}
const char *tmc_xhci_os_board_name(void){return NULL;}
void tmc_xhci_os_fatal(void){abort();}
u8 tmc_xhci_os_in8(u16 p){(void)p;return 0xff;}
u16 tmc_xhci_os_in16(u16 p){(void)p;return 0xffff;}
u32 tmc_xhci_os_in32(u16 p){(void)p;return ~0u;}
void tmc_xhci_os_out8(u16 p,u8 v){(void)p;(void)v;abort();}
void tmc_xhci_os_out16(u16 p,u16 v){(void)p;(void)v;abort();}
void tmc_xhci_os_out32(u16 p,u32 v){(void)p;(void)v;abort();}
int tmc_lx_snprintf(char *s,size_t n,const char *fmt,...){va_list ap;va_start(ap,fmt);int rc=vsnprintf(s,n,fmt,ap);va_end(ap);return rc;}
static TmcXhciDevice *rearm_device;static unsigned rearm_remaining;static u8 rearm_payload[8];
static void done(void *ctx,int status,unsigned bytes)
{
 assert(ctx==(void *)0x1234);struct usb_hcd *h=xhci_devices[0]->hcd;
 assert(hcd_giveback_urb_in_bh(h)&&!hcd_to_xhci(h)->lock.word);assert(h->high_prio_bh.completing_ep||(h->shared_hcd&&h->shared_hcd->high_prio_bh.completing_ep));
 int_callbacks++;int_status=status;int_bytes=bytes;
 if(rearm_remaining){assert(!status&&bytes==8);if(--rearm_remaining)assert(!tmc_linux_xhci_interrupt(rearm_device,1,8,8,rearm_payload,done,ctx));}
}
static void init_hardware(void)
{
 memset(registers,0,sizeof registers);registers[0]=0x01000020;registers[1]=0x02000108;registers[4]=0x00400000;registers[5]=0x1000;registers[6]=0x2000;
 registers[0x100/4]=0x02000402;registers[0x104/4]=0x20425355;registers[0x108/4]=0x00000101;
 registers[0x110/4]=0x03000002;registers[0x114/4]=0x20425355;registers[0x118/4]=0x00000102;
 op()[1]=1;op()[2]=1;registers[0x420/4]=PORT_CONNECT|PORT_POWER|(3u<<10);registers[0x430/4]=PORT_POWER;
 clock_ms=0;command_dequeue=event_base=0;event_index=0;event_cycle=command_cycle=1;next_slot=0;memset(pending,0,sizeof pending);
 hw_never=hw_short=hw_stall=hw_interrupt_ready=0;
}
static void clean(void)
{if(heap_alive||dma_alive||timers)fprintf(stderr,"LIVE heap%lu dma%lu timers%p\n",heap_alive,dma_alive,(void*)timers);assert(!heap_alive&&!dma_alive&&!timers&&!registered_driver&&!work_head&&!bh_head[0]&&!bh_head[1]);assert(irq_flags==0x200);}
int main(void)
{
 init_hardware();assert(!tmc_linux_xhci_module_init());assert(tmc_linux_xhci_controllers()==2);
 assert(bus_hcd(0)->speed==HCD_USB2&&bus_hcd(1)->speed==HCD_USB3);
 assert(tmc_linux_xhci_ports(0)==1&&tmc_linux_xhci_ports(1)==1);
 u8 payload[128]={0};assert(tmc_linux_xhci_hub(0,GetPortStatus,0,1,payload,4)==4);assert(payload[0]&USB_PORT_STAT_CONNECTION);
 assert(tmc_linux_xhci_hub(1,GetPortStatus,0,1,payload,4)==4);assert(!(payload[0]&USB_PORT_STAT_CONNECTION));
 TmcXhciDevice *d=tmc_linux_xhci_device(0,USB_SPEED_HIGH);assert(d);assert(!tmc_linux_xhci_device_port(d,NULL,1));tmc_linux_xhci_device_state(d,0,64,0);
 assert(tmc_linux_xhci_control(d,128,6,0x100,0,payload,18,300)==18&&payload[0]==0x5a);
 assert(!tmc_linux_xhci_control(d,0,5,2,0,NULL,0,300));tmc_linux_xhci_device_state(d,2,64,1);
 assert(!tmc_linux_xhci_interrupt(d,1,8,8,payload,done,(void *)0x1234));hw_interrupt_ready=1;
 for(unsigned i=0;i<30&&!int_callbacks;i++)tmc_linux_xhci_poll();assert(int_callbacks==1&&!int_status&&int_bytes==8&&payload[0]==0x40);
 hw_short=1;assert(!tmc_linux_xhci_interrupt(d,1,8,8,payload,done,(void *)0x1234));hw_interrupt_ready=1;
 for(unsigned i=0;i<30&&int_callbacks<2;i++)tmc_linux_xhci_poll();assert(int_callbacks==2&&!int_status&&int_bytes==3);hw_short=0;
 assert(!tmc_linux_xhci_interrupt(d,1,8,8,payload,done,(void *)0x1234));hw_interrupt_ready=1;hw_stall=1;
 for(unsigned i=0;i<30&&int_callbacks<3;i++)tmc_linux_xhci_poll();assert(int_callbacks==3&&(int)int_status==-EPIPE);
 assert(!tmc_linux_xhci_control(d,2,1,0,0x81,NULL,0,300));tmc_linux_xhci_reset_toggle(d,1);hw_interrupt_ready=0;
 assert(!tmc_linux_xhci_interrupt(d,1,8,8,payload,done,(void *)0x1234));assert(!tmc_linux_xhci_interrupt_stop(d,1));assert(int_callbacks==4&&(int)int_status==-ENOENT);
 hw_never=1;assert(tmc_linux_xhci_control(d,128,6,0x100,0,payload,18,10)==-ETIMEDOUT);hw_never=0;
 rearm_device=d;rearm_remaining=600;unsigned before=int_callbacks;assert(!tmc_linux_xhci_interrupt(d,1,8,8,rearm_payload,done,(void *)0x1234));
 for(unsigned i=0;i<2048&&rearm_remaining;i++){hw_interrupt_ready=1;tmc_linux_xhci_poll();}assert(!rearm_remaining&&int_callbacks==before+600);
 /* Exact interrupt-OUT payload/length and distinct endpoint direction. */
 payload[0]=0x5a;before=int_callbacks;assert(!tmc_linux_xhci_interrupt_report(d,0x101,8,8,1,payload,done,(void *)0x1234));hw_interrupt_ready=1;
 for(unsigned i=0;i<40&&int_callbacks==before;i++)tmc_linux_xhci_poll();assert(int_callbacks==before+1&&!int_status&&int_bytes==1&&output_bytes==1&&payload[0]==0x5a);
 assert(!tmc_linux_xhci_device_destroy(d));
 /* Hardware slot contexts are checked, not private facade counters. */
 TmcXhciDevice *hub=tmc_linux_xhci_device(0,USB_SPEED_HIGH);assert(hub);assert(!tmc_linux_xhci_device_port(hub,NULL,1));
 assert(!tmc_linux_xhci_control(hub,0,5,3,0,NULL,0,300));tmc_linux_xhci_device_state(hub,3,64,1);assert(!tmc_linux_xhci_hub_device(hub,4,0,666));
 d=tmc_linux_xhci_device(0,USB_SPEED_FULL);assert(d);assert(!tmc_linux_xhci_device_port(d,hub,3));assert(!tmc_linux_xhci_control(d,0,5,4,0,NULL,0,300));
 assert((context(d->core.slot_id)[0]&0xfffff)==3);assert((context(d->core.slot_id)[2]&0xffff)==(hub->core.slot_id|(3u<<8)));
 assert(tmc_linux_xhci_device_destroy(hub)==-EBUSY);assert(!tmc_linux_xhci_hub_device(d,4,0,666));
 TmcXhciDevice *child=tmc_linux_xhci_device(0,USB_SPEED_LOW);assert(child);assert(!tmc_linux_xhci_device_port(child,d,2));assert(!tmc_linux_xhci_control(child,0,5,5,0,NULL,0,300));
 assert((context(child->core.slot_id)[0]&0xfffff)==0x23);assert((context(child->core.slot_id)[2]&0xffff)==(hub->core.slot_id|(3u<<8)));
 assert(tmc_linux_xhci_device_destroy(d)==-EBUSY);assert(!tmc_linux_xhci_device_destroy(child));assert(!tmc_linux_xhci_device_destroy(d));assert(!tmc_linux_xhci_device_destroy(hub));
 d=tmc_linux_xhci_device(1,USB_SPEED_SUPER);assert(d);assert(!tmc_linux_xhci_device_port(d,NULL,1));
 tmc_linux_xhci_device_state(d,0,512,0);assert(!tmc_linux_xhci_control(d,0,5,6,0,NULL,0,300));tmc_linux_xhci_device_state(d,6,512,1);
 assert(tmc_linux_xhci_interrupt(d,1,8,8,payload,done,(void *)0x1234)==-EINVAL);
 assert(tmc_linux_xhci_interrupt_companion(d,1,16,0,16)==-EINVAL);assert(!tmc_linux_xhci_interrupt_companion(d,1,1,0,16));
 assert(!tmc_linux_xhci_interrupt(d,1,8,8,payload,done,(void *)0x1234));
 assert(((context(d->core.slot_id)[25]>>8)&255)==1);assert((context(d->core.slot_id)[28]>>16)==16);
 unsigned ss_before=int_callbacks;hw_interrupt_ready=1;for(unsigned i=0;i<30&&int_callbacks==ss_before;i++)tmc_linux_xhci_poll();assert(int_callbacks==ss_before+1&&!int_status);
 assert(!tmc_linux_xhci_device_destroy(d));tmc_linux_xhci_module_exit();clean();
 for(long at=0;at<96;at++){
  init_hardware();fail_after=at;int rc=tmc_linux_xhci_module_init();fail_after=-1;if(!rc)tmc_linux_xhci_module_exit();clean();
 }
 for(long at=0;at<32;at++){
  init_hardware();assert(!tmc_linux_xhci_module_init());d=tmc_linux_xhci_device(0,USB_SPEED_HIGH);assert(d);assert(!tmc_linux_xhci_device_port(d,NULL,1));
  assert(!tmc_linux_xhci_control(d,0,5,2,0,NULL,0,300));tmc_linux_xhci_device_state(d,2,64,1);
  fail_after=at;int rc=tmc_linux_xhci_interrupt(d,1,8,8,payload,done,(void *)0x1234);fail_after=-1;
  if(rc)assert(!tmc_linux_xhci_interrupt(d,1,8,8,payload,done,(void *)0x1234));
  unsigned before=int_callbacks;hw_interrupt_ready=1;for(unsigned i=0;i<30&&int_callbacks==before;i++)tmc_linux_xhci_poll();assert(int_callbacks==before+1&&!int_status);
  assert(!tmc_linux_xhci_device_destroy(d));tmc_linux_xhci_module_exit();clean();
 }
 puts("PASS 96 controller allocation budgets and 32 endpoint/configure/DMA failure budgets with successful retry and zero leaked resources");
 puts("PASS original xHCI whole core/memory/ring/hub/PCI: separate USB2/USB3 root hubs, SuperSpeed companion burst/ESIT and EP0=512, nonidentity DMA command and event/transfer rings, control, full/short/STALL interrupt and clear-halt, cancellation, timeout, 600 callback requeues with ring rollover, nested route/TT contexts and parent lifetime, teardown with no leaks");
}
