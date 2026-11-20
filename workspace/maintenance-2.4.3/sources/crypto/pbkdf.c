#include "sha256.h"
// One 32-byte PBKDF2-HMAC-SHA256 block. Prepared pad states allow bounded slices.
static void mac(PBKDF *k, const u8 *p, int n, u8 *d)
{
    SHA256 a = k->in, b = k->out;
    u8 buf[32];
    sha_update(&a, p, n);
    sha_final(&a, buf);
    sha_update(&b, buf, 32);
    sha_final(&b, d);
    erase(&a, sizeof a);
    erase(&b, sizeof b);
    erase(buf, sizeof buf);
}
void pbk_begin(PBKDF *k, const u8 *p, size_t n, const u8 salt[16])
{
    memset(k, 0, sizeof * k);
    u8 key[64] = {0}, s[20] = {0};
    if (n > 64)sha_bytes(p, n, key);
    else memcpy(key, p, n);
    for (int j = 0; j < 64; j++)key[j] ^= 0x36;
    sha_init(&k->in);
    sha_update(&k->in, key, 64);
    for (int j = 0; j < 64; j++)key[j] ^= 0x36 ^ 0x5c;
    sha_init(&k->out);
    sha_update(&k->out, key, 64);
    erase(key, sizeof key);
    memcpy(s, salt, 16);
    s[19] = 1;
    mac(k, s, 20, k->u);
    memcpy(k->sum, k->u, 32);
    k->done = 1;
}
void pbk_run(PBKDF *k, unsigned rounds)
{
    while (rounds--) {
        mac(k, k->u, 32, k->u);
        for (int j = 0; j < 32; j++)k->sum[j] ^= k->u[j];
        k->done++;
    }
}
