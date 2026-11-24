/* SPDX-License-Identifier: GPL-2.0-only */
/* Real services for the private, serialized XHCI executor. */
unsigned xhci_cf_port_reset_rwsem;
extern void tmc_linux_usb_cf_begin(void),tmc_linux_usb_cf_end(void);
void down_write(unsigned *s){if(s!=&xhci_cf_port_reset_rwsem)BUG();tmc_linux_usb_cf_begin();*s=1;}
void up_write(unsigned *s){if(s!=&xhci_cf_port_reset_rwsem||!*s)BUG();*s=0;tmc_linux_usb_cf_end();}
ktime_t ktime_get_real(void){return tmc_xhci_os_clock_ns();} /* Only XHCI elapsed intervals use this private clock. */
int platform_register_drivers(struct platform_driver * const *p,unsigned n){(void)p;return n?-EOPNOTSUPP:0;}
void platform_unregister_drivers(struct platform_driver * const *p,unsigned n){(void)p;if(n)WARN_ON(1);}
long schedule_timeout_uninterruptible(long ms)
{if(ms<=0)return 0;u64 start=tmc_lx_os_clock_ms();while(tmc_lx_os_clock_ms()-start<(u64)ms)xhci_runtime_progress();return 0;}
int scnprintf(char *out,size_t n,const char *fmt,...)
{if(!out||!n||!fmt)return 0;va_list ap;va_start(ap,fmt);i64 value;if(!strcmp(fmt,"%d\n"))value=va_arg(ap,int);else if(!strcmp(fmt,"%u\n"))value=va_arg(ap,unsigned);else{va_end(ap);return -EINVAL;}va_end(ap);char digits[32],text[40];unsigned k=0,p=0;if(value<0){text[p++]='-';value=-value;}do{digits[k++]='0'+value%10;value/=10;}while(value);while(k)text[p++]=digits[--k];text[p++]='\n';unsigned copy=min_t(unsigned,p,n-1);memcpy(out,text,copy);out[copy]=0;return copy;}
int kstrtouint(const char *text,unsigned base,unsigned *out)
{if(!text||!out||(base!=0&&base!=10))return -EINVAL;unsigned value=0,seen=0;if(*text=='+')text++;while(*text>='0'&&*text<='9'){unsigned digit=*text++-'0';if(value>(UINT_MAX-digit)/10)return -EOVERFLOW;value=value*10+digit;seen=1;}if(*text=='\n')text++;if(!seen||*text)return -EINVAL;*out=value;return 0;}
int sscanf(const char *text,const char *format,...)
{if(!text||strcmp(format,"%d"))return -EINVAL;while(*text==' '||*text=='\t')text++;int negative=*text=='-';if(*text=='-'||*text=='+')text++;unsigned value=0,seen=0;while(*text>='0'&&*text<='9'){unsigned digit=*text++-'0';if(value>((unsigned)INT_MAX+negative-digit)/10)return 0;value=value*10+digit;seen=1;}if(!seen)return 0;va_list ap;va_start(ap,format);int *out=va_arg(ap,int *);va_end(ap);*out=negative?(int)(0u-value):(int)value;return 1;}
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,struct pci_dev *p)
{for(;ids->vendor||ids->device||ids->class_mask;ids++)if((!ids->vendor||ids->vendor==p->vendor)&&(!ids->device||ids->device==p->device)&&(!ids->class_mask||((tmc_xhci_os_pci_read(p->address,8)>>8)&ids->class_mask)==ids->class))return ids;return NULL;}
int pci_set_mwi(struct pci_dev *p)
{u16 command;int rc=pci_read_config_word(p,4,&command);if(rc)return rc;pci_write_config_word(p,4,command|16);pci_read_config_word(p,4,&command);return command&16?0:-EIO;}
void pci_clear_mwi(struct pci_dev *p){u16 v;if(!pci_read_config_word(p,4,&v))pci_write_config_word(p,4,v&~16u);}
int dma_set_coherent_mask(struct device *d,u64 mask){if(!d||mask<4095||mask>0xffffffffu)return -EOPNOTSUPP;d->mask=mask;return 0;}
int pci_read_config_byte(struct pci_dev *p,int off,u8 *out){if(!p||!out||off<0||off>255)return -EINVAL;*out=tmc_xhci_os_pci_read(p->address,off&~3)>>(8*(off&3));return 0;}
int pci_read_config_dword(struct pci_dev *p,int off,u32 *out){if(!p||!out||off<0||off>252||off&3)return -EINVAL;*out=tmc_xhci_os_pci_read(p->address,off);return 0;}
int pci_find_capability(struct pci_dev *p,int id)
{u16 status;if(pci_read_config_word(p,6,&status)||!(status&16))return 0;u8 off;if(pci_read_config_byte(p,0x34,&off))return 0;for(unsigned n=0;n<48&&off>=0x40;n++){u16 cap;if(off&3||pci_read_config_word(p,off,&cap))return 0;if((cap&255)==id)return off;off=cap>>8;}return 0;}
void usb_hcd_pci_shutdown(struct pci_dev *p){if(p&&p->hcd&&p->hcd->driver->shutdown)p->hcd->driver->shutdown(p->hcd);}
static int tmc_xhci_handoff(struct pci_dev *p,void *regs)
{
 (void)p;unsigned offset=xhci_find_next_ext_cap(regs,0,XHCI_EXT_CAPS_LEGACY);
 if(!offset)return 0;u32 *legacy=(u32 *)((u8 *)regs+offset);u32 v=readl(legacy);
 writel(v|(1u<<24),legacy);u64 start=tmc_lx_os_clock_ms();
 while(readl(legacy)&(1u<<16)){if(tmc_lx_os_clock_ms()-start>=1000)return -ETIMEDOUT;xhci_delay_us(1000);}
 writel(0,legacy+1);return 0;
}
int tmc_linux_xhci_module_init(void){int rc=tmc_linux_xhci_core_init();if(!rc){rc=tmc_linux_xhci_pci_init();if(rc)tmc_linux_xhci_core_exit();}return rc;}
void tmc_linux_xhci_module_exit(void){tmc_linux_xhci_pci_exit();if(!registered_driver)tmc_linux_xhci_core_exit();}
int hcd_periodic_completion_in_progress(struct usb_hcd *h,struct usb_host_endpoint *e){return h->high_prio_bh.completing_ep==e;}
static struct work_struct *work_head,**work_tail=&work_head;
static unsigned work_dispatch_depth;
int schedule_work(struct work_struct *w)
{if(!w||!w->function)return 0;if(w->pending)return 0;w->pending=1;w->next=NULL;*work_tail=w;work_tail=&w->next;return 1;}
static void xhci_dispatch_work(void)
{
 if(work_dispatch_depth)return;work_dispatch_depth=1;
 for(unsigned n=0;n<64&&work_head;n++){struct work_struct *w=work_head;work_head=w->next;if(!work_head)work_tail=&work_head;w->next=NULL;w->pending=0;w->running=1;w->function(w);w->running=0;}
 work_dispatch_depth=0;
}
int usb_control_msg(struct usb_device *core,unsigned pipe,u8 req,u8 rt,u16 value,u16 index,void *data,u16 n,int timeout)
{(void)pipe;return tmc_linux_xhci_control(container_of(core,TmcXhciDevice,core),rt,req,value,index,data,n,timeout);}
int usb_set_port_feature(struct usb_device *d,int port,int feature){return usb_control_msg(d,usb_sndctrlpipe(d,0),3,0x23,feature,port,NULL,0,1000);}
int usb_clear_port_feature(struct usb_device *d,int port,int feature){return usb_control_msg(d,usb_sndctrlpipe(d,0),1,0x23,feature,port,NULL,0,1000);}
#include "../../vendor/linux-v6.12/hub-tt.inc"
int tmc_linux_xhci_translator(TmcXhciDevice *d,TmcXhciDevice *hub,unsigned port,unsigned multi,unsigned think_time)
{
 if(!d||!hub||!port||port>15||hub->core.speed!=USB_SPEED_HIGH||d->core.bus!=hub->core.bus||!list_empty(&d->core.ep0.urb_list))return -EINVAL;
 struct usb_hub *h=&hub->hub;
 if(!h->hdev){h->hdev=&hub->core;h->tt.hub=&hub->core;h->tt.multi=!!multi;h->tt.think_time=think_time;INIT_LIST_HEAD(&h->tt.clear_list);h->tt.clear_work.function=hub_tt_work;}
 if(d->core.tt)return -EBUSY;d->core.tt=&h->tt;d->core.ttport=port;d->tt_ref=usb_get_dev(&hub->core);return 0;
}

/* BH work cannot share the sleeping TT worker's recursion guard: that worker
 * waits for control URBs, whose giveback must progress independently. */
static struct work_struct *bh_head[2],**bh_tail[2]={&bh_head[0],&bh_head[1]};
static unsigned bh_dispatch_depth;
int xhci_queue_bh(void *queue,struct work_struct *w)
{
 if(!w||!w->function||w->pending)return 0;
 unsigned q=queue==system_bh_highpri_wq?0:1;w->pending=1;w->next=NULL;*bh_tail[q]=w;bh_tail[q]=&w->next;return 1;
}
static void xhci_dispatch_bh(void)
{
 if(bh_dispatch_depth)return;bh_dispatch_depth=1;
 for(unsigned n=0;n<64;n++){
  unsigned q=bh_head[0]?0:1;struct work_struct *w=bh_head[q];if(!w)break;
  bh_head[q]=w->next;if(!bh_head[q])bh_tail[q]=&bh_head[q];w->next=NULL;w->pending=0;w->running=1;w->function(w);w->running=0;
 }
 bh_dispatch_depth=0;
}

static void xhci_dispatch_hrtimers(void){} /* xHCI uses ordinary/delayed-work timers. */
