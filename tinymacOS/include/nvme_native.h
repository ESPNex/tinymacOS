/* SPDX-License-Identifier: MIT
 * Native NVM Express NVM command-set transport. No firmware BlockIO at runtime.
 * Caller serializes a controller, including partial 4Kn read/modify/write.
 * DMA memory must be physically contiguous, identity-addressable and coherent;
 * platform adapter owns PCI bus mastering, mapping and interrupt policy.
 */
#pragma once
#include "base.h"
#define NV_DEPTH 16
#define NV_NS_MAX 8
#define NV_BYTES (128u*1024u)
typedef struct PACK {u32 op,nsid;u64 reserved,mptr,prp1,prp2;u32 d[6];} NvCommand;
typedef struct PACK {u32 result,reserved;u16 sqhead,sqid,cid,status;} NvCompletion;
_Static_assert(sizeof(NvCommand)==64&&sizeof(NvCompletion)==16,"NVMe queue ABI");
typedef struct {
 void *context;
 u32 (*read32)(void*,u32);
 void (*write32)(void*,u32,u32);
 u64 (*clock_ms)(void*);
 void (*poll)(void*);
} NvPlatform;
typedef struct {
 NvCommand sq[NV_DEPTH] ALIGNED(4096);
 volatile NvCompletion cq[NV_DEPTH] ALIGNED(4096);
 u16 tail,head,cid,phase;
} NvQueue;
typedef struct {u32 id;u32 block_bytes;u64 blocks,sectors512;} NvNamespace;
typedef struct {
 NvPlatform os;
 NvQueue admin,io;
 u8 bounce[NV_BYTES] ALIGNED(4096);
 u64 prp[512] ALIGNED(4096);
 u8 identify[4096] ALIGNED(4096);
 NvNamespace ns[NV_NS_MAX];
 u32 count,stride,max_bytes,ready_timeout_ms;
 int dead,namespace_limit_reached;
 char model[41],serial[21];
} NvController;
int nvme_controller_start(NvController*,NvPlatform);
/* io supports 512-byte addresses over 512 B/4 KiB namespaces. This translation
 * does NOT make a 512-B GPT bootable on a 4Kn firmware: partition/boot adapters
 * must expose the real namespace block size or reject unsupported geometry. */
int nvme_namespace_io(NvController*,unsigned,u64,u32,void*,int);
int nvme_namespace_flush(NvController*,unsigned);
