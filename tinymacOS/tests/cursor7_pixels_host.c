#include <assert.h>
#include <stdio.h>
#include "../desktop/gfx.h"
static u32 rgba[1024],before[64*64];static u8 invert[1024];
int main(void)
{
 W=H=64;resetclip();for(int i=0;i<4096;i++)canvas[i]=before[i]=0x123456;
 rgba[0]=0xffff0000;rgba[1]=0x8000ff00;invert[2]=1;cursor_pixels(rgba,invert,16,16);
 assert(canvas[16*64+16]==0xff0000);assert(canvas[16*64+17]==mix(0x123456,0x00ff00,128));assert(canvas[16*64+18]==(0x123456^0xffffff));assert(canvas[16*64+19]==0x123456);
 Rect damage={16,16,32,32};u32 save[1024];copyout(damage,save);copyin(damage,save);
 for(int y=-31;y<64;y+=7)for(int x=-31;x<64;x+=7){for(int i=0;i<4096;i++)canvas[i]=before[i];cursor_pixels(rgba,invert,x,y);
  for(int yy=0;yy<64;yy++)for(int xx=0;xx<64;xx++)if(xx<x||xx>=x+32||yy<y||yy>=y+32)assert(canvas[yy*64+xx]==before[yy*64+xx]);}
 puts("PASS actual compositor cursor pixels: exact opaque/alpha/invert operations, fully transparent pixels unchanged, every screen edge clipped without stray damage");
}
