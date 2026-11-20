#include "k.h"
#include "sha256.h"
#include "../crypto/jitterentropy/jitterentropy.h"

// This module supplies one PUBLIC, 16-byte password salt for a fresh account.
// It is not a system RNG API, key generator, reset token or authentication method.
static int usable(const u8 bytes[16])
{
    // Reject dead firmware/hardware sources returning repeating halves (including
    // all-00 and all-FF). This sanity check alone is not an entropy estimate.
    return memcmp(bytes, bytes + 8, 8) != 0;
}
static int cpu_salt(u8 out[16], int seed)
{
    for (int i = 0; i < 2; i++) {
        u64 v;
        int ok = 0;
        for (int j = 0; j < 64 && !ok; j++) {
            if (seed) asm volatile("rdseed %0" : "=r"(v), "=@ccc"(ok));
            else asm volatile("rdrand %0" : "=r"(v), "=@ccc"(ok));
            if (!ok) asm volatile("pause");
        }
        if (!ok) return 0;
        memcpy(out + i * 8, &v, 8);
    }
    return usable(out);
}
int enrollment_salt(u8 out[16])
{
    u32 a[4];
    cpuid(0, a);
    u32 max = a[0];
    if (boot.rng_ok && usable(boot.entropy)) {
        memcpy(out, boot.entropy, 16);
        logstr("[salt] EFI RNG\n");
        return 1;
    }
    if (max >= 7) {
        cpuid(7, a);
        if ((a[1] & (1u << 18)) && cpu_salt(out, 1)) {
            logstr("[salt] RDSEED\n");
            return 1;
        }
    }
    cpuid(1, a);
    if ((a[2] & (1u << 30)) && cpu_salt(out, 0)) {
        logstr("[salt] RDRAND\n");
        return 1;
    }
    if (a[3] & (1u << 4)) { // Never execute RDTSC if the virtual CPU does not advertise it.
        const unsigned flags = JENT_FORCE_FIPS | JENT_DISABLE_INTERNAL_TIMER;
        if (!jent_entropy_init_ex(3, flags)) {
            struct rand_data *ec = jent_entropy_collector_alloc(3, flags);
            if (ec) {
                int ok = jent_read_entropy(ec, (char *)out, 16) == 16 && usable(out);
                jent_entropy_collector_free(ec);
                if (ok) {
                    logstr("[salt] jitter health checks passed\n");
                    return 1;
                }
            }
        }
    }
    erase(out, 16);
    logstr("[salt] host setup code required\n");
    return 0;
}
static int nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
int decode_salt(const char text[33], u8 out[16])
{
    if (text[32])return 0;
    u8 tmp[16];
    int ok = 1;
    for (int i = 0; i < 16; i++) {
        int a = nibble(text[i * 2]), b = nibble(text[i * 2 + 1]);
        if (a < 0 || b < 0) {
            ok = 0;
            break;
        }
        tmp[i] = (u8)((a << 4) | b);
    }
    ok = ok && usable(tmp);
    if (ok)memcpy(out, tmp, 16);
    erase(tmp, 16);
    return ok;
}
