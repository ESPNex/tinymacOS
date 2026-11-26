#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include "tmc_time.h"
#include "tmc_backend.h"
#include "abi.h"
static uint64_t tick,slept,largest;static unsigned sleeps;static int fail_clock,fail_sleep,short_sleep;
long tmc_syscall(long n,uint64_t a,uint64_t b,uint64_t c,uint64_t d)
{
 (void)b;(void)c;(void)d;
 if(n==S_TICKS)return fail_clock?-1:(long)tick;
 assert(n==S_SLEEP&&a&&a<=360000);sleeps++;slept+=a;if(a>largest)largest=a;
 if(fail_sleep)return -1;
 tick+=short_sleep?1:a;return 0;
}
int main(void)
{
 struct tmc_timespec t={99,99},res={99,99};uint64_t deadline=99;
 tick=123;errno=72;assert(!tmc_clock_gettime(TMC_CLOCK_MONOTONIC,&t)&&t.tv_sec==1&&t.tv_nsec==230000000&&errno==72);
 assert(!tmc_clock_getres(TMC_CLOCK_MONOTONIC,&res)&&res.tv_sec==0&&res.tv_nsec==10000000&&errno==72);
 assert(!tmc_clock_getres(TMC_CLOCK_MONOTONIC,0));
 assert(tmc_clock_gettime(TMC_CLOCK_REALTIME,&t)==-1&&errno==ENOTSUP);
 assert(tmc_clock_gettime(99,&t)==-1&&errno==EINVAL);
 assert(tmc_clock_gettime(TMC_CLOCK_MONOTONIC,0)==-1&&errno==EFAULT);
 struct tmc_timespec bad[]={ {-1,0},{0,-1},{0,1000000000L} };
 for(unsigned i=0;i<3;i++)assert(tmc_time_deadline_ticks(bad+i,&deadline)==EINVAL&&deadline==99);
 t=(struct tmc_timespec){INT64_MAX,999999999};assert(tmc_time_deadline_ticks(&t,&deadline)==EOVERFLOW&&deadline==99);
 assert(tmc_time_deadline_ticks(0,&deadline)==EFAULT&&tmc_time_deadline_ticks(&t,0)==EOVERFLOW);
 t=(struct tmc_timespec){0,0};assert(!tmc_time_deadline_ticks(&t,&deadline)&&deadline==1);
 t=(struct tmc_timespec){1,1};assert(!tmc_time_deadline_ticks(&t,&deadline)&&deadline==101);
 t.tv_nsec=10000000;assert(!tmc_time_deadline_ticks(&t,&deadline)&&deadline==101);
 t.tv_nsec=10000001;assert(!tmc_time_deadline_ticks(&t,&deadline)&&deadline==102);
 errno=73;t=(struct tmc_timespec){0,0};assert(!tmc_clock_nanosleep(TMC_CLOCK_MONOTONIC,0,&t,&res)&&!sleeps&&errno==73);
 t.tv_nsec=1;assert(!tmc_clock_nanosleep(TMC_CLOCK_MONOTONIC,0,&t,&res)&&sleeps==1&&slept==2&&tick==125&&errno==73);
 t=(struct tmc_timespec){0,1};assert(!tmc_clock_nanosleep(TMC_CLOCK_MONOTONIC,TMC_TIMER_ABSTIME,&t,0)&&sleeps==1);
 tick=0;sleeps=0;slept=0;t=(struct tmc_timespec){7201,0};assert(!tmc_clock_nanosleep(TMC_CLOCK_MONOTONIC,TMC_TIMER_ABSTIME,&t,0)&&sleeps==3&&tick==720100&&largest==360000);
 short_sleep=1;tick=10;sleeps=0;t=(struct tmc_timespec){0,150000000};assert(!tmc_clock_nanosleep(TMC_CLOCK_MONOTONIC,TMC_TIMER_ABSTIME,&t,0)&&tick==15&&sleeps==5);short_sleep=0;
 fail_clock=1;t=(struct tmc_timespec){7,17};errno=74;assert(tmc_clock_gettime(TMC_CLOCK_MONOTONIC,&t)==-1&&errno==EIO&&t.tv_sec==7&&t.tv_nsec==17);
 errno=75;assert(tmc_clock_nanosleep(TMC_CLOCK_MONOTONIC,0,&t,0)==EIO&&errno==75);fail_clock=0;
 fail_sleep=1;assert(tmc_clock_nanosleep(TMC_CLOCK_MONOTONIC,0,&t,0)==EIO&&errno==75);assert(tmc_nanosleep(&t,0)==-1&&errno==EIO);fail_sleep=0;
 errno=76;assert(tmc_clock_nanosleep(TMC_CLOCK_REALTIME,0,&t,0)==ENOTSUP&&errno==76);
 assert(tmc_clock_nanosleep(TMC_CLOCK_MONOTONIC,99,&t,0)==EINVAL&&errno==76);
 assert(tmc_clock_nanosleep(TMC_CLOCK_MONOTONIC,0,&bad[0],0)==EINVAL&&errno==76);
 puts("PASS time: monotonic 100Hz conversion/ceil, invalid/overflow/output preservation, fail-closed realtime, zero/past sleeps, conservative relative phase, 1h chunking, short-wake recheck, syscall failures and errno contracts (fake transport)");
}
