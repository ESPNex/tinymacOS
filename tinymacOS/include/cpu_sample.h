#pragma once
#include "base.h"
/* Percent of scheduled non-idle timer samples, not cycles/instructions.
 * Binary threshold calculation avoids overflow in busy*100. */
static inline int cpu_percent(u64 busy,u64 total)
{
    if(!total)return 0;
    if(busy>=total)return 100;
    int lo=0,hi=100;
    while(lo<hi){
        int p=(lo+hi+1)/2;
        u64 threshold=(total/100)*p+((total%100)*p+99)/100;
        if(busy>=threshold)lo=p;else hi=p-1;
    }
    return lo;
}
static inline int cpu_interval(u64 tick,u64 busy,u64 oldtick,u64 oldbusy,u64 *dt,u64 *db)
{
    *dt=*db=0;
    if(!oldtick||tick<=oldtick||busy<oldbusy)return 0;
    u64 t=tick-oldtick,b=busy-oldbusy;
    /* Reject inconsistent/non-atomic snapshots; never display an underflow.
     * Bound aggregate addition for the eight exported cores. */
    if(b>t||t>(~(u64)0)/8)return 0;
    *dt=t;*db=b;return 1;
}
