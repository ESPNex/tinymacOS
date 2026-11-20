/* Real native compare/park/wake, lock/condition SMP. No pthread/join claim. */
#include "lib.h"
#include "tmc_sync.h"
#include "tmc_tls.h"
#include "wordwaitabi.h"
#include <errno.h>
static struct tmc_tls_block tcbs[5] ALIGNED(16);
static struct tmc_mutex mutex=TMC_MUTEX_INITIALIZER;
static struct tmc_condition condition=TMC_CONDITION_INITIALIZER;
static u32 ready,go,done,counter,inside;
static volatile u32 failed,mask;
static void bad_at(u32 line){u32 zero=0;__atomic_compare_exchange_n(&failed,&zero,line,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED);}
#define bad() bad_at(__LINE__)
static void say(const char *s){sc(S_STDOUT,s,strlen(s),0,0);}
static void num(u64 n){char b[24];number(b,n);say(b);}
static void sample(void){SysInfo s;if(sc(S_INFO,&s,0,0,0)||s.current_cpu>=8)bad();else __atomic_fetch_or(&mask,1u<<s.current_cpu,__ATOMIC_RELAXED);}
static void worker(u64 id)
{
 if(tmc_tls_install(&tcbs[id]))bad();
 errno=(int)(id*111);
 if(tmc_mutex_lock(&mutex))bad();
 ready++;
 if(tmc_condition_broadcast(&condition))bad();
 while(!go)if(tmc_condition_wait(&condition,&mutex,sc(S_TICKS,0,0,0,0)+12000)){bad();break;}
 if(tmc_mutex_unlock(&mutex))bad();
 for(unsigned j=0;j<60;j++){
  /* Break the lock-handoff convoy so the probe actually exercises multiple
   * CPUs, not 240 correct serialized sections all on one scheduler CPU. */
  slp(1);
  if(tmc_mutex_lock(&mutex)){bad();break;}
  if(inside)bad();
  inside=1;u32 old=counter;
  sample();slp(0);counter=old+1;inside=0;
  if(errno!=(int)(id*111))bad();
  if(tmc_mutex_unlock(&mutex))bad();
 }
 if(tmc_mutex_lock(&mutex))bad();
 done++;
 if(tmc_condition_broadcast(&condition)||tmc_mutex_unlock(&mutex))bad();
 if(tmc_tls_uninstall())bad();
 sc(S_EXIT,1,0,0,0);for(;;){}
}
void app_main(void)
{
 if(tmc_tls_install(&tcbs[0]))bad();
 errno=777;
 if(tmc_word_query()!=TMC_WORD_ABI_VERSION)bad();
 u32 word=3;u64 start=sc(S_TICKS,0,0,0,0);
 if(tmc_word_wait(&word,4,0)!=EAGAIN || tmc_word_wake(&word,0)!=0 || tmc_word_wake(&word,~0u)!=0)bad();
 if(tmc_word_wait((void *)1,0,0)!=EINVAL || tmc_word_wait((void *)(u64)app_main,0,0)!=EINVAL ||
    sc(S_WORDWAIT,TMC_WORD_WAIT,(u64)&word,0x100000000ull,0)!=TMC_WORD_INVALID ||
    sc(S_WORDWAIT,99,0,0,0)!=TMC_WORD_INVALID)bad();
 if(tmc_word_wait(&word,3,start+3)!=ETIMEDOUT || (u64)sc(S_TICKS,0,0,0,0)<start+3)bad();
 if(tmc_mutex_lock(&mutex)||tmc_mutex_trylock(&mutex)!=EBUSY)bad();
 if(tmc_mutex_lock_until(&mutex,sc(S_TICKS,0,0,0,0)+3)!=ETIMEDOUT)bad();
 if(tmc_condition_wait(&condition,&mutex,sc(S_TICKS,0,0,0,0)+3)!=ETIMEDOUT || tmc_mutex_trylock(&mutex)!=EBUSY)bad();
 if(tmc_mutex_unlock(&mutex)||tmc_mutex_unlock(&mutex)!=EPERM)bad();
 for(u64 i=1;i<=4;i++)if(sc(S_THREAD,worker,i,0,0)<0)bad();
 if(tmc_mutex_lock(&mutex))bad();
 while(ready!=4)if(tmc_condition_wait(&condition,&mutex,sc(S_TICKS,0,0,0,0)+12000)){bad();break;}
 go=1;if(tmc_condition_broadcast(&condition))bad();
 while(done!=4)if(tmc_condition_wait(&condition,&mutex,sc(S_TICKS,0,0,0,0)+12000)){bad();break;}
 if(counter!=240||inside||done!=4||errno!=777)bad();
 if(tmc_mutex_unlock(&mutex))bad();
 SysInfo s;sc(S_INFO,&s,0,0,0);if(s.ncpu>1 && !(mask&(mask-1)))bad();
 say(failed?"SYNC PARK FAIL\n":"SYNC PARK SMP PASS\n");say("updates=");num(counter);say(" workers=4 cpus=");num(s.ncpu);
 say("\ncompare-park=atomic timeout=relocked\nthread-errno=preserved\n");
 say("first-failure-line=");num(failed);say(" cpu-mask=");num(mask);say("\n");
 sc(S_RETURN,failed?42:0,0,0,0);
}
