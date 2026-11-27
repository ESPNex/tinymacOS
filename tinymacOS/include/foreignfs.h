/* SPDX-License-Identifier: MIT
 * Read-only, bounded foreign-volume reader. No write callback exists.
 * Offsets/sizes in bytes, partition relative. Caller serializes the mount.
 */
#pragma once
#include "partition.h"
typedef struct {u64 id,size,valid;u32 start,flags;int dir;char name[48];} FvNode;
typedef struct {u64 vcn,lcn,length;int sparse;} FvRun;
typedef struct {
 void *context;u64 bytes;int (*read)(void*,u64,void*,size_t);
 int type;u32 bps,cluster,clusters,record,slots;u64 fat,heap,root,mft,mft_size;
 FvRun runs[64];unsigned nruns;
} ForeignFs;
int fv_mount(ForeignFs*,void*,u64,int(*)(void*,u64,void*,size_t));
int fv_root(ForeignFs*,FvNode*);
/* 1 entry, 0 EOF, -1 corrupt/unsupported/I/O. cookie starts at zero. */
int fv_next(ForeignFs*,const FvNode*,u64*,FvNode*);
int fv_lookup(ForeignFs*,const FvNode*,const char*,FvNode*);
i64 fv_read(ForeignFs*,const FvNode*,u64,void*,size_t);
