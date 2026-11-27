/* SPDX-License-Identifier: MIT
 * Kernel-only partition view, shared by external writable filesystem drivers.
 * Coordinates are always 512-byte sectors; no partition-table I/O is exposed.
 * The caller holds fs_gate, validates the partition map and owns permissions.
 */
#pragma once
#include "base.h"
typedef struct {
 void*context;u64 sectors;u32 logical_bytes;
 int(*read)(void*,u64,u32,void*);
 int(*write)(void*,u64,u32,const void*); /* NULL = strictly read-only */
 int(*flush)(void*);
 void*(*alloc)(void*,size_t);void(*free)(void*,void*);
 u32(*timestamp)(void*); /* packed FAT timestamp from the real system clock */
} ForeignIo;
/* Full UTF8 name; the namespace adapter must NOT silently truncate to Dirent. */
typedef struct {u64 size;int dir,ro;char name[1021];} ForeignEntry;
enum { FOREIGN_OK=0,FOREIGN_IO=-1,FOREIGN_INVALID=-2,FOREIGN_READ_ONLY=-3,FOREIGN_EXISTS=-4,FOREIGN_MISSING=-5,FOREIGN_NOT_EMPTY=-6,FOREIGN_FULL=-7,FOREIGN_TOO_LARGE=-8 };
int fat_volume_stat(ForeignIo*,const char*,ForeignEntry*);
int fat_volume_list(ForeignIo*,const char*,ForeignEntry*,int,int);
int fat_volume_create(ForeignIo*,const char*,int);
int fat_volume_remove(ForeignIo*,const char*);
int fat_volume_move(ForeignIo*,const char*,const char*);
i64 fat_volume_read(ForeignIo*,const char*,u64,void*,size_t);
/* Positional write followed by truncate at offset+bytes, matching FS2 ABI.
 * In particular, a shorter overwrite never leaves an old tail visible. */
i64 fat_volume_write(ForeignIo*,const char*,u64,const void*,size_t);
/* Formats ONLY the supplied partition view as exFAT, no MBR/GPT or VHD.
 * Privileged caller must separately enforce consent, truly unallocated space,
 * source/extent disjointness and delayed partition-table publication. */
int fat_volume_format_exfat(ForeignIo*);

int nt_volume_stat(ForeignIo*,const char*,ForeignEntry*);
int nt_volume_list(ForeignIo*,const char*,ForeignEntry*,int,int);
int nt_volume_create(ForeignIo*,const char*,int);
int nt_volume_remove(ForeignIo*,const char*);
int nt_volume_move(ForeignIo*,const char*,const char*);
i64 nt_volume_read(ForeignIo*,const char*,u64,void*,size_t);
i64 nt_volume_write(ForeignIo*,const char*,u64,const void*,size_t);
