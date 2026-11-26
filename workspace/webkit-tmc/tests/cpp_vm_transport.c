/* Hosted VM transport only. Actual guest links posix/src/vm.c instead. */
#define _GNU_SOURCE
#include <assert.h>
#include <sys/mman.h>
#include <errno.h>
#include "tmc_vm.h"
static struct {void *base,*mapping;size_t bytes;} spans[TMC_VM_SLOTS];
void *tmc_vm_reserve(size_t n,size_t a,unsigned guards)
{
 assert(a==4096&&guards==1&&n<=TMC_VM_MAX_REGION&&!(n%4096));
 unsigned i=0;while(i<TMC_VM_SLOTS&&spans[i].base)i++;
 if(i==TMC_VM_SLOTS){errno=ENOMEM;return 0;}
 void*p=mmap(0,n+8192,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(p==MAP_FAILED)return 0;
 spans[i].mapping=p;spans[i].base=(char*)p+4096;spans[i].bytes=n;return spans[i].base;
}
int tmc_vm_commit(void*p,size_t n,unsigned protection)
{
 assert(protection==(TMC_VM_READ|TMC_VM_WRITE));return mprotect(p,n,PROT_READ|PROT_WRITE);
}
int tmc_vm_release(void*p,size_t n)
{
 for(unsigned i=0;i<TMC_VM_SLOTS;i++)if(spans[i].base==p){assert(spans[i].bytes==n);int r=munmap(spans[i].mapping,n+8192);if(!r)spans[i].base=0;return r;}
 assert(0);return -1;
}
