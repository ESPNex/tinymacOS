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
/* v1 keeps the historical fixed 96MiB container. v2 owns the selected gap;
 * FAT boot prefix and tail images stay fixed-sized, FS2 gets all the middle.
 * Subtractions and minimum-size checks prevent descriptor overflow. */
static inline u32 container_span(u32 available){return available&~2047u;}
static inline u32 container_rec(u32 sectors){return sectors-(TC_SECTORS-TC_REC);}
static inline int container_bounds(const NativeContainer*h,u32 n){
    if(memcmp(h->magic,TC_MAGIC,8)||h->sectors>n||h->sectors<TC_SECTORS||h->data!=TC_DATA)return 0;
    if(h->version==1)return h->sectors==TC_SECTORS&&h->rec==TC_REC&&h->source==TC_SOURCE&&h->duet==TC_DUET;
    if(h->version!=2||h->sectors%2048)return 0;
    u32 rec=container_rec(h->sectors);
    return h->rec==rec&&h->source==rec+20480u&&h->duet==rec+20480u+49153u;
}
