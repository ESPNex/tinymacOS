#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../kernel/k.h"
static void hostlock(Spin*s,u32*f){(void)s;*f=0;}
static void hostunlock(Spin*s,u32 f){(void)s;(void)f;}
#define spin_lock hostlock
#define spin_unlock hostunlock
#include "../kernel/memory.c"
__attribute__((noreturn)) void retail_stop(void){abort();}
void tlb_shoot(u64 a){(void)a;}
int main(void){
 u64 begin=0x10000000,bytes=8*1024*1024;
 assert(mmap((void *)begin,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void *)begin);
 memset(used,255,sizeof used);limit=(begin+bytes)/PAGE;hint=begin/PAGE;
 for(unsigned j=begin/PAGE;j<limit;j++)clear(j);
 totalpages=freepages=bytes/PAGE;firmware_pages=totalpages+123;
 kroot=(u64 *)page_get();u64 *r=vm_new();assert(r);
 SysInfo b={0},a={0};mem_info(&b);assert(b.total==bytes&&b.ram_bytes==bytes+123*PAGE&&b.reserved_bytes==123*PAGE&&!b.user_bytes);
 for(int i=0;i<1024;i++)assert(vm_add(r,UBASE+i*PAGE,2|NX));
 mem_info(&a);assert(a.user_bytes==1024*PAGE&&vm_resident(r)==1024*PAGE&&a.free<=b.free-1024*PAGE);
 assert(!vm_add(r,UBASE,2|NX));mem_info(&a);assert(a.user_bytes==1024*PAGE);
 for(int i=0;i<512;i++)vm_unmap(r,UBASE+i*PAGE);
 vm_unmap(r,UBASE);mem_info(&a);assert(a.user_bytes==512*PAGE&&vm_resident(r)==512*PAGE);
 vm_drop(r);mem_info(&a);assert(!a.user_bytes&&a.free==b.free+PAGE); /* root also released */
 for(int j=0;j<10;j++){
  r=vm_new();assert(r);for(int i=0;i<5;i++)assert(vm_add(r,UBASE+0x30000000+i*PAGE,2|NX));
  assert(vm_resident(r)==5*PAGE);vm_drop(r);mem_info(&a);assert(!a.user_bytes&&a.free==b.free+PAGE);
 }
 r=vm_new();assert(r);unsigned committed=0;
 while(vm_add(r,UBASE+committed*PAGE,2|NX))committed++;
 assert(committed>1000);mem_info(&a);assert(a.user_bytes==committed*PAGE&&a.free<4*PAGE);
 vm_drop(r);mem_info(&a);assert(!a.user_bytes&&a.free==b.free+PAGE);
 page_put((u64)kroot);assert(freepages==totalpages);munmap((void *)begin,bytes);
 puts("PASS actual page allocator/VM: committed leaf vs page-table backing, duplicate/absent mapping, unmap and full process teardown, 10 leak-free remaps, allocation exhaustion rollback/teardown, locked pool/free/UEFI/user snapshot");
}
