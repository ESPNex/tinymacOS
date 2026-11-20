// Symmetric multiprocessing: MADT-based AP bring-up through the physical
// trampoline, per-CPU LAPIC timers and TLB shootdown IPIs.
#include "../kernel/k.h"
#include "aptramp.inc"
extern u8 apic_cpus[];
extern int napic_cpus;
u32 bsp_apic_id(void);
volatile u32 *lapic_mmio(void);
static u64 smp_period;
static Spin shoot_lock;
static volatile u64 shoot_addr;
static volatile u32 shoot_acks;
static u8 ap_boot_stacks[7][8192] ALIGNED(16);
static volatile u32 shoot_gen, shoot_active;
static volatile u32 ack_gen[8];
static volatile u32 online_mask;
static u16 pit_cnt(void)
{
    outb(0x43, 0x00);
    u8 l = inb(0x40), h = inb(0x40);
    return (u16)(l | (h << 8));
}
static u16 pit_elapsed(u16 start)
{
    return (u16)((start - pit_cnt() + 11932) % 11932);
}
static void pit_wait(u16 counts)
{
    // QEMU's PIT in mode 3 presents only one counter parity, so an odd
    // target like 11931 would never be observed exactly: stop at counts-1.
    u16 s = pit_cnt();
    while (pit_elapsed(s) + 1 < counts)
        asm volatile("pause");
}
static void icr(u32 low, u32 dest)
{
    volatile u32 *l = lapic_mmio();
    l[0x310 / 4] = dest << 24;
    l[0x300 / 4] = low;
    while (l[0x300 / 4] & (1u << 12))
        asm volatile("pause");
}
static void lapic_periodic(void)
{
    if (!apic_enabled)
        return;
    volatile u32 *l = lapic_mmio();
    l[0xF0 / 4] = 0x1ff; // APIC software-enable: APs arrive from INIT with SVR=0xFF
    l[0x3e0 / 4] = 3;            // divide by 16, same as calibration (APs reset to /1)
    l[0x320 / 4] = 0x20000 | 64; // periodic, vector 64, unmasked
    l[0x380 / 4] = (u32)smp_period;
}
void lapic_timer_arm(void)
{
    if (!apic_enabled)
        return;
    {
        u32 a, b, c, d;
        asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x15), "c"(0));
        logstr("[cal] cpuid15 a=");
        lognum(a);
        logstr(" b=");
        lognum(b);
        logstr(" c=");
        lognum(c);
        logstr("\n");
        asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x16), "c"(0));
        logstr("[cal] cpuid16 a=");
        lognum(a);
        logstr("\n");
    }
    volatile u32 *l = lapic_mmio();
    // Calibrate the LAPIC timer against the free-running PIT over a 5 ms window.
    l[0x350 / 4] = 0x10000 | 64; // masked while measuring
    l[0x3e0 / 4] = 3;            // divide by 16
    l[0x380 / 4] = 0xffffffff;
    u64 t0, t1;
    asm volatile("rdtsc" : "=a"(t0) : : "edx");
    pit_wait(5966);
    asm volatile("rdtsc" : "=a"(t1) : : "edx");
    u32 left = l[0x390 / 4];
    logstr("[cal] tsc=");
    lognum(t1 - t0);
    logstr(" left=");
    lognum(0xffffffffu - left);
    logstr(" ticks=");
    lognum(ticks);
    logstr("\n");
    u64 counts10 = (u64)(0xffffffffu - left) * 2;
    if (counts10 < 10000 || counts10 > 400000000ull)
        counts10 = 11932; // sanity fallback: about one PIT period at 100 Hz
    smp_period = counts10;
    logstr("[smp] period=");
    lognum(counts10);
    logstr("\n");
    cpus[0].lapic_period = counts10;
    lapic_periodic();
}
int smp_count(void)
{
    if (!apic_enabled || napic_cpus < 1)
        return 1;
    return napic_cpus > 8 ? 8 : napic_cpus;
}
void smp_start(void)
{
    logstr("[smp] arm\n");
    lapic_timer_arm();
    online_mask |= 1;
    if (!apic_enabled || napic_cpus < 2)
        return;
    logstr("[smp] launched\n");
    memcpy((void *)TRAMP_PHYS, tramp_img, sizeof tramp_img);
    u32 bsp = bsp_apic_id();
    int slot = 1;
    for (int i = 0; i < napic_cpus && slot < 8; i++) {
        u32 id = apic_cpus[i];
        if (id == bsp)
            continue;
        u32 *slots = (u32 *)TRAMP_SLOTS;
        slots[0] = (u32)(u64)(ap_boot_stacks[slot - 1] + 8192);
        slots[1] = (u32)(u64)kroot;
        slots[2] = (u32)(u64)ap_main_entry;
        slots[3] = (u32)slot;
        slots[4] = 0xa9b8c7d6;
        slots[5] = 0;
        icr(0x00004500, id); // INIT, assert, edge
        pit_wait(11931);      // ~10 ms
        icr(0x00004608, id);  // STARTUP, vector 8
        pit_wait(239);        // ~200 us
        icr(0x00004608, id);
        int started = 0;
        for (u32 t = 0; t < 30000000u; t++) {
            if (slots[5] == 1) {
                started = 1;
                break;
            }
            asm volatile("pause");
        }
        if (started) {
            logstr("[smp] ap up id=");
            lognum(id);
            logstr("\n");
        } else {
            logstr("[smp] ap timeout id=");
            lognum(id);
            logstr("\n");
        }
        slot++;
    }
}
// AP entry: trampoline already enabled long mode with the kernel identity map.
void ap_main_entry(int cpu)
{
    PerCpu *P = cpus + cpu;
    asm volatile("wrmsr":: "c"(0xc0000101), "a"((u32)(u64)P), "d"((u32)((u64)P >> 32)));
    asm volatile("wrmsr":: "c"(0xc0000102), "a"((u32)(u64)P), "d"((u32)((u64)P >> 32)));
    u64 c;
    asm volatile("mov %%cr0,%0" : "=r"(c));
    c |= 0x10001 | 0x20; // WP | PE | NE
    asm volatile("mov %0,%%cr0" :: "r"(c) : "memory");
    asm volatile("mov %%cr4,%0" : "=r"(c));
    c |= 0x600;
    asm volatile("mov %0,%%cr4" :: "r"(c) : "memory");
    wrmsr(0xc0000080, rdmsr(0xc0000080) | 0x801); // SCE | NXE
    P->self = P;
    P->cpu = cpu;
    P->lapic_period = smp_period;
    if (apic_enabled)
        wrmsr(0x1b, rdmsr(0x1b) | 0x800); // enable xAPIC BEFORE SVR/LVT setup
    logstr("[ap] tables\n");
    cpu_tables(cpu);
    fx_setup();
    if (apic_enabled)
        lapic_periodic();
#ifdef TMC_DEBUG
    volatile u32 *la = lapic_mmio();
    u64 flg;
    asm volatile("pushfq; popq %0" : "=r"(flg));
    logstr("[ap] online cpu=");
    lognum(cpu);
    logstr(" lvt=");
    loghex(la[0x320 / 4]);
    logstr(" cnt=");
    lognum(la[0x390 / 4]);
    logstr(" rflags=");
    loghex(flg);
    logstr(" tpr=");
    loghex(la[0x80 / 4]);
    logstr(" svr=");
    loghex(la[0xF0 / 4]);
    logstr(" isr_hi=");
    loghex(la[0x110 / 4]);
    logstr("\n");
    u32 c1 = la[0x390 / 4];
    for (volatile int w = 0; w < 4000000; w++)
        ;
    logstr("[ap] cnt2=");
    lognum(la[0x390 / 4]);
    logstr(" delta=");
    lognum(c1 > la[0x390 / 4] ? c1 - la[0x390 / 4] : 0);
    logstr("\n");
#endif
    __sync_fetch_and_or(&online_mask, 1u << cpu);
    __sync_add_and_fetch(&ncpu_online, 1);
    cur = idle_slots[cpu];
#ifdef TMC_DEBUG
    logstr("[ap] enter cur=");
    lognum(cur);
    logstr(" rip=");
    loghex(task[cur].f->rip);
    logstr(" flg=");
    loghex(task[cur].f->flags);
    logstr("\n");
#endif
    arch_rsp0((u64)(task[cur].stack + sizeof task[cur].stack));
    enter_frame(task[cur].f);
}
void tlb_shoot_isr(void)
{
    if (!shoot_active)
        return;
    u64 a = shoot_addr;
    if (a)
        asm volatile("invlpg (%0)" :: "r"(a) : "memory");
    u64 cr;
    asm volatile("mov %%cr3,%0" : "=r"(cr));
    asm volatile("mov %0,%%cr3" :: "r"(cr) : "memory");
    ack_gen[pc()->cpu] = shoot_gen;
}
void tlb_shoot(u64 a)
{
    if (ncpu_online <= 1 || !apic_enabled)
        return;
    if (!a)
        a = 0x40000000; // any canonical address: the handler also reloads CR3
    while (__sync_lock_test_and_set(&shoot_lock.v, 1))
        asm volatile("pause");
    u32 gen = ++shoot_gen;
    u32 mask = online_mask;
    int me = pc()->cpu;
    shoot_addr = a;
    shoot_active = 1;
    ack_gen[me] = gen; // this CPU already flushed locally inside vm_unmap
    lapic_mmio()[0x300 / 4] = (3u << 18) | 0x42;
    for (int c = 0; c < 8; c++)
        if ((mask >> c) & 1)
            while (ack_gen[c] != gen)
                asm volatile("pause");
    shoot_active = 0;
    shoot_addr = 0;
    __sync_lock_release(&shoot_lock.v);
}
