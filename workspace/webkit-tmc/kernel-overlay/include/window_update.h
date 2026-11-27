#pragma once
#include "abi.h"
/* A repeated command publication is not new visual content. Never deduplicate
 * pointer-based raster payloads: identical commands may refer to changed pixels.
 * Call only after validating both command arrays and the optional title. */
static inline int window_commands_equal(const Draw *old, unsigned on,
                                        const Draw *next, unsigned nn)
{
    if(on!=nn)return 0;
    for(unsigned i=0;i<nn;i++){
        if(old[i].type==C_RASTER||next[i].type==C_RASTER)return 0;
        const unsigned char *a=(const void*)(old+i),*b=(const void*)(next+i);
        for(unsigned j=0;j<sizeof(Draw);j++)if(a[j]!=b[j])return 0;
    }
    return 1;
}
