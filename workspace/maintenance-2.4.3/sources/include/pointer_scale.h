#pragma once
/* Relative deltas are distances, not absolute coordinates. Retain signed
 * remainders, otherwise every +/-1 physical report becomes zero at 1080p.
 * State belongs to the compositor and is reset on a mode change. */
typedef struct { int x, y; } PointerScale;
static inline int pointer_scale_axis(int delta,int logical,int physical,int *remainder)
{
    if(logical<=0 || physical<=0){*remainder=0;return 0;}
    long long n=(long long)delta*logical+*remainder;
    int out=(int)(n/physical);
    *remainder=(int)(n-(long long)out*physical);
    return out;
}
