#pragma once
#include "base.h"
/* One activation for each pair, never for every click after the first.
 * Time is the input occurrence time (100 Hz), not the paint/delivery time.
 * target must identify the item as well as its containing window/directory. */
typedef struct { u64 time, target; int x,y,armed; } ClickPair;
static inline int click_pair(ClickPair *p,u64 now,u64 target,int x,int y)
{
    int pair=p->armed && p->target==target && now>=p->time && now-p->time<=50 &&
        (i64)x-p->x>=-8 && (i64)x-p->x<=8 && (i64)y-p->y>=-8 && (i64)y-p->y<=8;
    *p=(ClickPair){.time=now,.target=target,.x=x,.y=y,.armed=!pair};
    return pair;
}
