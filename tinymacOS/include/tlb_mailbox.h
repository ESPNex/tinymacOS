#pragma once
#include "base.h"
/* Synchronous SMP shootdown mailbox. One serialized initiator; each CPU polls
 * with local interrupts disabled, including while spinning on another lock.
 * Publication/acknowledgement are release/acquire: the sender may reclaim pages
 * ONLY after tlb_mailbox_done(), never merely after sending the IPI.
 * IRQ-off polling and the IPI handler are mutually exclusive on each CPU. */
#define TMC_TLB_CPUS 8
typedef struct {
    u32 generation, serial, targets;
    u64 address;
    u32 ack[TMC_TLB_CPUS];
} TmcTLBMailbox;
static inline void tlb_mailbox_begin(TmcTLBMailbox *m,u32 mask,u64 address)
{
    u32 gen=++m->serial;
    if(!gen)gen=++m->serial; /* zero means no active request */
    __atomic_store_n(&m->targets,mask,__ATOMIC_RELAXED);
    __atomic_store_n(&m->address,address,__ATOMIC_RELAXED);
    __atomic_store_n(&m->generation,gen,__ATOMIC_RELEASE);
}
static inline void tlb_mailbox_poll(TmcTLBMailbox *m,unsigned cpu,
                                    void (*invalidate)(u64,void *),void *context)
{
    u32 gen=__atomic_load_n(&m->generation,__ATOMIC_ACQUIRE);
    if(!gen || cpu>=TMC_TLB_CPUS || !(__atomic_load_n(&m->targets,__ATOMIC_RELAXED)&(1u<<cpu)) ||
       __atomic_load_n(&m->ack[cpu],__ATOMIC_RELAXED)==gen)return;
    invalidate(__atomic_load_n(&m->address,__ATOMIC_RELAXED),context);
    __atomic_store_n(&m->ack[cpu],gen,__ATOMIC_RELEASE);
}
static inline int tlb_mailbox_done(TmcTLBMailbox *m)
{
    u32 gen=__atomic_load_n(&m->generation,__ATOMIC_ACQUIRE);
    if(!gen)return 1;
    for(unsigned c=0;c<TMC_TLB_CPUS;c++)
        if((__atomic_load_n(&m->targets,__ATOMIC_RELAXED)&(1u<<c)) && __atomic_load_n(&m->ack[c],__ATOMIC_ACQUIRE)!=gen)return 0;
    return 1;
}
static inline void tlb_mailbox_finish(TmcTLBMailbox *m)
{
    __atomic_store_n(&m->generation,0,__ATOMIC_RELEASE);
}
