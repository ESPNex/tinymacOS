/* Actual Recovery background generator and cached framebuffer restore. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../recovery/screen.c"
static void geometry(int w,int h)
{
    rw=w;rh=h;
    unsigned low=255,high=0;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        u32 c=rec_background_pixel(x,y,w,h);unsigned v=c&255;
        assert(c==v*0x010101&&v>=3&&v<=108);
        low=MIN(low,v);high=MAX(high,v);
        int mirror=(int)(rec_background_pixel(w-1-x,y,w,h)&255)-(int)v;
        assert(mirror>=-1&&mirror<=1);
    }
    assert(low==3&&high>=106);
    assert((rec_background_pixel(0,0,w,h)&255)<=4);
    assert((rec_background_pixel(w/2,h*3/8,w,h)&255)>=105);
    int previous=0;
    for(int x=0;x<w/2;x++){
        int gray=rec_background_pixel(x,h*3/8,w,h)&255;
        assert(gray>=previous-1);previous=gray;
    }
    printf("PASS neutral soft radial background %dx%d: black corners, upper-center glow, smooth falloff, symmetric to one gray level\n",w,h);
}
int main(void)
{
    assert(rec_background_pixel(0,0,1,1)==0x030303);
    geometry(800,600);geometry(1024,768);geometry(1280,800);geometry(1280,1024);
    rw=1280;rh=800;pix=malloc(rw*rh*sizeof *pix);backdrop=malloc(rw*rh*sizeof *backdrop);assert(pix&&backdrop);
    for(int y=0;y<rh;y++)for(int x=0;x<rw;x++)backdrop[y*rw+x]=rec_background_pixel(x,y,rw,rh);
    for(int frame=0;frame<64;frame++){
        fill(0,0,rw,rh,0,0xffffff);screen_background();assert(!memcmp(pix,backdrop,rw*rh*sizeof *pix));
    }
    free(pix);free(backdrop);pix=backdrop=0;
    puts("PASS actual screen_background: 64 cached restores erase prior frame without recomputing the gradient");
}
