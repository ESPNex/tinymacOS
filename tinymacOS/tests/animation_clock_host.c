#include <assert.h>
#include <stdio.h>
#include "../desktop/desk.h"
View views[NPROC];SysInfo sys;int dirty;
int W=1280,H=800,x1,y1,x2=1280,y2=800;u32 canvas[1920*1080];
int main(void)
{
    assert(animation_delta(100,100,64)==0);
    assert(animation_delta(90,100,64)==0);
    assert(animation_delta(~0ull,1,64)==64);
    assert(animation_delta(117,100,64)==17);
    View v={0};v.box=(Rect){10,40,600,400};Rect end={400,160,240,160},q;int a,b;
    sys.ticks=100;startmotion(&v,AN_MC,v.box,end,24);
    sys.ticks=120;assert(motionstep(&v,&q,&a,&b)<256&&v.motion.age==20);
    sys.ticks=124;assert(motionstep(&v,&q,&a,&b)==256);
    assert(q.x==end.x&&q.y==end.y&&q.w==end.w&&q.h==end.h);
    sys.ticks=200;startmotion(&v,AN_MIN,v.box,end,43);
    sys.ticks=500;assert(motionstep(&v,&q,&a,&b)==256&&v.motion.age==43);
    sys.ticks=600;startmotion(&v,AN_OPEN,end,v.box,31);sys.reduce_motion=1;
    assert(motionstep(&v,&q,&a,&b)==256&&a==255);
    sys.reduce_motion=0;
    const int spans[]={0,31,22,43,36,32,24};
    for(int kind=AN_OPEN;kind<=AN_MC;kind++){
        sys.ticks=1000;startmotion(&v,kind,v.box,end,spans[kind]);
        sys.ticks+=spans[kind]+100;
        assert(motionstep(&v,&q,&a,&b)==256);
        assert(q.x==end.x&&q.y==end.y&&q.w==end.w&&q.h==end.h);
        motionfinish(&v);assert(!v.motion.kind);
    }
    puts("PASS real motionstep: delayed frames retain elapsed time, exact endpoints, long stall, reduced motion");
}
