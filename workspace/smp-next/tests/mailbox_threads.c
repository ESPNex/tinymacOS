/* Real shared mailbox algorithm, mocked invalidation, host pthread workers.
 * This tests ordering/liveness, NOT an x86 TLB or interrupt controller. */
#include "tlb_mailbox.h"
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
static TmcTLBMailbox box;
static unsigned ready,stop,allow_last=1,held_lock=1;
static unsigned flushes[8],waiting[8];
static u64 observed[8];
static void invalidate(u64 address,void *context)
{
    unsigned cpu=(unsigned)(uintptr_t)context;
    __atomic_store_n(&observed[cpu],address,__ATOMIC_RELAXED);
    __atomic_add_fetch(&flushes[cpu],1,__ATOMIC_RELAXED);
}
static void *worker(void *arg)
{
    unsigned cpu=(unsigned)(uintptr_t)arg;
    __atomic_add_fetch(&ready,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&stop,__ATOMIC_ACQUIRE)) {
        /* The sender holds this lock throughout the test. With only an IPI
         * and IRQs off a real CPU could never acknowledge, so the owner waits
         * forever. Polling pending TLB work must not require acquiring it. */
        while(__atomic_load_n(&held_lock,__ATOMIC_ACQUIRE)) {
            __atomic_store_n(&waiting[cpu],1,__ATOMIC_RELEASE);
            if(cpu!=7 || __atomic_load_n(&allow_last,__ATOMIC_ACQUIRE))
                tlb_mailbox_poll(&box,cpu,invalidate,arg);
            if(__atomic_load_n(&stop,__ATOMIC_ACQUIRE))return 0;
            /* Yield only the host fixture to avoid monopolizing the sandbox. */
            struct timespec pause={0,100000};nanosleep(&pause,0);
        }
    }
    return 0;
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void wait_done(void)
{
    double end=now()+10;
    while(!tlb_mailbox_done(&box)){assert(now()<end);sched_yield();}
}
int main(void)
{
    pthread_t threads[7];
    for(unsigned c=1;c<8;c++)assert(!pthread_create(&threads[c-1],0,worker,(void *)(uintptr_t)c));
    while(__atomic_load_n(&ready,__ATOMIC_ACQUIRE)!=7)sched_yield();
    for(unsigned c=1;c<8;c++)while(!__atomic_load_n(&waiting[c],__ATOMIC_ACQUIRE))sched_yield();
    __atomic_store_n(&allow_last,0,__ATOMIC_RELEASE);
    tlb_mailbox_begin(&box,255,0x12345000);tlb_mailbox_poll(&box,0,invalidate,0);
    /* Simulated reclamation MUST stay blocked until the last CPU flushes. */
    double end=now()+10;
    for(unsigned c=1;c<7;c++)while(__atomic_load_n(&box.ack[c],__ATOMIC_ACQUIRE)!=1){assert(now()<end);sched_yield();}
    assert(!tlb_mailbox_done(&box));assert(!__atomic_load_n(&flushes[7],__ATOMIC_RELAXED));
    __atomic_store_n(&allow_last,1,__ATOMIC_RELEASE);wait_done();tlb_mailbox_finish(&box);
    for(unsigned request=1;request<=10000;request++) {
        u64 address=(u64)request*4096;
        tlb_mailbox_begin(&box,255,address);tlb_mailbox_poll(&box,0,invalidate,0);wait_done();
        for(unsigned c=0;c<8;c++) {
            assert(__atomic_load_n(&observed[c],__ATOMIC_RELAXED)==address);
            assert(__atomic_load_n(&flushes[c],__ATOMIC_RELAXED)==request+1);
        }
        tlb_mailbox_finish(&box);
        if(request%1000==0){printf("requests=%u\n",request);fflush(stdout);}
    }
    __atomic_store_n(&stop,1,__ATOMIC_RELEASE);
    for(unsigned c=1;c<8;c++)assert(!pthread_join(threads[c-1],0));
    puts("PASS: 10001 serialized requests / 8 host workers, IRQ-off lock-wait polling, delayed final ACK prevents premature reclamation; mock invalidation only.");
}
