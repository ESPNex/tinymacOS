/* SPDX-License-Identifier: GPL-2.0-or-later */
#define TMC_LINUX_CONFIG_UNIT 1
#include <linux/tmc_usb_config.h>
static TmcLinuxConfigStats tmc_linux_config_counters;
#define cstats tmc_linux_config_counters
void *tmc_config_alloc(size_t bytes,unsigned flags)
{
 /* Linux kmalloc allocations have size-class tailroom. Preserve a zeroed
  * guard for optional descriptor lookahead (e.g. missing SS companion),
  * without lying about the actual transfer length passed to the parser. */
 if(flags!=GFP_KERNEL||!bytes||bytes>65536-32)return NULL;
 void *p=tmc_config_os_alloc(bytes+32);if(p){memset(p,0,bytes+32);cstats.allocations++;}return p;
}
void tmc_config_free(void *p){if(p){cstats.frees++;tmc_config_os_free(p);}}
void tmc_config_delay(unsigned ms){tmc_config_os_delay(ms);}
void tmc_config_log(struct device *d,unsigned level,const char *fmt,...)
{
 (void)d;(void)fmt;if(level)cstats.notices++; /* bounded diagnostics, no keystrokes */
}
int tmc_config_get_descriptor(struct usb_device *d,unsigned type,unsigned index,void *out,unsigned n)
{
 if(!d||!d->read||!out||!n||n>65535)return -EINVAL;
 cstats.descriptor_reads++;
 int result=d->read(d->context,type,index,out,n);
 if(result<0)return result;if((unsigned)result>n)return -EIO;
 /* The original configuration core expects at least the configuration
  * header before copying it, even for short transfers. Guard this boundary
  * explicitly instead of interpreting unreceived/uninitialized bytes. */
 if(type==USB_DT_CONFIG){
  if(result<USB_DT_CONFIG_SIZE)return -EINVAL;
  const u8 *p=out;
  if(p[0]<9||p[1]!=USB_DT_CONFIG||(unsigned)(p[2]|p[3]<<8)<9)return -EINVAL;
 }
 return result;
}
#include "../../vendor/linux-v6.12/config.c"
struct TmcLinuxConfig {struct usb_device device;};
TmcLinuxConfig *tmc_linux_config_open(unsigned speed,unsigned count,TmcConfigRead read,void *context)
{
 if(!read||!count||count>255||(speed!=1&&speed!=2&&speed!=3&&speed!=5&&speed!=6)){cstats.failures++;return NULL;}
 TmcLinuxConfig *config=tmc_config_alloc(sizeof(*config),GFP_KERNEL);
 if(!config){cstats.failures++;return NULL;}
 struct usb_device *d=&config->device;d->dev.usb=d;d->descriptor.bNumConfigurations=count;d->speed=speed;d->read=read;d->context=context;
 if(usb_get_configuration(d)<0){cstats.failures++;tmc_linux_config_close(config);return NULL;}
 cstats.opens++;cstats.configs+=d->descriptor.bNumConfigurations;
 for(unsigned c=0;c<d->descriptor.bNumConfigurations;c++)for(unsigned i=0;i<d->config[c].desc.bNumInterfaces;i++){
  struct usb_interface_cache *cache=d->config[c].intf_cache[i];cstats.interfaces++;
  for(unsigned a=0;a<cache->num_altsetting;a++)cstats.endpoints+=cache->altsetting[a].desc.bNumEndpoints;
 }
 return config;
}
void tmc_linux_config_close(TmcLinuxConfig *config)
{
 if(!config)return;usb_destroy_configuration(&config->device);usb_release_bos_descriptor(&config->device);tmc_config_free(config);
}
int tmc_linux_config_bos(TmcLinuxConfig *config)
{
 if(!config)return -EINVAL;usb_release_bos_descriptor(&config->device);
 int result=usb_get_bos_descriptor(&config->device);if(!result)cstats.bos++;return result;
}
unsigned tmc_linux_config_count(const TmcLinuxConfig *c){return c?c->device.descriptor.bNumConfigurations:0;}
unsigned tmc_linux_config_interfaces(const TmcLinuxConfig *c,unsigned cfg)
{
 return cfg<tmc_linux_config_count(c)?c->device.config[cfg].desc.bNumInterfaces:0;
}
unsigned tmc_linux_config_alternates(const TmcLinuxConfig *c,unsigned cfg,unsigned iface)
{
 return iface<tmc_linux_config_interfaces(c,cfg)?c->device.config[cfg].intf_cache[iface]->num_altsetting:0;
}
static const struct usb_host_interface *alternate(const TmcLinuxConfig *c,unsigned cfg,unsigned iface,unsigned alt)
{
 return alt<tmc_linux_config_alternates(c,cfg,iface)?&c->device.config[cfg].intf_cache[iface]->altsetting[alt]:NULL;
}
int tmc_linux_config_interface(const TmcLinuxConfig *c,unsigned cfg,unsigned iface,unsigned alt,TmcLinuxConfigInterface *out)
{
 const struct usb_host_interface *a=alternate(c,cfg,iface,alt);if(!out||!a)return 0;
 *out=(TmcLinuxConfigInterface){.number=a->desc.bInterfaceNumber,.alternate=a->desc.bAlternateSetting,.cls=a->desc.bInterfaceClass,.subclass=a->desc.bInterfaceSubClass,.protocol=a->desc.bInterfaceProtocol,.endpoints=a->desc.bNumEndpoints,.configuration=c->device.config[cfg].desc.bConfigurationValue,.extra=a->extra,.extra_length=a->extralen};return 1;
}
int tmc_linux_config_endpoint(const TmcLinuxConfig *c,unsigned cfg,unsigned iface,unsigned alt,unsigned ep,TmcLinuxConfigEndpoint *out)
{
 const struct usb_host_interface *a=alternate(c,cfg,iface,alt);if(!out||!a||ep>=a->desc.bNumEndpoints)return 0;
 const struct usb_host_endpoint *e=a->endpoint+ep;
 *out=(TmcLinuxConfigEndpoint){.address=e->desc.bEndpointAddress,.attributes=e->desc.bmAttributes,.packet=usb_endpoint_maxp(&e->desc),.interval=e->desc.bInterval,.burst=e->ss_ep_comp.bMaxBurst,.mult=e->ss_ep_comp.bmAttributes,.bytes_per_interval=e->ss_ep_comp.wBytesPerInterval,.ssp_bytes_per_interval=e->ssp_isoc_ep_comp.dwBytesPerInterval};return 1;
}
struct cached_descriptor {const u8 *bytes;unsigned size;};
static int cached_read(void *context,unsigned type,unsigned index,void *out,unsigned n)
{
 const struct cached_descriptor *cache=context;if(type!=USB_DT_CONFIG||index)return -EPIPE;
 unsigned bytes=MIN(n,cache->size);memcpy(out,cache->bytes,bytes);return bytes;
}
int tmc_linux_usb_config_select(const u8 *bytes,unsigned length,unsigned speed,HidInterface *out,unsigned capacity,unsigned *configuration,int *hub)
{
 if(!bytes||!out||!capacity||length<9||length>1024)return -EINVAL;
 struct cached_descriptor cached={bytes,length};
 TmcLinuxConfig *config=tmc_linux_config_open(speed,1,cached_read,&cached);if(!config)return -ENOMEM;
 if(configuration)*configuration=config->device.config[0].desc.bConfigurationValue;if(hub)*hub=0;
 unsigned found=0;
 for(unsigned i=0;i<tmc_linux_config_interfaces(config,0);i++)for(unsigned a=0;a<tmc_linux_config_alternates(config,0,i);a++){
  TmcLinuxConfigInterface f;if(!tmc_linux_config_interface(config,0,i,a,&f)||f.alternate)continue;
  if(f.cls==9){if(hub)*hub=1;continue;}if(f.cls!=3)continue;
  unsigned report=0;
  for(unsigned at=0;at+2<=f.extra_length;){
   const u8 *p=f.extra+at;unsigned n=p[0];if(n<2||n>f.extra_length-at)break;
   if(p[1]==0x21&&n>=6&&p[5]<=(n-6)/3)for(unsigned k=0;k<p[5];k++)if(p[6+3*k]==0x22)report=p[7+3*k]|p[8+3*k]<<8;
   at+=n;
  }
  unsigned proto=f.subclass==1&&(f.protocol==1||f.protocol==2)?f.protocol:3;
  for(unsigned e=0;e<f.endpoints;e++){
   TmcLinuxConfigEndpoint ep;if(!tmc_linux_config_endpoint(config,0,i,a,e,&ep))continue;
   if(!(ep.address&128)||(ep.attributes&3)!=3||!ep.interval||ep.packet<(proto==1?8u:proto==2?3u:1u)||ep.packet>1024)continue;
   if(found<capacity)out[found++]=(HidInterface){.iface=f.number,.proto=proto,.ep=2*(ep.address&15)+1,.packet=ep.packet,.interval=ep.interval,.report_length=report};
   break;
  }
 }
 tmc_linux_config_close(config);return found;
}
int tmc_linux_usb_config_hid(const u8 *bytes,unsigned n,unsigned speed,HidInterface *out,unsigned cap)
{
 return tmc_linux_usb_config_select(bytes,n,speed,out,cap,NULL,NULL);
}
void tmc_linux_config_stats(TmcLinuxConfigStats *out){if(out)*out=cstats;}
