#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include "../kernel/k.h"
static _Thread_local PerCpu local;
static PerCpu *hostpc(void){return &local;}
#undef pc
#define pc() hostpc()
#undef current
#define current() (&proc[task[cur].pi])
#include "../kernel/sched.c"
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static void *count(void *arg){
 int c=(int)(u64)arg;local.cpu=c;local.cur_slot=c+1;
 for(int i=0;i<30000;i++){
  pthread_mutex_lock(&gate);cpu_account_tick();pthread_mutex_unlock(&gate);
 }
 return 0;
}
int main(void){
 for(int c=0;c<8;c++){idle_slots[c]=NTASK-1-c;task[c+1].pin=-1;task[c+1].pi=1;}
 local.cpu=0;local.cur_slot=idle_slots[0];cpu_account_tick();assert(local.core_ticks==1&&!local.core_busy&&!busy_ticks&&!proc[0].cpu_ticks);
 local.cur_slot=1;task[1].pin=0;task[1].pi=0;cpu_account_tick();assert(local.core_ticks==2&&local.core_busy==1&&busy_ticks==1&&proc[0].cpu_ticks==1);
 task[1].pin=-1;task[1].pi=1;pthread_t threads[8];
 for(int i=0;i<8;i++)assert(!pthread_create(threads+i,0,count,(void *)(u64)i));
 for(int i=0;i<8;i++)assert(!pthread_join(threads[i],0));
 assert(proc[1].cpu_ticks==8*30000&&busy_ticks==8*30000+1&&cpu_samples==8*30000+2);
 local.core_ticks=~0ull;local.core_busy=~0ull;local.cur_slot=1;cpu_account_tick();assert(!local.core_ticks&&!local.core_busy);
 puts("PASS actual timer accounting: only true idle excluded, pinned kernel USB included, eight concurrent threads accrue exact shared-process ticks under scheduler lock, defined unsigned wrap");
}
