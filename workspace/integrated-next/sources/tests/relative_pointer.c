#include "hidtablet.h"
#include "pointer_scale.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Non-boot USB mouse: 5 buttons, signed 16-bit X/Y, wheel, Report ID 4. */
static const u8 desc[]={
 5,1,9,2,0xa1,1,0x85,4,9,1,0xa1,0,
 5,9,0x19,1,0x29,5,0x15,0,0x25,1,0x75,1,0x95,5,0x81,2,
 0x75,3,0x95,1,0x81,1,
 5,1,9,0x30,9,0x31,0x16,0x01,0x80,0x26,0xff,0x7f,0x75,16,0x95,2,0x81,6,
 9,0x38,0x15,0x81,0x25,0x7f,0x75,8,0x95,1,0x81,6,0xc0,0xc0
};
int main(void){
 PointerScale scale={0};int x=0,y=0;
 for(int i=0;i<300;i++){x+=pointer_scale_axis(1,1280,1920,&scale.x);y+=pointer_scale_axis(0,800,1080,&scale.y);}
 assert(x==200&&y==0&&scale.y==0);
 for(int i=0;i<300;i++)x+=pointer_scale_axis(-1,1280,1920,&scale.x);
 assert(x==0&&y==0);
 TabletLayout t;TabletState s;
 assert(pointer_layout(desc,sizeof desc,64,&t)&&t.relative&&t.bytes==7&&t.id==4);
 assert(!tablet_layout(desc,sizeof desc,64,&t));
 assert(pointer_layout(desc,sizeof desc,64,&t));
 u8 p[]={4,5,0x34,0x12,0xfe,0xff,0xff};
 assert(pointer_read(&t,p,sizeof p,1280,800,&s));
 assert(s.x==0x1234&&s.y==-2&&s.buttons==5&&s.wheel==-1);
 assert(!tablet_read(&t,p,sizeof p,1280,800,&s));
 p[0]=3;assert(!pointer_read(&t,p,sizeof p,1280,800,&s));p[0]=4;
 for(unsigned n=0;n<t.bytes;n++)assert(!pointer_read(&t,p,n,1280,800,&s));
 p[2]=1;p[3]=0x80;p[4]=0xff;p[5]=0x7f;
 assert(pointer_read(&t,p,7,1280,800,&s)&&s.x==-32767&&s.y==32767);
 for(unsigned n=0;n<sizeof desc;n++)assert(!pointer_layout(desc,n,64,&t));
 /* Other report IDs, vendor usages and a restored global stack do not
  * alter the mouse's bit offsets. Extended usage items override global page. */
 u8 multi[256];unsigned z=0;
 const u8 vendor[]={0x06,0,0xff,0x09,1,0xa1,1,0x85,7,0x75,8,0x95,12,0x81,2,0xc0};
 memcpy(multi,vendor,sizeof vendor);z+=sizeof vendor;
 memcpy(multi+z,desc,sizeof desc);z+=sizeof desc;
 assert(pointer_layout(multi,z,64,&t)&&t.id==4&&t.x.bit==16&&t.y.bit==32);
 u8 horizontal[]={4,0,64,0,0,0,0};
 assert(pointer_read(&t,horizontal,7,1280,800,&s)&&s.x==64&&s.y==0);
 horizontal[2]=0;horizontal[4]=64;
 assert(pointer_read(&t,horizontal,7,1280,800,&s)&&s.x==0&&s.y==64);
 u8 stackdesc[256];memcpy(stackdesc,desc,12);
 const u8 push[]={0xa4,0x06,0,0xff,0x75,32,0x95,31,0xb4};
 memcpy(stackdesc+12,push,sizeof push);memcpy(stackdesc+12+sizeof push,desc+12,sizeof desc-12);
 assert(pointer_layout(stackdesc,sizeof desc+sizeof push,64,&t)&&t.x.bit==16&&t.y.bit==32);
 puts("PASS independent axes with other Report IDs and bounded HID global Push/Pop");
 puts("PASS: report-protocol relative mouse, signed 16-bit deltas, report ID, buttons, wheel and truncated-report rejection.");
}
