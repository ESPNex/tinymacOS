/* Native TLS/FS context-switch proof, not a JavaScriptCore/pthread claim. */
#include "lib.h"
#include "tlsabi.h"
#include "tmc_tls.h"
int errno; /* Existing unrelated network adapters still have their old ABI. */
static struct tmc_tls_block tcbs[5] ALIGNED(16);
static volatile u32 ready,go,done,failed,mask,checks;
static void bad(void){__atomic_store_n(&failed,1,__ATOMIC_RELEASE);}
static void say(const char *s){sc(S_STDOUT,s,strlen(s),0,0);}
static void num(u64 n){char s[24];number(s,n);say(s);}
static void sample(void){SysInfo s;if(sc(S_INFO,&s,0,0,0)||s.current_cpu>=8)bad();else __atomic_fetch_or(&mask,1u<<s.current_cpu,__ATOMIC_RELAXED);}
static void worker(u64 id)
{
 if(sc(S_TLS,TMC_TLS_GET,0,0,0)!=0)bad();
 if(tmc_tls_install(&tcbs[id])||!tmc_tls_errno_location())bad();
 __atomic_add_fetch(&ready,1,__ATOMIC_RELEASE);
 while(!__atomic_load_n(&go,__ATOMIC_ACQUIRE))slp(1);
 for(unsigned j=0;j<150;j++){
  int *e=tmc_tls_errno_location();if(!e){bad();break;}
  *e=(int)(id*100000+j);sample();slp(1);
  if(tmc_tls_current()!=&tcbs[id]||tmc_tls_errno_location()!=e||*e!=(int)(id*100000+j))bad();
  __atomic_add_fetch(&checks,1,__ATOMIC_RELAXED);
 }
 if(tmc_tls_uninstall()||sc(S_TLS,TMC_TLS_GET,0,0,0)!=0||tmc_tls_errno_location())bad();
 __atomic_add_fetch(&done,1,__ATOMIC_RELEASE);sc(S_EXIT,1,0,0,0);for(;;){}
}
void app_main(void)
{
 if(sc(S_TLS,TMC_TLS_QUERY,0,0,0)!=TMC_TLS_ABI_VERSION||sc(S_TLS,TMC_TLS_GET,0,0,0)||tmc_tls_errno_location())bad();
 if(tmc_tls_install(&tcbs[0]))bad();
 const u64 invalid[]={1,0xffff800000000000ull,0x800000000000ull,UBASE+3,UEND,~0ull};
 for(unsigned j=0;j<COUNT(invalid);j++)if(sc(S_TLS,TMC_TLS_SET,invalid[j],0,0)!=-1||(u64)sc(S_TLS,TMC_TLS_GET,0,0,0)!=(u64)&tcbs[0])bad();
 if(sc(S_TLS,99,0,0,0)!=-1)bad();
 int *main_error=tmc_tls_errno_location();if(main_error)*main_error=777;else bad();
 for(u64 i=1;i<=4;i++)if(sc(S_THREAD,worker,i,0,0)<0)bad();
 SysInfo s;sc(S_INFO,&s,0,0,0);u64 until=s.ticks+12000;
 while(ready!=4){slp(1);sc(S_INFO,&s,0,0,0);if(s.ticks>until){bad();break;}}
 __atomic_store_n(&go,1,__ATOMIC_RELEASE);
 while(done!=4){sample();if(tmc_tls_current()!=&tcbs[0]||!main_error||*main_error!=777)bad();slp(1);sc(S_INFO,&s,0,0,0);if(s.ticks>until){bad();break;}}
 if(s.ncpu>1 && !(mask&(mask-1)))bad();
 if(done!=4||checks!=600||tmc_tls_uninstall()||tmc_tls_errno_location())bad();
 say(failed?"TLS FS FAIL\n":"TLS FS SMP PASS\n");say("checks=");num(checks);say(" workers=4 cpus=");num(s.ncpu);say("\nerrno-per-thread=isolated\n");
 sc(S_RETURN,failed?42:0,0,0,0);
}
