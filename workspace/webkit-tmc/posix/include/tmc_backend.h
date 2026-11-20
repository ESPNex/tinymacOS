/* TinymacOS backend boundary. This is NOT yet a POSIX libc. */
#ifndef TMC_BACKEND_H
#define TMC_BACKEND_H
#include <stddef.h>
#include <stdint.h>
#include "netabi.h"
#ifdef __cplusplus
extern "C" {
#endif
/* May be replaced by the contract-test harness. Native implementation uses syscall. */
long tmc_syscall(long, uint64_t, uint64_t, uint64_t, uint64_t);
/* IPv4 is returned in network byte order. No IPv6 or DNSSEC claim. */
int tmc_resolve_ipv4(const char *host, uint32_t *address);
/* Native handles, NOT POSIX file descriptors; callers must serialize access. */
int tmc_tcp_open(const char *host, uint16_t port);
/* Return bytes transferred; -1 on error, errno set. Partial writes are legal. */
long tmc_tcp_write(int handle, const void *data, size_t length);
/* Requires the new RECV_EX kernel operation. Returns zero only for clean EOF;
 * distinguishes would-block, invalid handles, resets and other failures. */
long tmc_tcp_read(int handle, void *data, size_t capacity);
int tmc_tcp_close(int handle);
int tmc_tcp_status(int handle);
/* Socket-handle polling foundation, NOT POSIX descriptor polling yet. */
#define TMC_POLL_INVALID 16
struct tmc_pollfd { int handle; unsigned events, revents; };
int tmc_socket_poll(struct tmc_pollfd *items, size_t count, int timeout_ms);
/* 100 Hz tick clock. Units are nanoseconds, resolution is 10 ms. */
uint64_t tmc_monotonic_ns(void);
/* Fail closed until a kernel CSPRNG byte API exists. Does NOT use rand/ticks. */
int tmc_secure_random(void *buffer, size_t length);
#ifdef __cplusplus
}
#endif
#endif
