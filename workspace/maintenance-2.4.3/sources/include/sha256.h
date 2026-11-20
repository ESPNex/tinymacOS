#pragma once
#include "base.h"
typedef struct {
    u32 h[8];
    u64 size;
    u8 tail[64];
} SHA256;
void sha_init(SHA256 *);
void sha_update(SHA256 *, const void *, size_t);
void sha_final(SHA256 *, u8 out[32]);
void sha_bytes(const void *, size_t, u8 out[32]);
void sha_hex(const u8 hash[32], char out[65]);
void erase(void *, size_t);
typedef struct {
    SHA256 in, out;
    u8 u[32], sum[32];
    u32 done;
} PBKDF;
void pbk_begin(PBKDF *, const u8 *, size_t, const u8 salt[16]);
void pbk_run(PBKDF *, unsigned);
