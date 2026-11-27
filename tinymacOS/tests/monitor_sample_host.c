#include <assert.h>
#include <stdio.h>
#include "../userspace/ui.h"
static SysInfo fixture;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d)
{
    (void)b;(void)c;(void)d;
    if(n==S_INFO){*(SysInfo*)a=fixture;return 0;}
    assert(n==S_PS);return 0;
}
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/monitor.c"
int aw=800,ah=600;SysInfo si;
int main(void)
{
    fixture.ncpu=4;fixture.cpu_online_mask=15;fixture.ticks=100;
    for(int c=0;c<4;c++){fixture.core_ticks[c]=100;fixture.core_busy[c]=20;}
    sample();assert(cpu==0&&!hlen&&lastTick==100);
    fixture.ticks=200;fixture.busy_ticks=100;
    for(int c=0;c<4;c++)fixture.core_ticks[c]+=100;
    fixture.core_busy[0]+=100;sample();
    assert(cpu==25&&corep[0]==100&&corep[1]==0&&hlen==1&&lastTick==200);
    for(int c=0;c<4;c++){fixture.core_ticks[c]+=100;fixture.core_busy[c]+=100;}
    fixture.ticks+=100;sample();assert(cpu==100&&hlen==2);
    for(int c=0;c<4;c++){fixture.core_ticks[c]=1;fixture.core_busy[c]=0;}
    fixture.ticks=1;sample();assert(cpu==0&&hlen==2);
    fixture.ncpu=1;fixture.cpu_online_mask=1;fixture.core_ticks[0]=101;fixture.core_busy[0]=50;
    sample();assert(cpu==0&&!corep[1]&&!ptick[1]);
    fixture.core_ticks[0]+=100;fixture.core_busy[0]+=50;sample();assert(cpu==50);
    resume();assert(cpu==0);
    puts("PASS actual Monitor sample(): 4-core aggregate 25%/100%, per-core bars, reset handling, offline core baseline, sample clock maintained");
}
