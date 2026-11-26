#include "tmc_time.h"
#include "tmc_backend.h"
#include "abi.h"
#include <errno.h>
#define HZ UINT64_C(100)
#define NS_PER_TICK 10000000L
static int conversion(const struct tmc_timespec *t,uint64_t *ticks)
{
 if(!t||!ticks)return EFAULT;
 if(t->tv_sec<0||t->tv_nsec<0||t->tv_nsec>=1000000000L)return EINVAL;
 uint64_t sec=(uint64_t)t->tv_sec,extra=((uint64_t)t->tv_nsec+NS_PER_TICK-1)/NS_PER_TICK;
 if(sec>(UINT64_MAX-extra)/HZ)return EOVERFLOW;
 *ticks=sec*HZ+extra;return 0;
}
int tmc_time_deadline_ticks(const struct tmc_timespec*t,uint64_t*out)
{
 uint64_t ticks;int r=conversion(t,&ticks);if(r)return r;
 if(!out)return EFAULT;
 *out=ticks?ticks:1;return 0;
}
int tmc_time_now_ticks(uint64_t*out)
{
 if(!out)return EFAULT;
 long ticks=tmc_syscall(S_TICKS,0,0,0,0);if(ticks<0)return EIO;
 *out=(uint64_t)ticks;return 0;
}
int tmc_clock_gettime(int clock,struct tmc_timespec*out)
{
 if(clock!=TMC_CLOCK_MONOTONIC){errno=clock==TMC_CLOCK_REALTIME?ENOTSUP:EINVAL;return -1;}
 if(!out){errno=EFAULT;return -1;}
 int saved=errno;uint64_t ticks;int r=tmc_time_now_ticks(&ticks);if(r){errno=r;return -1;}
 *out=(struct tmc_timespec){(int64_t)(ticks/HZ),(long)(ticks%HZ)*NS_PER_TICK};errno=saved;return 0;
}
int tmc_clock_getres(int clock,struct tmc_timespec*out)
{
 if(clock!=TMC_CLOCK_MONOTONIC){errno=clock==TMC_CLOCK_REALTIME?ENOTSUP:EINVAL;return -1;}
 if(out)*out=(struct tmc_timespec){0,NS_PER_TICK};
 return 0;
}
int tmc_clock_nanosleep(int clock,int flags,const struct tmc_timespec*request,struct tmc_timespec*remaining)
{
 int saved=errno,r=0;uint64_t duration,now,deadline;
 if(clock!=TMC_CLOCK_MONOTONIC)return clock==TMC_CLOCK_REALTIME?ENOTSUP:EINVAL;
 if(flags!=0&&flags!=TMC_TIMER_ABSTIME)return EINVAL;
 r=conversion(request,&duration);if(r)return r;
 r=tmc_time_now_ticks(&now);if(r){errno=saved;return r;}
 if(flags==TMC_TIMER_ABSTIME)deadline=duration;
 else{
  if(!duration){if(remaining)*remaining=(struct tmc_timespec){0};errno=saved;return 0;}
  if(duration==UINT64_MAX||now>UINT64_MAX-duration-1){errno=saved;return EOVERFLOW;}
  deadline=now+duration+1;
 }
 while(now<deadline){
  uint64_t delta=deadline-now;if(delta>360000)delta=360000;
  if(tmc_syscall(S_SLEEP,delta,0,0,0)<0){r=EIO;break;}
  r=tmc_time_now_ticks(&now);if(r)break;
 }
 if(!r&&remaining)*remaining=(struct tmc_timespec){0};
 errno=saved;return r;
}
int tmc_nanosleep(const struct tmc_timespec*request,struct tmc_timespec*remaining)
{
 int r=tmc_clock_nanosleep(TMC_CLOCK_MONOTONIC,0,request,remaining);if(r){errno=r;return -1;}return 0;
}
