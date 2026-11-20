#pragma once
#include "base.h"
// Interrupt-safe ticketless spinlock: IRQs are disabled while held so a local
// re-entry cannot deadlock, and `pause` backs off on the other CPUs.
/* Poll pending TLB work while IRQ-off; never acquire a lock in this hook. */
void tlb_shoot_poll(void);
typedef struct { volatile u32 v; } Spin;
typedef struct { volatile u32 v; int owner, depth; } RecSpin;
static inline u32 irq_save(void)
{
    u64 f;
    asm volatile("pushfq; popq %0; cli" : "=r"(f) :: "memory");
    return (u32)f;
}
static inline void irq_restore(u32 f)
{
    if (f & 0x200)
        asm volatile("sti" ::: "memory");
}
#include "percpu.h"
static inline void spin_lock(Spin *s, u32 *flags)
{
    *flags = irq_save();
    while (__sync_lock_test_and_set(&s->v, 1))
        while (s->v) {
            tlb_shoot_poll();
            asm volatile("pause");
        }
}
static inline void spin_unlock(Spin *s, u32 flags)
{
    __sync_lock_release(&s->v);
    irq_restore(flags);
}
// Recursive lock for nested filesystem/maintenance entry points. Ownership is
// the current CPU (IRQs off ⇒ only this context can re-enter).
static inline void rec_lock(RecSpin *s, u32 *flags)
{
    int me = cpu_id_now();
    *flags = irq_save();
    if (s->owner == me + 1) {
        s->depth++;
        irq_restore(*flags);
        *flags = 0;
        return;
    }
    while (__sync_lock_test_and_set(&s->v, 1))
        while (s->v) {
            tlb_shoot_poll();
            asm volatile("pause");
        }
    s->owner = me + 1;
    s->depth = 1;
}
static inline void rec_unlock(RecSpin *s, u32 flags)
{
    if (!flags && s->depth > 1) {
        s->depth--;
        return;
    }
    s->depth = 0;
    s->owner = 0;
    __sync_lock_release(&s->v);
    irq_restore(flags);
}
