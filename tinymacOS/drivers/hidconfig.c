#include "hidconfig.h"
#include "linux_usb_config.h"
/* Strict transport-boundary checks retain the OS policy of rejecting a
 * truncated/malformed class descriptor or duplicate selected HID endpoint.
 * Interface/alternate/endpoint caches and normalization are Linux config.c,
 * not a second native configuration decoder. */
static int validate(const u8 *d,unsigned n)
{
 if(!d||n<9||n>1024||d[0]<9||d[1]!=2||(unsigned)(d[2]|d[3]<<8)!=n)return 0;
 unsigned endpoints=0,interfaces[8]={0},iface=0;int candidate=0,selected=0;
 for(unsigned at=0;at<n;){
  if(n-at<2)return 0;const u8 *p=d+at;unsigned z=p[0];if(z<2||z>n-at)return 0;
  if(p[1]==4){
   if(z<9)return 0;candidate=!p[3]&&p[5]==3;selected=0;iface=p[2];
  }else if(p[1]==0x21&&candidate){
   if(z<6||p[5]>(z-6)/3)return 0;
  }else if(p[1]==5&&candidate&&!selected){
   if(z<7)return 0;
   if((p[2]&128)&&(p[2]&15)&&(p[3]&3)==3){
    unsigned bit=1u<<(p[2]&15),ib=1u<<(iface%32);
    if((endpoints&bit)||(interfaces[iface/32]&ib))return 0;
    endpoints|=bit;interfaces[iface/32]|=ib;selected=1;
   }
  }
  at+=z;
 }
 return 1;
}
int hid_configuration_at_speed(const u8 *d,unsigned n,unsigned speed,HidInterface *out,unsigned capacity,unsigned *config,int *hub)
{
 if(!out||!capacity||!validate(d,n))return -1;
 int count=tmc_linux_usb_config_select(d,n,speed,out,capacity,config,hub);return count<0?-1:count;
}
int hid_interfaces_at_speed(const u8 *d,unsigned n,unsigned speed,HidInterface *out,unsigned capacity)
{
 return hid_configuration_at_speed(d,n,speed,out,capacity,0,0);
}
int hid_interfaces(const u8 *d,unsigned n,HidInterface *out,unsigned capacity)
{
 /* Standalone legacy callers did not supply bus speed. Actual HCD callers
  * use the speed-aware entry point below; this keeps the old 1024-byte cap. */
 return hid_interfaces_at_speed(d,n,3,out,capacity);
}
