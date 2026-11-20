#include "tmc_backend.h"
#include "abi.h"
#include <errno.h>
#include <limits.h>

static int valid_host(const char *host)
{
    if (!host) { errno = EINVAL; return 0; }
    size_t i = 0;
    for (; i < 256 && host[i]; ++i) {
        unsigned char c = (unsigned char)host[i];
        if (c <= 32 || c >= 127 || c == '/' || c == '\\' || c == ':' || c == '@') {
            errno = EINVAL; return 0;
        }
    }
    if (!i || i == 256) { errno = EINVAL; return 0; }
    return 1;
}
int tmc_resolve_ipv4(const char *host, uint32_t *address)
{
    if (!address) { errno = EINVAL; return -1; }
    *address = 0;
    if (!valid_host(host)) return -1;
    long r = tmc_syscall(S_NET, 3, (uintptr_t)host, (uintptr_t)address, 0);
    if (r < 0) { *address = 0; errno = EHOSTUNREACH; return -1; }
    return 0;
}
int tmc_tcp_open(const char *host, uint16_t port)
{
    if (!port) { errno = EINVAL; return -1; }
    if (!valid_host(host)) return -1;
    long r = tmc_syscall(S_NET, 4, (uintptr_t)host, port, 0);
    if (r < 0) { errno = ECONNREFUSED; return -1; }
    if (r > INT_MAX) { errno = EOVERFLOW; return -1; }
    return (int)r;
}
long tmc_tcp_write(int h, const void *p, size_t n)
{
    if (h < 0) { errno = EBADF; return -1; }
    if (!n) return 0;
    if (!p) { errno = EFAULT; return -1; }
    if (n > 65536) n = 65536; /* native syscall cap; preserve short write semantics */
    long r = tmc_syscall(S_NET, 5, (uintptr_t)p, n, (unsigned)h);
    if (r < 0) { errno = EIO; return -1; }
    if (!r) { errno = EAGAIN; return -1; }
    if ((unsigned long)r > n) { errno = EIO; return -1; }
    return r;
}
long tmc_tcp_read(int h, void *p, size_t n)
{
    if (h < 0) { errno = EBADF; return -1; }
    if (!n) return 0;
    if (!p) { errno = EFAULT; return -1; }
    if (n > 65536) n = 65536;
    long r = tmc_syscall(S_NET, TMC_NET_RECV_EX, (uintptr_t)p, n, (unsigned)h);
    if (r < 0) {
        switch (r) {
        case TMC_NET_AGAIN: errno = EAGAIN; break;
        case TMC_NET_EBADHANDLE: errno = EBADF; break;
        case TMC_NET_RESET: errno = ECONNRESET; break;
        case TMC_NET_UNAVAILABLE: errno = ENETDOWN; break;
        default: errno = EIO; break;
        }
        return -1;
    }
    if ((unsigned long)r > n) { errno = EIO; return -1; }
    return r; /* zero is clean EOF only, never a TCP reset */
}
int tmc_tcp_close(int h)
{
    if (h < 0) { errno = EBADF; return -1; }
    if (tmc_syscall(S_NET, 7, 0, 0, (unsigned)h) < 0) { errno = EBADF; return -1; }
    return 0;
}

int tmc_tcp_status(int h)
{
    if (h < 0) { errno = EBADF; return -1; }
    long r = tmc_syscall(S_NET, TMC_NET_STATUS, 0, 0, (unsigned)h);
    if (r < 0) {
        errno = r == TMC_NET_EBADHANDLE ? EBADF : ENETDOWN;
        return -1;
    }
    return (int)r;
}
