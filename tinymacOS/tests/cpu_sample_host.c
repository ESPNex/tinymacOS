#include <assert.h>
#include <stdio.h>
#include "../include/cpu_sample.h"
int main(void){
 assert(cpu_percent(0,0)==0&&cpu_percent(0,100)==0);
 assert(cpu_percent(100,400)==25); /* one full core on four CPUs */
 assert(cpu_percent(400,400)==100&&cpu_percent(100,100)==100);
 assert(cpu_percent(1,3)==33&&cpu_percent(2,3)==66);
 assert(cpu_percent((~(u64)0)/2,~(u64)0)==49);
 assert(cpu_percent(~(u64)0,~(u64)0)==100);
 for(u64 total=1;total<500;total++)for(u64 busy=0;busy<=total;busy++)assert(cpu_percent(busy,total)==busy*100/total);
 u64 dt,db;assert(cpu_interval(200,70,100,20,&dt,&db)&&dt==100&&db==50);
 assert(!cpu_interval(200,70,0,0,&dt,&db)&&!dt&&!db);
 assert(!cpu_interval(99,70,100,20,&dt,&db));
 assert(!cpu_interval(200,19,100,20,&dt,&db));
 assert(!cpu_interval(200,121,100,20,&dt,&db));
 puts("PASS CPU intervals: SMP one-core/4 = 25%, all cores = 100%, zero/reset/inconsistent counters rejected, percentage safe at UINT64_MAX");
}
