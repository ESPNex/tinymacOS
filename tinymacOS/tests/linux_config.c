/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Original entire config.c, actual adapter, scripted descriptor transport.
 * Does not replace parser/endpoint repair/cache/refcount/free/BOS logic. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../compat/linux/usb_config.c"
extern long tmc_config_test_alive,tmc_config_test_fail;
struct Wire {const u8 *configuration,*bos;unsigned size,bos_size,reads,short_at,error_at,oversized;};
static int read_descriptor(void *context,unsigned type,unsigned index,void *data,unsigned size)
{
 struct Wire *w=context;w->reads++;
 if(w->error_at==w->reads)return -EIO;
 if(w->oversized)return size+1;
 const u8 *p=type==USB_DT_CONFIG?w->configuration:type==USB_DT_BOS?w->bos:NULL;
 unsigned n=type==USB_DT_CONFIG?w->size:type==USB_DT_BOS?w->bos_size:0;
 if(!p||index>=8)return -EPIPE;
 n=MIN(n,size);if(w->short_at==w->reads)n=n>2?n-2:0;
 memcpy(data,p,n);return n;
}
static u8 simple[]={9,2,25,0,1,1,0,0x80,50, 9,4,0,0,1,3,1,2,0, 7,5,0x81,3,8,0,10};
static TmcLinuxConfig *open_config(struct Wire *w,unsigned speed)
{
 w->reads=0;return tmc_linux_config_open(speed,1,read_descriptor,w);
}
static void no_leaks(TmcLinuxConfig *c)
{
 tmc_linux_config_close(c);assert(!tmc_config_test_alive);
 TmcLinuxConfigStats s;tmc_linux_config_stats(&s);assert(s.allocations==s.frees);
}
int main(void)
{
 struct Wire w={.configuration=simple,.size=sizeof simple};
 assert(!tmc_linux_config_open(0,1,read_descriptor,&w));assert(!tmc_linux_config_open(3,0,read_descriptor,&w));
 assert(!tmc_linux_config_open(3,1,NULL,&w));assert(!tmc_linux_config_open(3,256,read_descriptor,&w));
 TmcLinuxConfig *c=open_config(&w,3);assert(c&&w.reads==2&&tmc_linux_config_count(c)==1);
 assert(tmc_linux_config_interfaces(c,0)==1&&tmc_linux_config_alternates(c,0,0)==1);
 TmcLinuxConfigInterface f;TmcLinuxConfigEndpoint ep;
 assert(tmc_linux_config_interface(c,0,0,0,&f)&&f.cls==3&&f.protocol==2&&f.endpoints==1&&f.configuration==1);
 assert(tmc_linux_config_endpoint(c,0,0,0,0,&ep)&&ep.address==0x81&&ep.packet==8&&ep.interval==10);
 assert(!tmc_linux_config_endpoint(c,0,0,0,1,&ep));assert(!tmc_linux_config_interface(c,0,1,0,&f));no_leaks(c);
 /* Exhaust every normal configuration allocation failure; all partial raw
  * descriptor/interface/endpoint caches are unwound by original Linux. */
 unsigned failures=0;
 for(long n=0;n<20;n++){
  tmc_config_test_fail=n;c=open_config(&w,3);if(!c)failures++;no_leaks(c);
 }
 assert(failures>=7);tmc_config_test_fail=-1;
 for(unsigned read=1;read<=2;read++){
  w.error_at=read;assert(!open_config(&w,3));assert(!tmc_config_test_alive);w.error_at=0;
 }
 w.short_at=1;assert(!open_config(&w,3));w.short_at=0;w.oversized=1;assert(!open_config(&w,3));w.oversized=0;assert(!tmc_config_test_alive);
 /* Two configurations, actual header+full reads for each. */
 w.reads=0;c=tmc_linux_config_open(3,2,read_descriptor,&w);assert(c&&w.reads==4&&tmc_linux_config_count(c)==2);no_leaks(c);
 w.reads=0;c=tmc_linux_config_open(3,20,read_descriptor,&w);assert(c&&w.reads==16&&tmc_linux_config_count(c)==8);no_leaks(c);
 /* Alternates retain independent endpoints; duplicate cross-interface
  * endpoint is skipped by Linux, while native HID selection rejects it. */
 u8 composite[]={9,2,57,0,2,1,0,0x80,50,
  9,4,0,0,1,3,1,1,0,7,5,0x81,3,8,0,10,
  9,4,0,1,1,3,1,2,0,7,5,0x81,3,8,0,10,
  9,4,1,0,1,3,0,0,0,7,5,0x82,3,8,0,10};
 w.configuration=composite;w.size=sizeof composite;c=open_config(&w,3);assert(c);
 assert(tmc_linux_config_interfaces(c,0)==2&&tmc_linux_config_alternates(c,0,0)==2);
 assert(tmc_linux_config_interface(c,0,0,1,&f)&&f.alternate==1&&f.protocol==2);no_leaks(c);
 composite[52]=0x81;c=open_config(&w,3);assert(c&&tmc_linux_config_interface(c,0,1,0,&f)&&!f.endpoints);no_leaks(c);
 /* Linux endpoint normalization uses actual bus speed, not VID/PID hacks. */
 u8 repair[sizeof simple];memcpy(repair,simple,sizeof repair);repair[20]=0xf1;repair[22]=0xff;repair[23]=7;repair[24]=0;
 w.configuration=repair;w.size=sizeof repair;
 unsigned speeds[]={1,2,3,5,6},maxima[]={8,64,1024,1024,1024};
 for(unsigned i=0;i<COUNT(speeds);i++){
  c=open_config(&w,speeds[i]);assert(c&&tmc_linux_config_endpoint(c,0,0,0,0,&ep));
  assert(ep.address==0x81&&ep.packet==maxima[i]&&ep.interval==(speeds[i]>=3?7u:10u));no_leaks(c);
 }
 repair[21]=2;c=open_config(&w,1);assert(c&&tmc_linux_config_endpoint(c,0,0,0,0,&ep)&&ep.attributes==3&&ep.packet==8&&ep.interval==1);no_leaks(c);
 repair[20]=0;c=open_config(&w,3);assert(c&&tmc_linux_config_interface(c,0,0,0,&f)&&!f.endpoints);no_leaks(c);
 /* SuperSpeed companion + SSP isochronous service byte count. */
 u8 ss[]={9,2,39,0,1,1,0,0x80,50,9,4,0,0,1,3,0,0,0,
  7,5,0x81,1,0,4,1,6,48,3,128,0,16,8,49,0,0,0x78,0x56,0x34,0x12};
 w.configuration=ss;w.size=sizeof ss;c=open_config(&w,6);assert(c&&tmc_linux_config_endpoint(c,0,0,0,0,&ep));
 assert(ep.burst==3&&ep.mult==128&&ep.bytes_per_interval==4096&&ep.ssp_bytes_per_interval==0x12345678);no_leaks(c);
 /* BOS EXT, SS, container ID, SSP and PTM are parsed by the original unit. */
 u8 bos[65]={5,15,65,0,5,7,16,2,2,0,0,0,10,16,3,0,0,0,0,0,0,0,20,16,4};
 bos[42]=20;bos[43]=16;bos[44]=10;bos[46]=1; /* two SSP speed entries */
 bos[62]=3;bos[63]=16;bos[64]=11;
 w.configuration=simple;w.size=sizeof simple;w.bos=bos;w.bos_size=sizeof bos;c=open_config(&w,6);assert(c);
 assert(!tmc_linux_config_bos(c)&&c->device.bos->ext_cap&&c->device.bos->ss_cap&&c->device.bos->ss_id&&c->device.bos->ssp_cap&&c->device.bos->ptm_cap);
 assert(!tmc_linux_config_bos(c)); /* replace, never leak a cached BOS */
 no_leaks(c);
 for(long n=0;n<3;n++){
  c=open_config(&w,6);assert(c);tmc_config_test_fail=n;assert(tmc_linux_config_bos(c)==-ENOMEM);tmc_config_test_fail=-1;no_leaks(c);
 }
 for(unsigned n=0;n<5;n++){
  w.bos_size=n;c=open_config(&w,6);assert(c&&tmc_linux_config_bos(c)<0);no_leaks(c);
 }
 w.bos_size=sizeof bos;bos[2]=4;c=open_config(&w,6);assert(c&&tmc_linux_config_bos(c)==-EINVAL);no_leaks(c);bos[2]=65;
 /* Strict native HID transport gate: reject malformed/truncated/class data,
  * preserve alternate zero selection and feed original configuration cache. */
 HidInterface selected[4];assert(hid_interfaces_at_speed(simple,sizeof simple,3,selected,4)==1);
 for(unsigned n=0;n<sizeof simple;n++)assert(hid_interfaces_at_speed(simple,n,3,selected,4)==-1);
 assert(hid_interfaces_at_speed(composite,sizeof composite,3,selected,4)==-1);
 unsigned cfg=0;int hub=0;u8 hub_descriptor[sizeof simple];memcpy(hub_descriptor,simple,sizeof simple);hub_descriptor[14]=9;
 assert(hid_configuration_at_speed(hub_descriptor,sizeof hub_descriptor,3,selected,4,&cfg,&hub)==0&&hub&&cfg==1);
 /* Deterministic mutation/truncation against actual original parsing and
  * teardown. The permissive Linux core may retain valid descriptor prefixes;
  * the native gate separately rejects malformed transport blobs. */
 u8 mutated[sizeof composite];u32 random=0x43129;
 for(unsigned iteration=0;iteration<4096;iteration++){
  memcpy(mutated,composite,sizeof mutated);random=random*1664525u+1013904223u;
  unsigned at=random%sizeof mutated;random=random*1664525u+1013904223u;mutated[at]=(u8)(random>>24);
  w.configuration=mutated;w.size=sizeof mutated;c=open_config(&w,3);no_leaks(c);
 }
 for(unsigned n=0;n<=sizeof composite;n++){
  w.configuration=composite;w.size=n;c=open_config(&w,5);no_leaks(c);
 }
 for(unsigned i=0;i<1024;i++){
  w.configuration=simple;w.size=sizeof simple;c=open_config(&w,3);assert(c);no_leaks(c);
 }
 puts("PASS complete original Linux USB core/config.c: actual descriptor transport, 8-config cap, alternate/interface/endpoint caches, duplicate endpoints, speed-aware repairs, SS+SSP companions, full BOS capabilities, all allocation/transport rollback, bounded malformed input and 1024 leak-free lifecycles; native HCD/enumeration/hubs/HID remain separate");
}
