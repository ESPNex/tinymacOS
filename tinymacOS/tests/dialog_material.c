/* Execute the shipping glass shader on a deterministic host framebuffer.
 * Only raster clipping/palette imports are mocked. This is a material equality
 * contract, not a guest or GPU/hardware test. */
#include "../desktop/gfx.h"
#include <assert.h>
#include <stdio.h>
_Static_assert(MAT_DIALOG==MAT_DOCK,"File panels must share Control Center material");
int W=320,H=240,PW=320,PH=240,night,lite,lightx=160,lighty=30;
int x1,y1,x2=320,y2=240;
u32 canvas[1920*1080],back[1920*1080];
static u32 expected[320*240];
u32 color(u32 c){
    if(c==INK(I_PANEL))return night?0x303441:0xe6ebf5;
    if(c==INK(I_BG))return night?0x171b25:0xf5f7fb;
    return c;
}
int coverage(int x,int y,int w,int h,int r){(void)r;return x>=0&&y>=0&&x<w&&y<h?255:0;}
void roundr(int x,int y,int w,int h,int r,u32 c,int a){
    (void)r;c=color(c);
    for(int j=MAX(0,y);j<MIN(H,y+h);j++)for(int i=MAX(0,x);i<MIN(W,x+w);i++)canvas[j*W+i]=blend(canvas[j*W+i],c,a);
}
#include "../desktop/glass.c"
static void scene(void){for(int y=0;y<H;y++)for(int x=0;x<W;x++)canvas[y*W+x]=((x*3+y)&255)<<16|((y*7+x)&255)<<8|((x+y*5)&255);}
int main(void){
    Rect q={45,35,220,170};
    for(night=0;night<2;night++)for(lite=0;lite<2;lite++){
        scene();glass(q,14,MAT_DOCK);memcpy(expected,canvas,W*H*sizeof(u32));
        scene();glass(q,14,MAT_DIALOG);assert(!memcmp(expected,canvas,W*H*sizeof(u32)));
    }
    puts("PASS actual glass shader: file panel and Control Center material are pixel-identical in light/dark and opaque fallback; no independent alpha");
}
