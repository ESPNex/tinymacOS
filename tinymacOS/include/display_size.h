#pragma once
#include "base.h"
/* Decimal SI units. Integer-only, bounded output; no overflowing bytes*10.
 * Always one decimal above B, truncation (not rounding past a boundary). */
static inline void display_size(char *out,int cap,u64 bytes)
{
    static const char *units[]={" B"," kB"," MB"," GB"," TB"," PB"," EB"};
    u64 scale=1;int unit=0;char digits[32];
    while(unit<6&&bytes/scale>=1000){scale*=1000;unit++;}
    number(digits,bytes/scale);strset(out,digits,cap);
    if(unit){char fraction[3]={'.',(char)('0'+(bytes%scale)/(scale/10)),0};append(out,fraction,cap);}
    append(out,units[unit],cap);
}

static inline void display_usage(char *out,int cap,u64 used,u64 total)
{
    char capacity[40];display_size(out,cap,used);display_size(capacity,sizeof capacity,total);
    append(out," su ",cap);append(out,capacity,cap);
}
