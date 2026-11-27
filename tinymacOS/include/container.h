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
#define TMC_CONTAINER_MAX (1ull<<35)
#define TC_MAGIC "TMCVOL23"
typedef struct PACK {
    u8 magic[8];u32 version,sectors,data,rec,source,duet;
    u8 id[16],esp_id[16];u32 crc;
    u64 span64,rec64,source64,duet64;u8 reserved[412];
} NativeContainer;
_Static_assert(sizeof(NativeContainer)==512,"native extent descriptor");
_Static_assert(__builtin_offsetof(NativeContainer,crc)==64 && __builtin_offsetof(NativeContainer,span64)==68,"v1/v2 prefix and CRC preserved");
/* v1 keeps the historical fixed 96MiB container. v2 owns the selected gap;
 * FAT boot prefix and tail images stay fixed-sized, FS2 gets all the middle.
 * Subtractions and minimum-size checks prevent descriptor overflow. */
static inline u64 container_span(u64 available){return available&~2047ull;}
static inline u64 container_rec(u64 sectors){return sectors-(TC_SECTORS-TC_REC);}
static inline int container_bounds(const NativeContainer*h,u64 n){
    if(memcmp(h->magic,TC_MAGIC,8)||h->data!=TC_DATA)return 0;
    if(h->version==3){u64 span=h->span64;return span>=TC_SECTORS&&span<=n&&span<=TMC_CONTAINER_MAX&&span%2048==0&&h->rec64==container_rec(span)&&h->source64==h->rec64+20480&&h->duet64==h->source64+49153&&h->sectors==0&&h->rec==0&&h->source==0&&h->duet==0;}
    if(h->sectors>n||h->sectors<TC_SECTORS)return 0;
    if(h->version==1)return h->sectors==TC_SECTORS&&h->rec==TC_REC&&h->source==TC_SOURCE&&h->duet==TC_DUET;
    if(h->version!=2||h->sectors%2048)return 0;
    u64 rec=container_rec(h->sectors);
    return h->rec==rec&&h->source==rec+20480u&&h->duet==rec+20480u+49153u;
}

static inline u64 container_rec_at(const NativeContainer*h){return h->version==3?h->rec64:h->rec;}
static inline u64 container_source_at(const NativeContainer*h){return h->version==3?h->source64:h->source;}
