#include "tmc_tls.h"
#include "tmc_backend.h"
#include "abi.h"
#include "tlsabi.h"
#define COOKIE UINT64_C(0x544d435448524431)
long tmc_tls_install(struct tmc_tls_block *p)
{
    if(!p || ((uintptr_t)p&7))return -1;
    p->self=(uint64_t)(uintptr_t)p;p->error_number=0;p->reserved=0;p->cookie=COOKIE;
    return tmc_syscall(S_TLS,TMC_TLS_SET,(uint64_t)(uintptr_t)p,0,0);
}
long tmc_tls_uninstall(void){return tmc_syscall(S_TLS,TMC_TLS_SET,0,0,0);}
struct tmc_tls_block *tmc_tls_current(void)
{
    long base=tmc_syscall(S_TLS,TMC_TLS_GET,0,0,0);
    if(base<=0)return 0;
    struct tmc_tls_block *p;
    __asm__ volatile("movq %%fs:0,%0":"=r"(p)::"memory");
    if((uintptr_t)p!=(uintptr_t)base || p->cookie!=COOKIE)return 0;
    return p;
}
int *tmc_tls_errno_location(void)
{
    struct tmc_tls_block *p=tmc_tls_current();return p?&p->error_number:0;
}
