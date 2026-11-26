#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include "tmc_vm.h"
#include "tmc_sync.h"
#include "heap_cases.h"
/* Hosted VM transport only: mmap guards + fault injection, NOT target MMU. */
static struct {void *base,*mapping;size_t bytes,total;} regions[TMC_VM_SLOTS];
static unsigned fail_reserve,fail_commit,fail_release,limit=TMC_VM_SLOTS;
static pthread_mutex_t mu=PTHREAD_MUTEX_INITIALIZER;
int tmc_mutex_lock(struct tmc_mutex*m){(void)m;return pthread_mutex_lock(&mu);}
int tmc_mutex_unlock(struct tmc_mutex*m){(void)m;return pthread_mutex_unlock(&mu);}
void *tmc_vm_reserve(size_t n,size_t a,unsigned g)
{
 assert(a==4096&&g==1&&n<=TMC_VM_MAX_REGION&&!(n%4096));
 if(fail_reserve){fail_reserve--;errno=ENOMEM;return 0;}
 unsigned i=0;while(i<limit&&regions[i].base)i++;
 if(i==limit){errno=ENOMEM;return 0;}
 void*p=mmap(0,n+8192,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(p==MAP_FAILED)return 0;
 regions[i].mapping=p;regions[i].base=(char*)p+4096;regions[i].bytes=n;regions[i].total=n+8192;return regions[i].base;
}
int tmc_vm_commit(void*p,size_t n,unsigned prot)
{
 assert(prot==(TMC_VM_READ|TMC_VM_WRITE));if(fail_commit){fail_commit--;errno=ENOMEM;return -1;}
 return mprotect(p,n,PROT_READ|PROT_WRITE);
}
int tmc_vm_release(void*p,size_t n)
{
 if(fail_release){fail_release--;errno=EIO;return -1;}
 for(unsigned i=0;i<TMC_VM_SLOTS;i++)if(regions[i].base==p){assert(n==regions[i].bytes);assert(!munmap(regions[i].mapping,regions[i].total));regions[i].base=0;return 0;}
 assert(0);return -1;
}
int main(void)
{
 heap_sequential();assert(!heap_failure);
 fail_reserve=1;assert(!tmc_heap_alloc(42)&&errno==ENOMEM);
 fail_commit=1;assert(!tmc_heap_alloc(42)&&errno==ENOMEM);assert(!tmc_heap_statistics().spans);
 fail_commit=fail_release=1;assert(!tmc_heap_alloc(42)&&errno==ENOMEM);assert(tmc_heap_statistics().spans==1);
 void*p=tmc_heap_alloc(42);assert(p);tmc_heap_free(p);assert(!tmc_heap_statistics().spans);
 p=tmc_heap_alloc(100);fill(p,100,44);fail_release=1;errno=91;tmc_heap_free(p);assert(errno==91&&tmc_heap_statistics().spans==1);
 p=tmc_heap_alloc(100);assert(p);tmc_heap_free(p);assert(!tmc_heap_statistics().spans);
 limit=2;void*a=tmc_heap_alloc(1<<20),*b=tmc_heap_alloc(1<<20);assert(a&&b&&!tmc_heap_alloc(1<<20)&&errno==ENOMEM);tmc_heap_free(a);tmc_heap_free(b);limit=TMC_VM_SLOTS;
 pthread_t threads[4];struct HeapJob jobs[4];
 for(unsigned i=0;i<4;i++){jobs[i]=(struct HeapJob){i+78,5000,tmc_heap_alloc(1024)};assert(jobs[i].transfer);fill(jobs[i].transfer,1024,jobs[i].seed);assert(!pthread_create(threads+i,0,heap_worker,jobs+i));}
 for(unsigned i=0;i<4;i++){void*r=0;assert(!pthread_join(threads[i],&r)&&r==jobs+i&&!jobs[i].transfer);}
 assert(!heap_failure&&!tmc_heap_statistics().spans&&!tmc_heap_statistics().live);
 puts("PASS heap: 25000 randomized operations, 4 concurrent workers, cross-thread frees, alignment 8..2MiB, calloc reuse/overflow, failed realloc preservation, split/coalesce, VM reserve/commit/release faults and region exhaustion (host transport)");
}
