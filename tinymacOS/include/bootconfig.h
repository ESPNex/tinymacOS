#pragma once
#include "sha256.h"
#define TMC_BOOT_VERBOSE 1u
#define TMC_BOOT_DISPLAY 2u
#define TMC_BOOT_DISPLAY_MASK 0x07ffff02u
static inline u32 bootconfig_display(int w,int h){return TMC_BOOT_DISPLAY|((u32)(w/8)<<8)|((u32)h<<16);}
static inline int bootconfig_flags_valid(u32 f){if(f&~(TMC_BOOT_VERBOSE|TMC_BOOT_DISPLAY_MASK))return 0;if(!(f&TMC_BOOT_DISPLAY))return !(f&TMC_BOOT_DISPLAY_MASK);unsigned w=((f>>8)&255)*8,h=(f>>16)&2047;return w>=800&&w<=1920&&h>=600&&h<=1080;}
/* Mutable settings live outside the sealed system ESP, in Recovery FAT root. */
typedef struct PACK {u8 magic[8];u32 version,flags;u8 digest[32],reserved[464];} BootConfig;
_Static_assert(sizeof(BootConfig)==512,"boot config sector");
static inline int bootconfig_valid(const BootConfig *c)
{
    u8 hash[32];sha_bytes(c,16,hash);
    return !memcmp(c->magic,"TMCBOOT1",8)&&c->version==1&&bootconfig_flags_valid(c->flags)&&!memcmp(hash,c->digest,32);
}
static inline void bootconfig_make(BootConfig *c,u32 flags)
{
    memset(c,0,sizeof *c);memcpy(c->magic,"TMCBOOT1",8);c->version=1;c->flags=flags;sha_bytes(c,16,c->digest);
}
