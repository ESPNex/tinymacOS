#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "../desktop/gfx.h"
void glass_reference(Rect,int,int);
static u32 original[1920*1080],expected[1920*1080];
static unsigned seed=17;
static unsigned random_pixel(void){seed=seed*1664525u+1013904223u;return seed&0xffffff;}
int main(void)
{
    W=1280;H=800;resetclip();
    for(int i=0;i<W*H;i++)original[i]=random_pixel();
    unsigned cases=0;
    for(int dark=0;dark<2;dark++)for(int radius=0;radius<=60;radius+=6)
    for(int k=0;k<4;k++){
        Rect q=k==0?(Rect){140,90,688,469}:k==1?(Rect){-23,-12,377,281}:k==2?(Rect){1180,740,200,180}:(Rect){90,150,51,43};
        night=dark;lightx=k*270;lite=0;recovery_mode=0;
        memcpy(canvas,original,W*H*4);glass_reference(q,radius,MAT_WINDOW);memcpy(expected,canvas,W*H*4);
        memcpy(canvas,original,W*H*4);glass(q,radius,MAT_WINDOW);
        assert(!memcmp(canvas,expected,W*H*4));cases++;
    }
    Rect q={140,90,688,469};night=0;resetclip();
    double times[2];
    for(int mode=0;mode<2;mode++){
        clock_t start=clock();
        for(int i=0;i<30;i++){
            memcpy(canvas,original,W*H*4);
            if(mode)glass(q,22,MAT_SHEET);else glass_reference(q,22,MAT_SHEET);
        }
        times[mode]=(double)(clock()-start)/CLOCKS_PER_SEC;
    }
    printf("PASS glass: %u full-frame bit-exact comparisons; 30 library panels CPU reference=%.6f optimized=%.6f speedup=%.2fx (host only)\n",cases,times[0],times[1],times[0]/times[1]);
}
