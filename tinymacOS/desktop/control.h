#pragma once
#include "gfx.h"
/* Virtual 376 x 550 layout. Drawing and hit testing share these exact bounds. */
enum {CC_APPEAR,CC_MATERIAL,CC_MOTION,CC_DOCK,CC_WALL,CC_MISSION,CC_LOCK,
      CC_THEME_SLIDER,CC_CALC,CC_CALENDAR,CC_TERMINAL,CC_ALL_SETTINGS,CC_COUNT};
static inline Rect control_bounds(int width,int height) {
    int h=MIN(550,height-34-26),w=376*h/550;
    return (Rect){width-w-18,46,w,h};
}
static inline Rect control_rect(Rect q,int id) {
    static const short box[CC_COUNT][4]={
      {0,28,180,78},{0,122,78,78},{102,122,78,78},{0,224,180,78},
      {196,28,180,172},{196,224,78,78},{298,224,78,78},
      {0,318,376,80},
      {24,414,64,64},{156,414,64,64},{288,414,64,64},
      {78,516,220,28}};
    if(id<0||id>=CC_COUNT)return (Rect){0};
    const short *b=box[id];return (Rect){q.x+b[0]*q.w/376,q.y+b[1]*q.h/550,b[2]*q.w/376,b[3]*q.h/550};
}
static inline int control_radius(Rect q,int id) {
    Rect b=control_rect(q,id);int r=MIN(b.w,b.h)/2;
    if(id==CC_WALL||id==CC_THEME_SLIDER)r=MAX(1,28*q.h/550);
    return r;
}
static inline int control_hit(Rect q,int x,int y,int id) {
    Rect b=control_rect(q,id);if(!inside(x,y,b))return 0;
    int r=control_radius(q,id);
    int dx=MAX(0,MAX(b.x+r-x,x-(b.x+b.w-r-1)));
    int dy=MAX(0,MAX(b.y+r-y,y-(b.y+b.h-r-1)));
    return dx*dx+dy*dy<=r*r;
}
static inline int control_slider_value(Rect b,int x,int count) {
    int pad=MAX(25,b.w*44/376),len=MAX(1,b.w-2*pad);
    return clampi(((x-b.x-pad)*(count-1)+len/2)/len,0,count-1);
}
void control_draw(Rect);
void control_click(Rect,int,int);
void control_pointer(Rect,int);
void control_cancel(void);
int control_key(Rect,Event *);
