/* Target-only libc errno bridge. Host contract tests use their host libc.
 * This is not automatic thread startup: native callers must install a TCB
 * before calling an errno-using adapter. Never fall back to a shared cell. */
#include "tmc_tls.h"
int *__errno_location(void)
{
    int *p=tmc_tls_errno_location();
    /* A missing/corrupt TCB is a runtime startup contract violation, not an
     * adapter I/O error. Fail in this process rather than corrupt another
     * thread's errno or access unmapped FS memory. */
    if(!p)__builtin_trap();
    return p;
}
