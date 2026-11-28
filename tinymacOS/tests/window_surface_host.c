#include <assert.h>
#include <stdio.h>
#include "../desktop/desk.h"
static u32 pixels[100*100],undo[100*100];static int paints;
static void dropbuf(u32 **p){*p=0;}
static u32 *buf(int w,int h){assert(w*h<=10000);return pixels;}
static u64 hash(u64 a,u64 b){return a*131+b;}
static u64 geo(u64 h,Rect q){return hash(hash(hash(hash(h,q.x),q.y),q.w),q.h);}
void copyout(Rect q,u32 *p){(void)q;(void)p;}
void copyin(Rect q,const u32 *p){(void)q;(void)p;}
void glass_project_backdrop(Rect a,Rect b,int on){(void)a;(void)b;(void)on;}
static void wincontent(View *v,int focus){(void)v;(void)focus;paints++;}
static int interacting;
int window_interacting(const View*v){(void)v;return interacting;}
#include "../desktop/window_surface.h"
int main(void)
{
    View v={0};v.box=(Rect){10,20,80,80};v.s.pid=12;v.s.seq=5;
    assert(cache(&v,100,1)&&paints==1);assert(cache(&v,100,1)&&paints==1);
    v.motion.kind=AN_MC;
    for(int i=1;i<=24;i++)assert(cache(&v,100+i,1));
    assert(paints==25);v.s.seq++;assert(cache(&v,130,1)&&paints==26);
    assert(cache(&v,131,0)&&paints==27);
    v.motion.kind=0;assert(cache(&v,150,0)&&paints==28);
    v.box.x++;assert(cache(&v,151,0)&&paints==29);
    v.box.w--;assert(cache(&v,152,0)&&paints==30);
    interacting=1;for(int j=0;j<100;j++){v.box.x++;v.box.w=70+j%5;assert(cache(&v,200+j,0));}assert(paints==30);
    v.s.seq++;assert(cache(&v,400,0)&&paints==31);interacting=0;assert(cache(&v,401,0)&&paints==32);
    puts("PASS production surface cache: 24 MC backdrop phases re-render instead of freezing old refractions; live content/focus/settled backdrop/drag/resize invalidate");
}
