#pragma once
#include "base.h"
/* Shared timing policy for text insertion markers. Visible immediately after
 * input/focus, then a 56-tick half-period; reset/future counters stay visible. */
#define CARET_HALF_PERIOD 56u
static inline int caret_phase(u64 now,u64 activity,int reduce_motion)
{
    return reduce_motion||now<activity||!(((now-activity)/CARET_HALF_PERIOD)&1);
}
static inline int caret_dimensions(int w,int h){return w>=1&&w<=8&&h>=1&&h<=32;}
