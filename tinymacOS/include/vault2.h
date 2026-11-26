/* SPDX-License-Identifier: MIT
 * Fresh-volume scalable authenticated block envelope. NOT the v1 vault format.
 * 4KiB physical I/O; each independently authenticated 16KiB logical block owns
 * four ciphertext units and one 4KiB metadata unit (25% physical overhead).
 * No shared tag sectors, capacity-sized tag table, initialization scan or u32
 * block addresses. Plaintext conversion and OS integration are separate gates.
 */
#pragma once
#include "vault.h"
#define FV2_UNIT 4096u
#define FV2_BLOCK 16384u
#define FV2_RECORD (5u*FV2_UNIT)
#define FV2_START 9u
#define FV2_MAX_BLOCKS ((1ull<<40)/FV2_BLOCK)
#define FV2_ROUNDS 600000u
typedef struct {
 void *context;u64 units;
 int (*read)(void*,u64,void*);int (*write)(void*,u64,const void*);int (*flush)(void*);
 void*(*alloc)(void*,size_t);void(*free)(void*,void*);
} Fv2Io;
typedef struct PACK {
 u8 magic[8];u32 version,state;u64 units,blocks,serial,ceiling;
 u8 uuid[16];FvSlot slot[2];u8 mac[32],reserved[208],sum[32];
} Fv2Header;
_Static_assert(sizeof(Fv2Header)==512,"vault2 header ABI");
typedef struct {Fv2Io io;Fv2Header h;u8 key[32];u64 next;int present,unlocked,poison;} Vault2;
u64 vault2_capacity(u64);
/* Probe is read-only. 0 only for BOTH blank headers, 1 for a valid envelope.
 * Wrong/corrupt/legacy/plaintext formats are never silently initialized. */
int vault2_probe(Vault2*,Fv2Io);
/* Explicit fresh blank-volume creation ONLY; never an in-place enable. */
int vault2_create(Vault2*,const char*,const u8 random[128],char recovery[65]);
int vault2_unlock(Vault2*,const char*);
void vault2_lock(Vault2*);
int vault2_read(Vault2*,u64,void*);
int vault2_write(Vault2*,u64,const void*);
int vault2_password(Vault2*,const char*,const char*,const u8 random[40]);
int vault2_recovery(Vault2*,const char*,const u8 random[72],char out[65]);
