/* tinymacOS x86-64 platform glue for the unmodified CPU Jitter RNG. */
#pragma once
#include "base.h"
#include <limits.h>
typedef ptrdiff_t ssize_t;
#define EAGAIN 11
#define EINVAL 22
#define EOPNOTSUPP 95
void *kmalloc(size_t);
void kfree(void *);
void erase(void *, size_t);
static inline void jent_get_nstime(uint64_t *out) {
    uint32_t l, h;
    __asm__ volatile("rdtsc" : "=a"(l), "=d"(h) :: "memory");
    *out = ((uint64_t)h << 32) | l;
}
static inline void *jent_zalloc(size_t n) {
    void *p = kmalloc(n);
    if (p) memset(p, 0, n);
    return p;
}
static inline void jent_zfree(void *p, size_t n) { if (p) { erase(p,n); kfree(p); } }
static inline void jent_memset_secure(void *p, size_t n) { erase(p,n); }
static inline int jent_fips_enabled(void) { return 1; } /* enable library health checks, not a certification */
static inline uint32_t jent_cache_size_roundup(void) { return 0; } /* upstream bounded default */
static inline uint64_t rol64(uint64_t x, int n) { return (x << (n & 63)) | (x >> ((64-n) & 63)); }
