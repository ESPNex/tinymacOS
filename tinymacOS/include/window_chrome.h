#ifndef TMC_WINDOW_CHROME_H
#define TMC_WINDOW_CHROME_H
/* Shared geometry for compositor windows and standalone UEFI Recovery panels. */
#define TMC_CHROME_TITLE 43
#define TMC_CHROME_TEXT_Y 12
#define TMC_CHROME_RADIUS 18
#define TMC_CHROME_LIGHT_SIZE 13
#define TMC_CHROME_LIGHT_X 17
#define TMC_CHROME_LIGHT_Y 16
#define TMC_CHROME_LIGHT_STEP 20
static inline unsigned tmc_chrome_light(unsigned k)
{
    const unsigned colors[3]={0xff695e,0xffbd45,0x32c96b};
    return colors[k%3];
}
static inline int tmc_chrome_coverage(int x,int y,int w,int h,int r)
{
    if(x<0||y<0||x>=w||y>=h)return 0;
    if(r<=0)return 255;
    int a=x<w-1-x?x:w-1-x,b=y<h-1-y?y:h-1-y;
    if(a>=r||b>=r)return 255;
    int hits=0;
    for(int sy=0;sy<4;sy++)for(int sx=0;sx<4;sx++){
        int dx=r*8-(a*8+2*sx+1),dy=r*8-(b*8+2*sy+1);
        hits+=dx*dx+dy*dy<=r*r*64;
    }
    return (hits*255+8)/16;
}
#endif
