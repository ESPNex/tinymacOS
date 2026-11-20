#ifndef TMC_POSIX_ERRNO_H
#define TMC_POSIX_ERRNO_H
#include <bits/errno.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Target ABI: every native thread must attach a caller-owned tmc_tls_block
 * before its first errno-using libc call. No global or shared fallback. */
int *__errno_location(void);
#ifdef __cplusplus
}
#endif
#define errno (*__errno_location())
#endif
