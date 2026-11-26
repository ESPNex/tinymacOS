/* SPDX-License-Identifier: MIT
 * TMCBFS2 core; mounted/installed only in opt-in development builds.
 * 16KiB immutable blocks, u64 sizes/IDs, dynamic trees and snapshot catalogue.
 * A caller owns/serializes a mount; never use against physical/user media in QA.
 */
#pragma once
#include "base.h"
#define T2_BLOCK 16384u
#define T2_MAX_BYTES (1ull<<40)
#define T2_NAME 48u
#define T2_DIR 1u
#define T2_FILE 2u
typedef struct {
 void *context;
 u64 blocks;
 int (*read)(void*,u64,void*);
 int (*write)(void*,u64,const void*);
 int (*flush)(void*);
 void *(*alloc)(void*,size_t);
 void (*free)(void*,void*);
} T2Io;
typedef struct {u64 key,parent,size,root,kind;char name[T2_NAME];u64 reserved;} T2Entry;
_Static_assert(sizeof(T2Entry)==96,"TMCBFS2 tree entry ABI");
typedef struct {
 u8 magic[8];u32 version,block_bytes;
 u64 blocks,generation,live,snapshots,next_inode,next_snapshot;
 u32 crc,reserved;u64 control;u8 pad[48];
} T2Super;
_Static_assert(sizeof(T2Super)==128,"TMCBFS2 superblock ABI");
typedef struct {
 T2Io io;T2Super super,slots[2];
 u8 **used,**tree;u64 cursor,allocated;size_t bitmap_pages;
 int valid[2],active,faulted,transaction;
 u64 fresh[64];unsigned nfresh;
} Tmc2;
int tmc2_format(Tmc2*,T2Io);
/* Explicit logical reset of a KNOWN mounted volume, not secure erasure. Seed
 * parents are 1-based earlier entries (0 = root). Publishes the fresh empty
 * files/directories, blank account and empty snapshot catalogue atomically. */
typedef struct {unsigned parent,kind;const char*name;} T2Seed;
int tmc2_reset(Tmc2*,const T2Seed*,unsigned);
/* Private account slots are unreachable through the ordinary file namespace. */
int tmc2_account(Tmc2*,unsigned,void*,int);
int tmc2_mount(Tmc2*,T2Io);
void tmc2_close(Tmc2*);
int tmc2_stat(Tmc2*,u64,T2Entry*);
int tmc2_lookup(Tmc2*,u64,const char*,T2Entry*);
/* Returns 1 entry, 0 at EOF, -1 on corruption/error; caller keeps a u64 cookie.
 * No array sized to the number of files, directories or snapshots. */
int tmc2_readdir(Tmc2*,u64,u64*,T2Entry*);
int tmc2_snapshots(Tmc2*,u64*,T2Entry*);
int tmc2_create(Tmc2*,u64,const char*,unsigned,u64*);
i64 tmc2_read(Tmc2*,u64,u64,void*,size_t);
i64 tmc2_write(Tmc2*,u64,u64,const void*,size_t);
int tmc2_remove(Tmc2*,u64);
int tmc2_readonly(Tmc2*,u64,int);
int tmc2_move(Tmc2*,u64,u64,const char*);
int tmc2_truncate(Tmc2*,u64,u64);
int tmc2_snapshot(Tmc2*,const char*,u64*);
int tmc2_snapshot_delete(Tmc2*,const char*);
/* Atomic named checkpoint replacement; never delete the old backup first. */
int tmc2_checkpoint(Tmc2*,const char*);
int tmc2_snapshot_find(Tmc2*,const char*,T2Entry*);
int tmc2_view_lookup(Tmc2*,u64,u64,const char*,T2Entry*);
int tmc2_view_stat(Tmc2*,u64,u64,T2Entry*);
int tmc2_view_readdir(Tmc2*,u64,u64,u64*,T2Entry*);
i64 tmc2_view_read(Tmc2*,u64,u64,u64,void*,size_t);
int tmc2_snapshot_restore(Tmc2*,const char*);
int tmc2_snapshot_verify(Tmc2*,const char*);
int tmc2_verify(Tmc2*);
int tmc2_snapshot_stat(Tmc2*,const char*,u64,T2Entry*);
i64 tmc2_snapshot_read(Tmc2*,const char*,u64,u64,void*,size_t);
/* Checkpoint retires the previous superblock's references, then marks BOTH
 * crash-fallback roots. Reclaims only unreachable blocks, never shared data. */
int tmc2_collect(Tmc2*);
u64 tmc2_used_blocks(const Tmc2*);
i64 tmc2_inode_count(Tmc2*);
/* Atomic external checkpoint import between distinct serialized media. Source
 * is read-only, destination live files + private accounts replaced together;
 * destination snapshots retained, source snapshots not copied en masse.
 * Refuses insufficient staging space. On I/O error discard destination mount. */
int tmc2_import_snapshot(Tmc2*,Tmc2*,const char*);
/* Read-only full verification and exact staging-space preflight. */
int tmc2_import_check(Tmc2*,Tmc2*,const char*,u64*);

/* Read-only enumeration of blocks retained by BOTH fallback roots and snapshots. */
int tmc2_allocated_next(const Tmc2*,u64*,u64*);
