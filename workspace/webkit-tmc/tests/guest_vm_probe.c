/* QA-only, invoked through command aliases in a disposable test volume.
 * Real user-mode MMU tests, including deliberately killed child processes.
 * NOT a browser, libc, mmap compatibility layer or production command program. */
#include "lib.h"
#include "tmc_tls.h"
static struct tmc_tls_block probe_tls ALIGNED(16);
#include "tmc_vm.h"
#include <errno.h>
static ExecArgs args;
static volatile unsigned char sink;
static void say(const char *s){unsigned n=0;while(s[n])n++;sc(S_STDOUT,s,n,0,0);}
static int eqs(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int failed(const char *reason){say("VM FAIL ");say(reason);say("\n");return 42;}
static int success(void){
 struct TmcVMInfo info;
 if(tmc_vm_info(&info) || info.version!=1 || info.page_size!=4096)return failed("query");
 if(!(info.flags&TMC_VM_AVAILABLE)) {
  if(tmc_vm_reserve(4096,4096,1) || errno!=ENOTSUP)return failed("SMP refusal");
  say("VM SMP REFUSAL PASS\n");return 0;
 }
 volatile unsigned char *p=tmc_vm_reserve(4*4096,65536,1);
 if(!p || ((uintptr_t)p%65536))return failed("reserve/alignment");
 /* IN/OUT validation must reject an uncommitted or guard address, not fault in kernel. */
 if(sc(S_STDOUT,p,1,0,0)!=-1 || sc(S_STDOUT,p-1,1,0,0)!=-1)return failed("uncommitted input check");
 if(tmc_vm_commit((void *)(p-4096),4096,3)!=-1 || errno!=EINVAL)return failed("guard commit rejection");
 if(tmc_vm_commit((void *)p,4*4096,3))return failed("commit");
 for(unsigned i=0;i<4*4096;i++)if(p[i])return failed("initial zero");
 for(unsigned i=0;i<4*4096;i++)p[i]=(unsigned char)(i*13+29);
 if(tmc_vm_commit((void *)p,4096,3)!=-1 || errno!=EBUSY)return failed("repeat commit");
 if(tmc_vm_protect((void *)p,4096,TMC_VM_READ))return failed("protect readonly");
 for(unsigned i=0;i<4096;i++)if(p[i]!=(unsigned char)(i*13+29))return failed("readonly contents");
 /* S_READ validates destination before filesystem handle lookup. */
 if(sc(S_READ,99,p,1,0)!=-1)return failed("readonly output check");
 if(tmc_vm_protect((void *)p,4096,0))return failed("protect none");
 if(sc(S_STDOUT,p,1,0,0)!=-1)return failed("noaccess input check");
 if(tmc_vm_protect((void *)p,4096,3))return failed("restore RW");
 if(p[4]!=(unsigned char)(4*13+29))return failed("none retained contents");
 p[4]=77;
 if(tmc_vm_protect((void *)p,4096,7)!=-1 || errno!=ENOTSUP)return failed("exec rejection");
 if(tmc_vm_decommit((void *)p,2*4096) || tmc_vm_decommit((void *)p,2*4096))return failed("decommit");
 if(sc(S_STDOUT,p,1,0,0)!=-1)return failed("decommitted validation");
 if(tmc_vm_protect((void *)p,3*4096,1)!=-1 || errno!=ENOMEM)return failed("hole protect atomicity");
 p[3*4096]=88; /* prior RW page must remain writable after failed protect */
 if(tmc_vm_commit((void *)p,2*4096,3))return failed("recommit");
 for(unsigned i=0;i<2*4096;i++)if(p[i])return failed("recommit zero");
 if(p[3*4096]!=88)return failed("unaffected page");
 if(tmc_vm_release((void *)p,4096)!=-1 || errno!=EINVAL)return failed("partial release");
 if(tmc_vm_release((void *)p,4*4096))return failed("release");
 if(tmc_vm_protect((void *)p,4096,3)!=-1 || errno!=EINVAL)return failed("released protection");
 /* Reservation slot exhaustion, then clean reuse. */
 void *slots[32];for(unsigned i=0;i<32;i++){slots[i]=tmc_vm_reserve(4096,4096,1);if(!slots[i])return failed("slot reserve");}
 if(tmc_vm_reserve(4096,4096,0) || errno!=ENOMEM)return failed("slot exhaustion");
 for(unsigned i=0;i<32;i++)if(tmc_vm_release(slots[i],4096))return failed("slot release");
 p=tmc_vm_reserve(2*1024*1024,2*1024*1024,1);
 if(!p || ((uintptr_t)p%(2*1024*1024)))return failed("large alignment");
 if(tmc_vm_commit((void *)p,2*1024*1024,3))return failed("large commit");
 for(unsigned i=0;i<2*1024*1024;i+=4096)p[i]=(unsigned char)(i/4096);
 for(unsigned i=0;i<2*1024*1024;i+=4096)if(p[i]!=(unsigned char)(i/4096))return failed("large pattern");
 if(tmc_vm_release((void *)p,2*1024*1024))return failed("large release");
 say("VM PASS reserve/commit/protect/guards\nVM PASS decommit/recommit zero/slots\nVM PASS 2MiB alignment and backing\n");return 0;
}
static int fault(const char *kind){
 volatile unsigned char *p=tmc_vm_reserve(4096,4096,1);
 if(!p)return failed("fault reserve");
 if(tmc_vm_commit((void *)p,4096,3))return failed("fault commit");
 p[0]=0xc3; /* x86 RET instruction used ONLY by NX negative test. */
 if(eqs(kind,"readonly") && tmc_vm_protect((void *)p,4096,1))return failed("RO precondition");
 if(eqs(kind,"none") && tmc_vm_protect((void *)p,4096,0))return failed("NONE precondition");
 if(eqs(kind,"decommitted") && tmc_vm_decommit((void *)p,4096))return failed("decommit precondition");
 if(eqs(kind,"released") && tmc_vm_release((void *)p,4096))return failed("release precondition");
 int known=eqs(kind,"left")||eqs(kind,"right")||eqs(kind,"readonly")||eqs(kind,"none")||eqs(kind,"decommitted")||eqs(kind,"released")||eqs(kind,"nx");
 if(!known)return failed("unknown fault kind");
 say("FAULT ARMED ");say(kind);say("\n");
 if(eqs(kind,"left"))sink=p[-1];
 else if(eqs(kind,"right"))sink=p[4096];
 else if(eqs(kind,"readonly"))p[0]=0x55;
 else if(eqs(kind,"nx"))((void (*)(void))(uintptr_t)p)();
 else sink=p[0];
 say("VM UNEXPECTED SURVIVAL\n");return 77;
}
void app_main(void){
 if(tmc_tls_install(&probe_tls)){sc(S_RETURN,42,0,0,0);return;}
 if(sc(S_ARGV,&args,0,0,0)){say("VM FAIL argv\n");sc(S_RETURN,42,0,0,0);return;}
 int result;
 if(args.argc>=4 && eqs(args.argv[1],"status")) {
  say("FAULT RESULT ");say(args.argv[3]);say(" status=");say(args.argv[2]);say("\n");result=0;
 } else if(args.argc>=3 && eqs(args.argv[1],"fault"))result=fault(args.argv[2]);
 else {result=success();if(!result && args.argc>=2 && eqs(args.argv[1],"post"))say("VM POSTFAULT PASS\n");}
 sc(S_RETURN,result,0,0,0);
}
