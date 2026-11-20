#include "sha256.h"
// FIPS 180-4, 32-bit big endian words. The host test compares against hashlib.
static const u32 K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};
static inline u32 ro(u32 x, int k)
{
    return x >> k | x << (32 - k);
}
static inline u32 be(const u8 *p)
{
    return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3];
}
static void block(u32 *h, const u8 *p)
{
    u32 t[64];
    for (int j = 0; j < 16; j++)t[j] = be(p + 4 * j);
    for (int j = 16; j < 64; j++) {
        u32 a = t[j - 15], b = t[j - 2];
        t[j] = t[j - 16] + (ro(a, 7)^ro(a, 18) ^ (a >> 3)) + t[j - 7] + (ro(b, 17)^ro(b, 19) ^ (b >> 10));
    }
    u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], z = h[7];
    for (int i = 0; i < 64; i++) {
        u32 A = z + (ro(e, 6)^ro(e, 11)^ro(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + t[i], B = (ro(a, 2)^ro(a, 13)^ro(a,
                22)) + ((a & b) ^ (a & c) ^ (b & c));
        z = g;
        g = f;
        f = e;
        e = d + A;
        d = c;
        c = b;
        b = a;
        a = A + B;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += z;
}
void erase(void *p, size_t n)
{
    volatile u8 *q = p;
    while (n--)*q++ = 0;
}
void sha_init(SHA256 *s)
{
    static const u32 iv[] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memset(s, 0, sizeof * s);
    memcpy(s->h, iv, 32);
}
void sha_update(SHA256 *s, const void *v, size_t n)
{
    const u8 *p = v;
    int k = s->size % 64;
    s->size += n;
    if (k) {
        size_t m = MIN(n, (size_t)(64 - k));
        memcpy(s->tail + k, p, m);
        k += m;
        p += m;
        n -= m;
        if (k == 64)block(s->h, s->tail);
    }
    while (n >= 64) {
        block(s->h, p);
        p += 64;
        n -= 64;
    }
    if (n)memcpy(s->tail, p, n);
}
void sha_final(SHA256 *s, u8 out[32])
{
    int k = s->size % 64;
    s->tail[k++] = 128;
    if (k > 56) {
        memset(s->tail + k, 0, 64 - k);
        block(s->h, s->tail);
        k = 0;
    }
    memset(s->tail + k, 0, 56 - k);
    u64 bits = s->size * 8;
    for (int j = 0; j < 8; j++)s->tail[63 - j] = bits >> (8 * j);
    block(s->h, s->tail);
    for (int j = 0; j < 32; j++)out[j] = s->h[j / 4] >> (24 - 8 * (j % 4));
}
void sha_bytes(const void *p, size_t n, u8 out[32])
{
    SHA256 s;
    sha_init(&s);
    sha_update(&s, p, n);
    sha_final(&s, out);
    erase(&s, sizeof s);
}
void sha_hex(const u8 h[32], char out[65])
{
    const char *d = "0123456789abcdef";
    for (int j = 0; j < 32; j++) {
        out[2 * j] = d[h[j] >> 4];
        out[2 * j + 1] = d[h[j] & 15];
    }
    out[64] = 0;
}
