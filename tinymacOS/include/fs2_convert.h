/* SPDX-License-Identifier: MIT
 * Offline, same-extent FS2 conversion. Requires enough UNREFERENCED tail for a
 * complete staged copy of all retained block IDs. Never compacts/drops history.
 * After preparation: resumable forward relocation, not power-fail rollback.
 */
#pragma once
#include "vault2.h"
#include "tmcbfs2.h"
#define CV_ENCRYPT 1u
#define CV_DECRYPT 2u
typedef struct PACK {
 u8 magic[8];u32 version,mode;u64 units,stage,count,progress,serial,cleaned;
 Fv2Header credential;u8 pad[3456],mac[32],sum[32];
} CvHeader;
_Static_assert(sizeof(CvHeader)==FV2_UNIT,"conversion journal unit ABI");
typedef struct {Fv2Io io;CvHeader h;u8 key[32];int present,unlocked,poison;} Fs2Convert;
int fs2_convert_probe(Fs2Convert*,Fv2Io);
int fs2_convert_unlock(Fs2Convert*,const char*);
void fs2_convert_lock(Fs2Convert*);
/* Authenticated decrypt uses password OR recovery key. Encrypt creates a new
 * password/recovery pair. A space/auth/validation refusal performs ZERO writes.
 * On success the staged copy+journal are durable; the original prefix is still
 * untouched. Caller must obtain user recovery-key acknowledgement before step. */
int fs2_convert_begin(Fs2Convert*,Fv2Io,unsigned,const char*,const u8[128],char[65]);
/* 0 still pending, 1 fully verified and journal retired, negative: stop/reopen. */
int fs2_convert_step(Fs2Convert*,unsigned);
