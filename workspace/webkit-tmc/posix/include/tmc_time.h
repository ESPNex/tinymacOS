#ifndef TMC_TIME_H
#define TMC_TIME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Opt-in native clock API. NOT a foreign libc time ABI. Only MONOTONIC is
 * supported: boot-relative 100Hz ticks; UTC/realtime stays ENOTSUP, never a
 * fabricated calendar suitable for TLS certificate validation. */
struct tmc_timespec {int64_t tv_sec;long tv_nsec;};
enum {TMC_CLOCK_REALTIME=0,TMC_CLOCK_MONOTONIC=1,TMC_TIMER_ABSTIME=1};
int tmc_clock_gettime(int,struct tmc_timespec*);
int tmc_clock_getres(int,struct tmc_timespec*);
/* Internal/public conversion: validated nonnegative absolute MONOTONIC time,
 * nanoseconds rounded UP. Native deadline 0 means infinite, so epoch zero is
 * represented by tick 1 (already expired on an initialized running system). */
int tmc_time_deadline_ticks(const struct tmc_timespec*,uint64_t*);
int tmc_time_now_ticks(uint64_t*);
/* clock_nanosleep returns an error NUMBER, preserving errno. nanosleep returns
 * -1/errno. Relative waits add a conservative tick for unknown sub-tick phase;
 * kernel sleeps are chunked/rechecked (native syscall caps sleeps to 1 hour).
 * No signals/cancellation, realtime sleeps or interrupt remainder contract. */
int tmc_clock_nanosleep(int,int,const struct tmc_timespec*,struct tmc_timespec*);
int tmc_nanosleep(const struct tmc_timespec*,struct tmc_timespec*);
#ifdef __cplusplus
}
#endif
#endif
