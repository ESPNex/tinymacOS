/* Single-line proportional viewport. Offsets are UTF-8 byte boundaries.
 * Painting and hit-testing MUST use the same advance callback and origin. */
#pragma once
#include "text_input.h"
typedef int (*FieldAdvance)(const char *,int,int);
static inline int field_boundary(const char *s,int at)
{
    at=MAX(0,MIN(at,(int)strlen(s)));
    while(at>0&&((u8)s[at]&0xc0)==0x80)at--;
    return at;
}
static inline int field_start(const char *s,int at,int width,FieldAdvance adv)
{
    int used=0,start=field_boundary(s,at),limit=MAX(0,width-1);
    while(start>0){int p=text_prev(s,start),w=MAX(0,adv(s,p,start));
        if(w>limit-used)break;
        used+=w;start=p;
    }
    return start;
}
static inline int field_hit(const char *s,int at,int width,int pixel,FieldAdvance adv)
{
    int i=field_start(s,at,width,adv),x=0;
    pixel=MAX(0,MIN(pixel,MAX(0,width-1)));
    while(s[i]){int n=text_next(s,i),w=MAX(0,adv(s,i,n));
        if(pixel<x+(w+1)/2)return i;
        x+=w;i=n;if(x>pixel)return i;
    }
    return i;
}
