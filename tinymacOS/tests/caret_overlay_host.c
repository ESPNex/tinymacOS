#include <assert.h>
#include <stdio.h>
#include "../desktop/desk.h"
#include "../include/caret.h"
View views[NPROC];SysInfo sys;
u32 canvas[1920*1080];int W=640,H=480,mc,gate,cursorDirty;
static int drag=-1,mcdim,selected,overlay;
static struct {int started;} filedrag;
static u64 caret_activity;
static int curx,cury,curw,curh,scans;
static unsigned scanned_pixels;
int topwin(void){return selected;}
int shell_overlay_open(void){return overlay;}
int recovery_overlay_open(void){return overlay;}
u32 color(u32 v){return v;}
static void undocursor(void){}
static void drawcursor(void){}
static void scanrect(int x,int y,int w,int h){assert(x>=0&&y>=0&&x+w<=W&&y+h<=H);scans++;scanned_pixels+=w*h;}
#include "../desktop/caret_overlay.h"
int main(void)
{
    assert(caret_phase(100,100,0)&&caret_phase(155,100,0));
    assert(!caret_phase(156,100,0)&&caret_phase(212,100,0));
    assert(caret_phase(1,100,0)&&caret_phase(156,100,1));
    for(int i=0;i<W*H;i++)canvas[i]=0x112233;
    View *v=views;v->box=(Rect){20,40,300,220};v->s.pid=42;v->used=1;v->n=1;
    v->cmd[0]=(Draw){.type=C_CARET,.x=10,.y=20,.w=2,.h=16,.ink=0xaabbcc};
    sys.ticks=100;assert(caret_pending());caret_paint();assert(caret_valid);
    int x=31,y=40+TITLE+20;assert(canvas[y*W+x]==0xaabbcc);
    sys.ticks=155;assert(!caret_pending());sys.ticks=156;assert(caret_pending());
    caret_present_partial();assert(!caret_valid&&canvas[y*W+x]==0x112233&&scans==1&&scanned_pixels==32);
    /* Input at an invisible phase resets it immediately, without app redraw. */
    caret_activity=sys.ticks;assert(caret_pending());caret_present_partial();assert(caret_valid);
    v->cmd[0].x=30;assert(caret_pending());caret_present_partial();assert(canvas[y*W+x]==0x112233&&canvas[y*W+x+20]==0xaabbcc);
    overlay=1;assert(caret_pending());caret_present_partial();assert(!caret_valid);overlay=0;
    assert(caret_pending());caret_present_partial();assert(caret_valid);
    selected=1;assert(caret_pending());caret_present_partial();assert(!caret_valid);selected=0;
    sys.reduce_motion=1;sys.ticks+=500;caret_paint();assert(caret_valid);caret_erase();
    v->cmd[0].w=9;Rect box;u32 ink;assert(!caret_pick(&box,&ink));
    v->cmd[0].w=2;v->cmd[0].x=299;assert(!caret_pick(&box,&ink));
    v->cmd[0].x=10;gate=1;assert(!caret_pick(&box,&ink));gate=0;
    mc=1;assert(!caret_pick(&box,&ink));mc=0;
    v->motion.kind=AN_OPEN;assert(!caret_pick(&box,&ink));v->motion.kind=0;
    assert(scanned_pixels<1000);
    puts("PASS real caret overlay: input-visible phase, blink, reduce-motion, reset counter, focus/modal/lock/Mission Control/motion suppression, bounded geometry, exact background restoration and tiny partial damage only");
}
