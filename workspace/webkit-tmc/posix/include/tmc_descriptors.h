/* Unified backend descriptor namespace. Not the public POSIX ABI.
 * Each process owns one zero-initialized table; caller serializes ALL operations,
 * including poll. No fork/exec, signals, cancellation, stdio or thread safety yet.
 * Kernel TCP handles remain global: this table is NOT a security boundary.
 * File support is READ-ONLY: native writes truncate at the write endpoint, unlike
 * POSIX write. Do not expose them as POSIX until the kernel contract is fixed. */
#ifndef TMC_DESCRIPTORS_H
#define TMC_DESCRIPTORS_H
#include "tmc_backend.h"
#define TMC_FD_MAX 64
#define TMC_FD_FIRST 3
#define TMC_FD_FILE 1
#define TMC_FD_TCP 2
struct tmc_open_description { int kind, native_handle; unsigned references; };
struct tmc_fd_table {
    unsigned slots[TMC_FD_MAX]; /* zero = absent; otherwise description index + 1 */
    struct tmc_open_description objects[TMC_FD_MAX];
};
struct tmc_fd_poll { int fd; unsigned events, revents; };
#ifdef __cplusplus
extern "C" {
#endif
/* Init ONLY a fresh/non-owning table. Calling init on a live table leaks handles. */
void tmc_fd_init(struct tmc_fd_table *);
/* Ownership transfers only on success. The SAME kind/native pair may not be
 * adopted twice. Use dup to share an open description and native file offset. */
int tmc_fd_adopt(struct tmc_fd_table *, int kind, int native_handle);
int tmc_fd_open_readonly(struct tmc_fd_table *, const char *absolute_path);
int tmc_fd_connect(struct tmc_fd_table *, const char *host, uint16_t port);
int tmc_fd_dup(struct tmc_fd_table *, int fd, int minimum);
/* close always consumes the valid descriptor, even if native close fails.
 * Never retry close on that descriptor. Last reference closes the native handle. */
int tmc_fd_close(struct tmc_fd_table *, int fd);
long tmc_fd_read(struct tmc_fd_table *, int fd, void *, size_t);
long tmc_fd_write(struct tmc_fd_table *, int fd, const void *, size_t);
/* Native absolute seek, not POSIX lseek: zero = success, no SEEK_CUR/END yet. */
int tmc_fd_seek_absolute(struct tmc_fd_table *, int fd, uint64_t offset);
int tmc_fd_poll(struct tmc_fd_table *, struct tmc_fd_poll *, size_t count, int timeout_ms);
#ifdef __cplusplus
}
#endif
#endif
