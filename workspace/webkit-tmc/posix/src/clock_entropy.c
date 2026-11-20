#include "tmc_backend.h"
#include "abi.h"
#include <errno.h>
uint64_t tmc_monotonic_ns(void)
{
    return (uint64_t)tmc_syscall(S_TICKS, 0, 0, 0, 0) * UINT64_C(10000000);
}
int tmc_secure_random(void *p, size_t n)
{
    if (!n) return 0;
    (void)p;
    errno = ENOSYS;
    return -1;
}
