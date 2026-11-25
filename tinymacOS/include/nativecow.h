/* SPDX-License-Identifier: MIT
 * Sector-granular snapshot CoW for the proprietary native FS, NOT APFS ABI.
 * All I/O passes through the caller's bounded/authenticated volume device.
 * Writes must be durable before returning success (as volume_cipher_io is).
 */
#pragma once
#include "nativefs.h"
#define NC_SNAPSHOTS 8u
#define NC_SUPER 137u
#define NC_JOURNAL 138u
#define NC_STAGED 139u
#define NC_IO (-1)
#define NC_CORRUPT (-2)
#define NC_SPACE (-3)
typedef struct PACK {u32 location,crc;} CowRef;
typedef struct PACK {char name[28];u32 date,id,base,crc,gen;} CowEntry;
typedef struct PACK {
 u8 magic[8];u32 version,frame,map_start,map_stride,store_start,store_count,limit,crc;u8 reserved[472];
} CowSuper;
typedef struct PACK {
 u8 magic[8];u32 version,logical,location,mask,ids[8],new_crc,crc;u8 reserved[448];
} CowJournal;
_Static_assert(sizeof(CowSuper)==512&&sizeof(CowJournal)==512,"native CoW disk ABI");
typedef struct {
 void*opaque;u32 sectors;
 int(*read)(void*,u32,void*);int(*write)(void*,u32,const void*);
} CowIo;
typedef struct {
 CowIo io;CowSuper super;
 CowRef refs[8][NF_MAX_IMAGE];
 u16 users[NPART+1];
 /* Reused only by explicit legacy conversion/offline relocation. */
 u32 source[NPART];u8 moved[(NPART+7)/8];
 u8 catalog[512];u32 active,cursor,allocated;
 int enabled,poison,error;
} NativeCow;
/* mount returns 1 CoW, 0 legacy full-image layout, negative on error.
 * It replays a pending CoW transaction before exposing any snapshot. */
int native_cow_mount(NativeCow*,CowIo,u32 frame,u32 limit);
int native_cow_enable(NativeCow*,CowIo,u32 frame,u32 limit);
int native_cow_read(NativeCow*,u32,void*);
int native_cow_write(NativeCow*,u32,const void*);
int native_cow_prepare(NativeCow*,int snapshot,u32*manifest);
int native_cow_verify(NativeCow*,int snapshot);
u32 native_cow_used(NativeCow*);

/* Offline resize separates validation, relocation and final publication.
 * The caller must keep an invalid live-volume header until finish succeeds. */
int native_cow_mount_readonly(NativeCow*,CowIo,u32,u32);
int native_cow_can_resize(NativeCow*,u32,u32);
int native_cow_relayout(NativeCow*,u32,u32);
int native_cow_finish(NativeCow*);
