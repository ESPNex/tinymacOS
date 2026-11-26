#ifndef TMC_SOURCE_TIME_H
#define TMC_SOURCE_TIME_H
/* Explicit target-only source API; never interpose over hosted libc headers. */
#include "tmc_time.h"
typedef int clockid_t;
typedef int64_t time_t;
#define timespec tmc_timespec
#define CLOCK_REALTIME TMC_CLOCK_REALTIME
#define CLOCK_MONOTONIC TMC_CLOCK_MONOTONIC
#define TIMER_ABSTIME TMC_TIMER_ABSTIME
#define clock_gettime tmc_clock_gettime
#define clock_getres tmc_clock_getres
#define clock_nanosleep tmc_clock_nanosleep
#define nanosleep tmc_nanosleep
#endif
