#include "tmc_backend.h"
#include "lib.h"
long tmc_syscall(long n, uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
    return call(n, a, b, c, d);
}
