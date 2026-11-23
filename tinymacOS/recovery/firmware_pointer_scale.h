#pragma once
#include "base.h"
/* UEFI SimplePointer deltas are counts, not pixels. Eight pixels/mm is a
 * device-independent UI sensitivity. Keep signed fractional pixels per axis
 * and source, including the full UINT64 counts/mm range without overflow. */
typedef struct { u64 carry,resolution; int sign; } RecPointerAxis;
static inline int rec_pointer_pixels(RecPointerAxis *axis,int32_t delta,u64 resolution)
{
    if(axis->resolution!=resolution){axis->carry=0;axis->sign=0;axis->resolution=resolution;}
    if(!resolution){axis->carry=0;axis->sign=0;return 0;} /* unsupported axis */
    if(!delta)return 0;
    int sign=delta<0?-1:1;
    u64 units=(u64)(delta<0?-(i64)delta:(i64)delta)*8;
    u64 pixels=units/resolution,rem=units%resolution;
    if(axis->carry){
        if(sign==axis->sign){
            /* Do not add two arbitrary UINT64 residuals before division. */
            if(rem>=resolution-axis->carry){pixels++;rem-=resolution-axis->carry;}
            else rem+=axis->carry;
        }else if(rem>=axis->carry)rem-=axis->carry;
        else if(pixels){pixels--;rem=resolution-(axis->carry-rem);}
        else {rem=axis->carry-rem;sign=axis->sign;}
    }
    axis->carry=rem;axis->sign=rem?sign:0;
    if(sign<0)return pixels>=2147483648ULL?(-2147483647-1):-(int)pixels;
    return pixels>2147483647ULL?2147483647:(int)pixels;
}
