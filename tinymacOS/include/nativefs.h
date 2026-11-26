/* SPDX-License-Identifier: MIT
 * TMCBFS1 variable-capacity geometry. Proprietary, NOT Linux Btrfs.
 * The inode/file ABI remains stable; the number of usable slots and the
 * snapshot/backup extents are derived from the selected volume capacity.
 */
#pragma once
#include "disklayout.h"
#define NF_MAX 384u
#define NF_MIN 64u
#define NF_DATA 144u
#define NF_NODE 64u
#define NF_SUPER 128u
#define NF_CAT 136u
#define NF_MAX_IMAGE (NF_DATA+NF_MAX*16u)
#define NF_GEOM_AT 256u
#define NF_MIN_VOLUME 16384u
#define NF_MAX_VOLUME 65536u
/* Fixed maximum format capacity is explicit: 384 files/dirs, 8 KiB/file.
 * Resizing 8..32 MiB changes real allocation capacity, not only table bounds.
 * Existing larger partitions remain readable; their usable format limit is
 * unchanged until explicitly resized within this supported capacity range. */
typedef struct PACK {
 u8 magic[8];u32 version,slots,image,backup,payload,sectors,crc;
} NativeGeometry;
int native_geometry(const void *header,NativeGeometry*);
int native_geometry_fit(u32 sectors,NativeGeometry*);
void native_geometry_store(void *header,const NativeGeometry*);
u32 native_crc32c(const void*,u32);
int native_image_check(const void*,const NativeGeometry*);
int native_resize(Medium*,DiskEdit*,int);

/* Shared full legacy image scratch. Kernel callers hold fs_gate; installer
 * state rejects concurrent disk edits/maintenance until activation finishes. */
extern u8 native_image_workspace[NF_MAX_IMAGE*512];
