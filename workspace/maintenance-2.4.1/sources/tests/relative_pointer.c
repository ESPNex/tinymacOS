#include "hidtablet.h"
#include <assert.h>
#include <stdio.h>
/* Non-boot USB mouse: 5 buttons, signed 16-bit X/Y, wheel, Report ID 4. */
static const u8 desc[]={
 5,1,9,2,0xa1,1,0x85,4,9,1,0xa1,0,
 5,9,0x19,1,0x29,5,0x15,0,0x25,1,0x75,1,0x95,5,0x81,2,
 0x75,3,0x95,1,0x81,1,
 5,1,9,0x30,9,0x31,0x16,0x01,0x80,0x26,0xff,0x7f,0x75,16,0x95,2,0x81,6,
 9,0x38,0x15,0x81,0x25,0x7f,0x75,8,0x95,1,0x81,6,0xc0,0xc0
};
int main(void){
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
 puts("PASS: report-protocol relative mouse, signed 16-bit deltas, report ID, buttons, wheel and truncated-report rejection.");
}
