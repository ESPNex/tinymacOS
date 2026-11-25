#pragma once
#include "base.h"
/* A single allocated partition owns every native extent. FAT16 occupies only
 * its prefix; data, Recovery, restore source and BIOS payload are not FAT files.
 * Never create a nested partition table or borrow sectors outside this extent. */
#define TC_SECTORS 196608u
#define TC_DATA 49152u
#define TC_DATA_N 73728u
#define TC_REC (TC_DATA+TC_DATA_N)
#define TC_SOURCE (TC_REC+20480u)
#define TC_DUET (TC_SOURCE+49153u)
#define TC_HEADER 9u
#define TC_OLD_MBR 10u
#define TC_MAGIC "TMCVOL23"
typedef struct PACK {
    u8 magic[8];u32 version,sectors,data,rec,source,duet;
    u8 id[16],esp_id[16];u32 crc;u8 reserved[444];
} NativeContainer;
_Static_assert(sizeof(NativeContainer)==512,"native extent descriptor");
static inline int container_bounds(const NativeContainer*h,u32 n){
    return !memcmp(h->magic,TC_MAGIC,8)&&h->version==1&&h->sectors==TC_SECTORS&&h->sectors<=n&&
           h->data==TC_DATA&&h->rec==TC_REC&&h->source==TC_SOURCE&&h->duet==TC_DUET;
}
