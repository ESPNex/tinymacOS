#include "hidconfig.h"
#include <assert.h>
#include <stdio.h>
/* Composite boot keyboard+boot mouse+nonboot HID; each keeps its own endpoint. */
static const u8 config[]={
9,2,84,0,3,1,0,0x80,50,
9,4,0,0,1,3,1,1,0, 9,0x21,0x11,1,0,1,0x22,63,0, 7,5,0x81,3,8,0,10,
9,4,1,0,1,3,1,2,0, 9,0x21,0x11,1,0,1,0x22,52,0, 7,5,0x82,3,8,0,10,
9,4,2,0,1,3,0,0,0, 9,0x21,0x11,1,0,1,0x22,67,0, 7,5,0x83,3,0,2,4};
int main(void){
 HidInterface f[4];assert(sizeof config==84);
 assert(hid_interfaces(config,sizeof config,f,4)==3);
 assert(f[0].iface==0&&f[0].proto==1&&f[0].ep==3);
 assert(f[1].iface==1&&f[1].proto==2&&f[1].ep==5);
 assert(f[2].iface==2&&f[2].proto==3&&f[2].ep==7&&f[2].packet==512&&f[2].report_length==67);
 assert(hid_interfaces(config,sizeof config,f,1)==1);
 for(unsigned i=0;i<sizeof config;i++)assert(hid_interfaces(config,i,f,4)==-1);
 u8 bad[sizeof config];memcpy(bad,config,sizeof bad);bad[9]=1;assert(hid_interfaces(bad,sizeof bad,f,4)==-1);
 memcpy(bad,config,sizeof bad);bad[54]=0x81;assert(hid_interfaces(bad,sizeof bad,f,4)==-1); /* duplicate endpoint */
 memcpy(bad,config,sizeof bad);bad[37]=1;assert(hid_interfaces(bad,sizeof bad,f,4)==2); /* alternate setting skipped */
 puts("PASS: composite HID keyboard/mouse/report interfaces, independent endpoints, 512-byte endpoint, bounds and alternate-setting rejection.");
}
