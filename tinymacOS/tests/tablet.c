/* Host-only unit cases for the same bounded HID reader linked into the kernel
 * and Recovery. The full descriptor on an actual usb-tablet is tested in QEMU. */
#include "hidtablet.h"
#include <assert.h>
#include <stdio.h>
/* Three buttons + padding, one absolute XY pair, and one signed relative wheel. */
static const u8 descriptor[]={
    5,1, 9,2, 0xa1,1, 9,1, 0xa1,0, 5,9, 0x19,1, 0x29,3,
    0x15,0, 0x25,1, 0x95,3, 0x75,1, 0x81,2, 0x95,1, 0x75,5, 0x81,1,
    5,1, 9,0x30, 9,0x31, 0x15,0, 0x26,0xff,0x7f, 0x75,16, 0x95,2, 0x81,2,
    9,0x38, 0x15,0x81, 0x25,0x7f, 0x75,8, 0x95,1, 0x81,6, 0xc0,0xc0
};
int main(void)
{
    TabletLayout t;TabletState p;
    assert(tablet_layout(descriptor,sizeof descriptor,8,&t) && t.bytes==6 && !t.id);
    assert(t.x.bit==8 && t.y.bit==24 && t.wheel.bit==40);
    const unsigned screens[][2]={{1280,800},{1024,768},{800,600},{1920,1080},{1,1}};
    for(unsigned i=0;i<COUNT(screens);i++) {
        int w=screens[i][0],h=screens[i][1];
        for(int x=0;x<2;x++) for(int y=0;y<2;y++) for(int buttons=0;buttons<8;buttons++) {
            u8 r[6]={0xf8|buttons,x?255:0,x?127:0,y?255:0,y?127:0,0xff};
            assert(tablet_read(&t,r,6,w,h,&p));
            assert(p.x==(x?w-1:0) && p.y==(y?h-1:0) && p.buttons==buttons && p.wheel==-1);
        }
    }
    u8 r[7]={0,0,64,0,64,1,0};assert(tablet_read(&t,r,6,1280,800,&p));assert(p.x==640&&p.y==400&&p.wheel==1);
    r[1]=r[2]=r[3]=r[4]=255;assert(tablet_read(&t,r,6,1280,800,&p)&&p.x==1279&&p.y==799);
    for(unsigned n=0;n<6;n++) assert(!tablet_read(&t,r,n,1280,800,&p));
    assert(!tablet_read(&t,r,6,0,800,&p));
    for(unsigned n=0;n<sizeof descriptor;n++) assert(!tablet_layout(descriptor,n,8,&t));
    assert(!tablet_layout(descriptor,sizeof descriptor,5,&t));
    u8 v[sizeof descriptor+2];v[0]=0x85;v[1]=7;memcpy(v+2,descriptor,sizeof descriptor);
    assert(tablet_layout(v,sizeof v,8,&t)&&t.id==7&&t.bytes==7);
    assert(!tablet_read(&t,r,7,1280,800,&p));r[0]=7;assert(tablet_read(&t,r,7,1280,800,&p));
    memcpy(v,descriptor,sizeof descriptor);v[3]=6;assert(!tablet_layout(v,sizeof descriptor,8,&t)); // Keyboard collection
    memcpy(v,descriptor,sizeof descriptor);v[45]=6;assert(!tablet_layout(v,sizeof descriptor,8,&t)); // Relative X/Y
    assert(pointer_pixel(100,100,500,1024)==0 && pointer_pixel(300,100,500,1024)==512 && pointer_pixel(500,100,500,1024)==1023);
    assert(pointer_pixel(0,0,UINT64_MAX,1280)==0 && pointer_pixel(UINT64_MAX/2,0,UINT64_MAX,1280)==639 && pointer_pixel(UINT64_MAX,0,UINT64_MAX,1280)==1279);
    puts("HID: 160 combinazioni angoli/pulsanti, 5 geometrie, centro, limiti, rotella con segno, report ID e rifiuto dei descrittori non supportati: OK");
}
