#pragma once
#include "base.h"
/* Sector-addressed authenticated data volume. GPT/EFI/Recovery OS stay public.
 * The logical payload includes every live/snapshot/backup/account sector.
 * Header/table/journal live AFTER that payload, never inside existing files. */
#ifndef FV_PAYLOAD
#define FV_PAYLOAD (57344u+6288u)
#endif
#define FV_TABLE ((FV_PAYLOAD+15u)/16u)
#define FV_META FV_PAYLOAD
#define FV_OVERHEAD (6u+FV_TABLE)
#define FV_REQUIRED (FV_PAYLOAD+FV_OVERHEAD)
#define FV_ROUNDS 600000u
#define FV_OFF 0u
#define FV_ENCRYPTING 1u
#define FV_ON 2u
#define FV_DECRYPTING 3u
#define FV_IO (-1)
#define FV_AUTH (-2)
#define FV_CORRUPT (-3)
#define FV_SPACE (-4)
#define FV_STATE (-5)
#define FV_ENTROPY (-6)
typedef struct {u8 salt[16],nonce[24],wrapped[32],tag[16];} FvSlot;
typedef struct PACK {
 u8 magic[8];u32 version,state,sectors,progress;u64 serial,ceiling;
 u8 uuid[16];FvSlot slot[2];u8 mac[32],reserved[216],sum[32];
} FvHeader;
_Static_assert(sizeof(FvHeader)==512,"vault header ABI");
typedef struct {u64 nonce;u8 tag[16],pad[8];} FvTag;
_Static_assert(sizeof(FvTag)==32,"vault tag ABI");
typedef struct {
 void *opaque;u32 sectors;
 int (*read)(void*,u32,void*);int (*write)(void*,u32,const void*);int (*flush)(void*);
} FvIo;
typedef struct {
 FvIo io;FvHeader h;u8 key[32];u64 next;int present,unlocked,poison;
} Vault;
/* Return 0 plain, 1 encrypted, or negative corruption/I/O. Never formats. */
int vault_probe(Vault*,FvIo);
int vault_enable(Vault*,const char*,const u8 random[128],char recovery[65]);
int vault_unlock(Vault*,const char*);
void vault_lock(Vault*);
int vault_read(Vault*,u32,void*);
int vault_write(Vault*,u32,const void*);
int vault_step(Vault*,u32);
int vault_decrypt(Vault*,const char*);
int vault_password(Vault*,const char*,const char*,const u8 random[40]);
int vault_recovery(Vault*,const char*,const u8 random[72],char out[65]);
