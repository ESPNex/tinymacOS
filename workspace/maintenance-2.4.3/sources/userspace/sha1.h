#pragma once
// SHA-1 only for compatibility with the default shasum utility, never for passwords or image integrity.
typedef struct {
    u32 state[5];
    u64 n;
    u8 tail[64];
} SHA1;
static u32 rol(u32 x, int k)
{
    return (x << k) | (x >> (32 - k));
}
static void sha1_block(SHA1 *c, const u8 *p)
{
    u32 w[80];
    for (int i = 0; i < 16;
         i++)w[i] = ((u32)p[i * 4] << 24) | ((u32)p[i * 4 + 1] << 16) | ((u32)p[i * 4 + 2] << 8) | p[i * 4 + 3];
    for (int i = 16; i < 80; i++)w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    u32 a = c->state[0], b = c->state[1], d = c->state[2], e = c->state[3], f = c->state[4];
    for (int i = 0; i < 80; i++) {
        u32 k = i < 20 ? 0x5a827999 : i < 40 ? 0x6ed9eba1 : i < 60 ? 0x8f1bbcdc : 0xca62c1d6;
        u32 v = i < 20 ? (b & d) | (~b & e) : i < 40 ? (b ^ d ^ e) : i < 60 ? (b & d) | (b & e) | (d & e) : (b ^ d ^ e);
        u32 t = rol(a, 5) + v + f + w[i] + k;
        f = e;
        e = d;
        d = rol(b, 30);
        b = a;
        a = t;
    }
    c->state[0] += a;
    c->state[1] += b;
    c->state[2] += d;
    c->state[3] += e;
    c->state[4] += f;
}
static void sha1_add(SHA1 *c, const void *p, size_t n)
{
    const u8 *b = p;
    while (n) {
        int at = c->n % 64;
        size_t z = MIN(n, (size_t)(64 - at));
        memcpy(c->tail + at, b, z);
        c->n += z;
        b += z;
        n -= z;
        if (!(c->n % 64))sha1_block(c, c->tail);
    }
}
static void sha1_end(SHA1 *c, u8 out[20])
{
    u64 bits = c->n * 8;
    u8 pad[72] = {128};
    int z = 56 - (c->n & 63);
    if (z <= 0)z += 64;
    for (int i = 0; i < 8; i++)pad[z + i] = bits >> (56 - 8 * i);
    sha1_add(c, pad, z + 8);
    for (int i = 0; i < 20; i++)out[i] = c->state[i / 4] >> (24 - 8 * (i % 4));
}
