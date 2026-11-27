#pragma once
/* Keep real elapsed time, bounded by the longest active desktop transition.
 * Clamping a delayed frame to 7/8 ticks makes load lengthen every animation. */
static inline int animation_delta(u64 now,u64 last,int remaining)
{
    if(now<=last||remaining<=0)return 0;
    u64 elapsed=now-last;
    return elapsed>(u64)remaining?remaining:(int)elapsed;
}
