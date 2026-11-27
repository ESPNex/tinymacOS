#pragma once
#include "partition.h"
/* A preview is mandatory. Apply re-reads metadata under fs_gate, verifies its
 * digest AND the selected extent, then requires an explicit destructive token.
 * Neither the filesystem probe nor a row index authorizes writes. */
#define DISK_EDIT_CONFIRM 0x544d4344u
enum { DISK_EDIT_CREATE=1, DISK_EDIT_DELETE=2, DISK_EDIT_RESIZE=3, DISK_EDIT_EXFAT_PUBLISH=4 };
typedef struct {
    int disk,op,number;
    u64 start,sectors,size;u32 confirm;
    char name[48];
    u8 stamp[32];
    int result;
    char message[160];
} DiskEdit;
/* Negative results are intentionally distinct; an I/O failure after writes is
 * NOT a rollback or success. Stop and use a backup/independent recovery tool. */
enum { DE_OK=0,DE_INVALID=-1,DE_PROTECTED=-2,DE_STALE=-3,DE_UNSUPPORTED=-4,DE_IO=-5,DE_OCCUPIED=-6 };
