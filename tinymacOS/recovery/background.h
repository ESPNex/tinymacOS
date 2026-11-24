#pragma once
#include "base.h"
/* Original procedural Recovery backdrop: neutral white light on near-black.
 * Elliptic radial falloff, centered slightly above mid-screen. Q10/Q8 fixed
 * point and sub-LSB ordered dithering avoid libm, image assets and banding.
 * Called once while creating the cached framebuffer, never by input polling. */
static inline u32 rec_background_pixel(int x,int y,int w,int h)
{
    if(w<2||h<2)return 0x030303;
    i64 dx=((i64)2*x-(w-1))*1024/(w-1);
    i64 dy=((i64)2*y-(i64)(h-1)*3/4)*1024/(h-1);
    u64 radius=(u64)(dx*dx+dy*dy)>>10;
    u64 t=radius>=1024?0:1024-radius;
    u64 q8=3*256+104*t*t*t*256/(1024ULL*1024*1024);
    static const u8 bayer[16]={0,128,32,160,192,64,224,96,48,176,16,144,240,112,208,80};
    unsigned gray=(unsigned)(q8>>8)+((q8&255)>bayer[(y&3)*4+(x&3)]);
    return gray*0x010101;
}
