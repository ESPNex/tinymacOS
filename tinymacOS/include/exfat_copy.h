/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copy to a NEW zero-filled raw exFAT partition view only.
 * Caller validates unallocated extent and publishes the partition last.
 */
#pragma once
#include "base.h"
typedef struct {u64 id,size;int dir;char name[48];} ExEntry;
typedef struct {
 void*context;
 int (*next)(void*,u64,u64*,ExEntry*);
 i64 (*read)(void*,u64,u64,void*,size_t);
 int (*write)(void*,u64,const void*,size_t);
 void*(*alloc)(void*,size_t);void(*free)(void*,void*);
 u32 serial;
 int (*prepare)(void*,u64,u64); /* logical length, conservative stored data bytes */
 u64 (*partition_lba)(void*); /* actual new partition origin, after prepare */
} ExCopyIo;
/* Source root ID; no writes occur before full tree/name/layout preflight.
 * Returns 0 on success, -1 input/read/allocation, -2 unsupported Windows name
 * or collision, -3 output I/O. required = raw partition bytes (no container or footer). */
int exfat_copy(ExCopyIo,u64,u64*);
