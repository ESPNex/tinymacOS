#pragma once
#include "sha256.h"
#define TMC_BOOT_VERBOSE 1u
/* Mutable settings live outside the sealed system ESP, in Recovery FAT root. */
typedef struct PACK {u8 magic[8];u32 version,flags;u8 digest[32],reserved[464];} BootConfig;
_Static_assert(sizeof(BootConfig)==512,"boot config sector");
static inline int bootconfig_valid(const BootConfig *c)
{
    u8 hash[32];sha_bytes(c,16,hash);
    return !memcmp(c->magic,"TMCBOOT1",8)&&c->version==1&&!(c->flags&~TMC_BOOT_VERBOSE)&&!memcmp(hash,c->digest,32);
}
static inline void bootconfig_make(BootConfig *c,u32 flags)
{
    memset(c,0,sizeof *c);memcpy(c->magic,"TMCBOOT1",8);c->version=1;c->flags=flags;sha_bytes(c,16,c->digest);
}
