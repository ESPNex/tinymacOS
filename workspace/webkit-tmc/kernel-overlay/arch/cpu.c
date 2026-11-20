#include "../kernel/k.h"
// Per-CPU GDT/TSS/IDT: every logical CPU carries its own descriptors and
// double-fault stack; the syscall MSRs and GS base are per-CPU as well.
static u64 gdts[8][7] ALIGNED(16);
typedef struct PACK TSS {
    u32 r;
    u64 rsp[3], r1, ist[7], r2;
    u16 r3, io;
} TSS;
static TSS tsses[8];
static u8 dfstacks[8][8192] ALIGNED(16);
typedef struct PACK {
    u16 lo, cs;
    u8 ist, type;
    u16 mid;
    u32 hi, r;
} Gate;
static Gate idts[8][256] ALIGNED(16);
struct PACK DT {
    u16 len;
    u64 base;
};
extern void *vectors[];
extern void sys_entry(void), load_gdt(struct DT *);
PerCpu cpus[8];
int ncpu_online = 1;
void cpu_tables(int c)
{
    u64 (*gdt)[7] = &gdts[c];
    TSS *tss = tsses + c;
    gdt[0][0] = 0;
    gdt[0][1] = 0x00af9a000000ffff;
    gdt[0][2] = 0x00cf92000000ffff;
    gdt[0][3] = 0x00cff2000000ffff;
    gdt[0][4] = 0x00affa000000ffff;
    u64 a = (u64)tss;
    memset(tss, 0, sizeof *tss);
    tss->io = sizeof(TSS);
    tss->ist[0] = (u64)(dfstacks[c] + sizeof dfstacks[c]);
    gdt[0][5] = (sizeof(TSS) - 1) | ((a & 0xffffff) << 16) | (0x89ull << 40) | (((a >> 24) & 255) << 56);
    gdt[0][6] = a >> 32;
    struct DT gd = {sizeof gdts[0] - 1, (u64)gdt};
    load_gdt(&gd);
    Gate (*idt)[256] = &idts[c];
    for (int i = 0; i < 256; i++) {
        a = (u64)vectors[i];
        idt[0][i] = (Gate) {
            a, 8, i == 8 ? 1 : 0, 0x8e, a >> 16, a >> 32, 0
        };
    }
    struct DT it = {sizeof idts[0] - 1, (u64)idt};
    asm volatile("lidt %0"::"m"(it));
    wrmsr(0xc0000081, (8ull << 32) | (0x13ull << 48));
    wrmsr(0xc0000082, (u64)sys_entry);
    wrmsr(0xc0000084, 0x44700);
    wrmsr(0xc0000080, rdmsr(0xc0000080) | 1);
    /* User FS is a task-owned thread pointer; do not inherit firmware/AP state.
     * FSGSBASE must remain disabled: users must never rewrite our per-CPU GS. */
    u64 cr4;asm volatile("mov %%cr4,%0":"=r"(cr4));
    cr4&=~(1ull<<16);asm volatile("mov %0,%%cr4"::"r"(cr4):"memory");
    wrmsr(0xc0000100,0);
    PerCpu *P = cpus + c;
    P->self = P;
    P->cpu = c;
    P->tss = tss;
    wrmsr(0xc0000101, (u64)P);
    wrmsr(0xc0000102, (u64)P);
}
void cpu_init(void)
{
    cpu_tables(0);
    logstr("[cpu] GDT IDT TSS installed; LMA=");
    lognum((rdmsr(0xc0000080) >> 10) & 1);
    logstr(" SYSCALL enabled\n");
}
void arch_rsp0(u64 p)
{
    PerCpu *P = pc();
    P->kstack_top = p;
    ((TSS *)P->tss)->rsp[0] = p;
}
Frame *dispatch(Frame *f)
{
    if (f->vec == 128)
        return syscall_do(f);
    if (f->vec == 129 && !(f->cs & 3)) {
        u32 fl;
        spin_lock(&sched_lock, &fl);
        task[cur].state = 2;
        task[cur].wake = ticks + f->rdi;
        task[cur].at = -1;
        Frame *r = schedule(f);
        spin_unlock(&sched_lock, fl);
        return r;
    }
    if (f->vec == 64) {
        // Local APIC timer: per-CPU preemption, accounting and scheduling.
        PerCpu *P = pc();
        if (!P->core_ticks) { logstr("[vec64] first on cpu="); lognum(P->cpu); logstr("\n"); }
        P->core_ticks++;
        if (task[cur].pin < 0) {
            P->core_busy++;
            __sync_fetch_and_add(&busy_ticks, 1);
            current()->cpu_ticks++;
        }
        eoi(0);
        u32 fl;
        spin_lock(&sched_lock, &fl);
        Frame *r = schedule(f);
        spin_unlock(&sched_lock, fl);
        return r;
    }
    if (f->vec == 0x42) {
        tlb_shoot_isr();
        eoi(0);
        return f;
    }
    if (f->vec == 32) {
        // PIT on CPU0: timekeeping and USB event drain only; each CPU
        // preempts on its own LAPIC timer (vector 64).
        ticks++;
        input_irq();
        net_tick();
        eoi(0);
        if (!apic_enabled) {
            u32 fl;
            spin_lock(&sched_lock, &fl);
            Frame *r = schedule(f);
            spin_unlock(&sched_lock, fl);
            return r;
        }
        return f;
    }
    if (f->vec == 255)
        return f;
    if (f->vec >= 32 && f->vec < 48) {
        eoi(f->vec - 32);
        return f;
    }
    u64 cr2;
    asm volatile("mov %%cr2,%0":"=r"(cr2));
    logstr("[exception] vector=");
    lognum(f->vec);
    logstr(" error=");
    loghex(f->err);
    logstr(" rip=");
    loghex(f->rip);
    logstr(" cr2=");
    loghex(cr2);
    logstr("\n");
    if ((f->cs & 3) == 3) {
        logstr("[process] isolated fault; terminating pid=");
        lognum(current()->pid);
        logstr("\n");
        u32 fl;
        spin_lock(&sched_lock, &fl);
        terminate(task[cur].pi);
        Frame *r = schedule(f);
        spin_unlock(&sched_lock, fl);
        return r;
    }
    panic("CPU exception (vector / RIP)", f->vec, f->rip);
}
// Preserve x87/SSE2 state as well as integer registers. AVX is not exposed (OSXSAVE=0).
static u8 cleanfx[512] ALIGNED(16);
void fx_setup(void)
{
    u64 c;
    asm volatile("mov %%cr0,%0":"=r"(c));
    c = (c & ~12ull) | 2;
    asm volatile("mov %0,%%cr0"::"r"(c) : "memory");
    asm volatile("mov %%cr4,%0":"=r"(c));
    c = (c | 0x600) & ~(1ull << 18);
    asm volatile("mov %0,%%cr4"::"r"(c) : "memory");
    *(u16 *)cleanfx = 0x37f;
    *(u32 *)(cleanfx + 24) = 0x1f80;
    asm volatile("fxrstor64 %0"::"m"(cleanfx));
}
void fx_new(void *p)
{
    memcpy(p, cleanfx, 512);
}
