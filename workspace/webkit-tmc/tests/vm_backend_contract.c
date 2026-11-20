#include "tmc_vm.h"
#include "abi.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
static long result;
static uint64_t op,b,c,d;
static int version=1;
long tmc_syscall(long n,uint64_t a,uint64_t x,uint64_t y,uint64_t z){
 assert(n==S_VM);op=a;b=x;c=y;d=z;
 if(a==TMC_VM_QUERY && result>=0){struct TmcVMInfo *p=(void *)(uintptr_t)x;p->version=version;p->page_size=4096;}
 return result;
}
int main(void){
 struct TmcVMInfo info;
 result=0;assert(!tmc_vm_info(&info)&&info.version==1&&op==0);
 assert(tmc_vm_info(0)==-1&&errno==EFAULT);
 version=2;assert(tmc_vm_info(&info)==-1&&errno==ENOTSUP);version=1;
 result=TMC_VM_ARENA_START;void *p=tmc_vm_reserve(8192,65536,2);
 assert((uintptr_t)p==TMC_VM_ARENA_START&&op==1&&b==8192&&c==65536&&d==2);
 result=0;assert(!tmc_vm_commit(p,8192,3)&&op==2&&b==(uintptr_t)p&&c==8192&&d==3);
 assert(!tmc_vm_protect(p,4096,0)&&op==3&&!d);
 assert(!tmc_vm_decommit(p,8192)&&op==4&&!d);
 assert(!tmc_vm_release(p,8192)&&op==5&&!d);
 const long errors[]={TMC_VM_INVALID,TMC_VM_NOMEM,TMC_VM_UNSUPPORTED,TMC_VM_BUSY,TMC_VM_NOT_COMMITTED,-99};
 const int expected[]={EINVAL,ENOMEM,ENOTSUP,EBUSY,ENOMEM,EIO};
 for(unsigned i=0;i<6;i++){
  result=errors[i];assert(!tmc_vm_reserve(4096,4096,0)&&errno==expected[i]);
  assert(tmc_vm_commit(p,4096,3)==-1&&errno==expected[i]);
 }
 result=0;assert(!tmc_vm_reserve(4096,4096,0)&&errno==EIO);
 result=TMC_VM_ARENA_START+1;assert(!tmc_vm_reserve(4096,4096,0)&&errno==EIO);
 puts("PASS: VM native ABI forwarding, feature query, pointer sanity and errno mapping.");
}
