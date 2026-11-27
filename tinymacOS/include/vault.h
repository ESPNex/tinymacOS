#pragma once
#include "base.h"
/* Shared FV003 status/credential types. The FV002 backend is removed. */
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
