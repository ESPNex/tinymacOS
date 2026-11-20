#pragma once
#include "base.h"
// Per-CPU control block. In 64-bit mode the kernel never swaps GS: the base
// stays on this block from cpu_init/AP entry through ring-3 execution (user
// code must never use GS; user FS is the task-owned TLS thread pointer).
struct PerCpu;
typedef struct PerCpu {
    struct PerCpu *self; // gs:0
    u64 kstack_top;      // gs:8  — SYSCALL kernel stack
    u64 user_rsp;        // gs:16 — saved ring-3 RSP
    int cpu;             // gs:24
    int cur_slot;        // gs:28 — current task index
    u64 lapic_period;    // gs:32 — LAPIC timer counts per 10 ms
    u64 core_ticks;      // gs:40
    u64 core_busy;       // gs:48
    void *tss;           // gs:56 — this CPU's TSS
} PerCpu;
static inline PerCpu *pc(void)
{
    PerCpu *p;
    asm volatile("mov %%gs:0,%0" : "=r"(p));
    return p;
}
static inline int cpu_id_now(void)
{
    return pc()->cpu;
}
#define cur (pc()->cur_slot)
