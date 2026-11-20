#include "../kernel/k.h"
u32 npci, apic_enabled;
u8 apic_cpus[8];
int napic_cpus;
static volatile u32 *lap, *ioa;
static void add_cpu(u32 id)
{
    if (id > 255 || napic_cpus >= 8)
        return;
    for (int i = 0; i < napic_cpus; i++)
        if (apic_cpus[i] == id)
            return;
    apic_cpus[napic_cpus++] = (u8)id;
}
u32 bsp_apic_id(void)
{
    return lap && apic_enabled ? (lap[0x20 / 4] >> 24) : 0;
}
volatile u32 *lapic_mmio(void)
{
    return (volatile u32 *)lap;
}
char cpuname[56];
static int irqpins[16], irqflag[16], io_base;
static u16 pmctrl;
static int slp = -1;
static u32 get32(void *p) {
    u32 x;
    memcpy(&x, p, 4);
    return x;
}
static u64 get64(void *p) {
    u64 x;
    memcpy(&x, p, 8);
    return x;
}
static int check(u8 *p, u32 n) {
    u8 s = 0;
    while (n--)
        s += *p++;
    return !s;
}
static int tableok(u64 a) {
    if (!a || a > 0xfffff000ull)
        return 0;
    u32 n = get32((void *)(a + 4));
    return n >= 36 && n <= 1048576 && a + n <= 0x100000000ull && check((void *)a, n);
}
static void acpi(void) {
    u8 *r = (u8 *)boot.rsdp;
    if (!r || (u64)r > 0xfffff000ull || memcmp(r, "RSD PTR ", 8) || !check(r, 20))
        return;
    int xs = r[15] >= 2 && get32(r + 20) >= 36 && check(r, 36);
    u64 a = xs ? get64(r + 24) : get32(r + 16);
    if (!tableok(a))
        return;
    u8 *t = (u8 *)a;
    u32 n = get32(t + 4);
    for (u32 off = 36; off + (xs ? 8 : 4) <= n; off += xs ? 8 : 4) {
        u64 p = xs ? get64(t + off) : get32(t + off);
        if (!tableok(p))
            continue;
        u8 *v = (u8 *)p;
        u32 z = get32(v + 4);
        if (!memcmp(v, "APIC", 4) && z >= 44) {
            lap = (void *)(u64)get32(v + 36);
            for (u32 j = 44; j + 2 <= z && v[j + 1] >= 2 && j + v[j + 1] <= z; j += v[j + 1]) {
                u8 *q = v + j;
                if (q[0] == 1 && q[1] >= 12 && get32(q + 8) == 0) {
                    ioa = (void *)(u64)get32(q + 4);
                    io_base = get32(q + 8);
                }
                if (q[0] == 2 && q[1] >= 10 && q[2] == 0 && q[3] < 16) {
                    irqpins[q[3]] = get32(q + 4);
                    irqflag[q[3]] = q[8] | (q[9] << 8);
                }
                if (q[0] == 0 && q[1] >= 8 && (get32(q + 4) & 1))
                    add_cpu(q[3]);
                if (q[0] == 9 && q[1] >= 12 && ((q[4] | (q[5] << 8)) & 1))
                    add_cpu((u32)q[2] | ((u32)q[3] << 8));
            }
        }
        if (!memcmp(v, "FACP", 4) && z >= 90) {
            u32 port = get32(v + 64);
            if (port && port < 65535)
                pmctrl = port;
            u64 ds = z >= 148 && get64(v + 140) ? get64(v + 140) : get32(v + 40);
            if (tableok(ds)) {
                u8 *d = (u8 *)ds;
                u32 dn = get32(d + 4);
                for (u32 j = 36; j + 12 < dn; j++)
                    if (!memcmp(d + j, "_S5_", 4) && d[j + 4] == 0x12) {
                        u8 *q = d + j + 5;
                        int len = 1 + (q[0] >> 6);
                        q += len + 1;
                        if (q[0] == 0x0a && q[1] < 8)
                            slp = q[1];
                        else if (q[0] == 0 || q[0] == 1)
                            slp = q[0];
                        break;
                    }
            }
        }
    }
    logstr("[acpi] validated root; S5=");
    lognum(slp >= 0);
    logstr("\n");
}
static u32 ioar(int r) {
    ioa[0] = r;
    return ioa[4];
}
static void ioaw(int r, u32 v) {
    ioa[0] = r;
    ioa[4] = v;
}
static void route(int irq) {
    int p = irqpins[irq] - io_base;
    int fl = irqflag[irq];
    ioaw(0x11 + p * 2, lap[0x20 / 4] & 0xff000000);
    ioaw(0x10 + p * 2, (32 + irq) | ((fl & 3) == 3 ? 1 << 13 : 0) | ((fl & 12) == 12 ? 1 << 15 : 0));
}
void platform_init(void) {
    u32 r[4];
    cpuid(0x80000000, r);
    if (r[0] >= 0x80000004) {
        for (int i = 0; i < 3; i++) {
            cpuid(0x80000002 + i, r);
            memcpy(cpuname + 16 * i, r, 16);
        }
        while (cpuname[0] == ' ')
            memmove(cpuname, cpuname + 1, strlen(cpuname));
    } else {
        cpuid(0, r);
        memcpy(cpuname, r + 1, 4);
        memcpy(cpuname + 4, r + 3, 4);
        memcpy(cpuname + 8, r + 2, 4);
    }
    for (int b = 0; b < 256; b++)
        for (int d = 0; d < 32; d++) {
            u32 a = 0x80000000u | (b << 16) | (d << 11);
            outl(0xcf8, a);
            if ((inl(0xcfc) & 65535) == 65535)
                continue;
            npci++;
            outl(0xcf8, a | 0xc);
            if (inl(0xcfc) & 0x800000)
                for (int f = 1; f < 8; f++) {
                    outl(0xcf8, a | (f << 8));
                    if ((inl(0xcfc) & 65535) != 65535)
                        npci++;
                }
        }
    for (int i = 0; i < 16; i++)
        irqpins[i] = i;
    acpi();
}
void irq_init(void) {
    outb(0x20, 0x11);
    waitio();
    outb(0xa0, 0x11);
    waitio();
    outb(0x21, 32);
    waitio();
    outb(0xa1, 40);
    waitio();
    outb(0x21, 4);
    waitio();
    outb(0xa1, 2);
    waitio();
    outb(0x21, 1);
    waitio();
    outb(0xa1, 1);
    waitio();
    outb(0x21, 255);
    outb(0xa1, 255);
    u32 r[4];
    cpuid(1, r);
    if ((r[3] & (1 << 9)) && lap && ioa) {
        wrmsr(0x1b, (rdmsr(0x1b) | 0x800) & ~0x400ull);
        lap[0xf0 / 4] = 0x1ff;
        lap[0x80 / 4] = 0;
        lap[0x350 / 4] = lap[0x360 / 4] = 1 << 16;
        int max = (ioar(1) >> 16) & 255;
        for (int i = 0; i <= max; i++)
            ioaw(0x10 + 2 * i, 1 << 16);
        route(0);

        apic_enabled = 1;
    } else {
        outb(0x21, 0xfe);
        outb(0xa1, 0xff);
    }
    outb(0x43, 0x36);
    outb(0x40, 11932 & 255);
    outb(0x40, 11932 >> 8);
    device_add(apic_enabled ? "Local APIC + I/O APIC" : "8259 PIC", "interrupt", 0);
    device_add("8254 PIT / 100 Hz", "timer", 0);
    logstr(apic_enabled ? "[irq] IOAPIC + LAPIC" : "[irq] 8259 PIC");
    logstr("; 100 Hz PIT, xHCI event polling\n");
}
void eoi(int irq) {
    if (apic_enabled)
        lap[0xb0 / 4] = 0;
    else {
        if (irq >= 8)
            outb(0xa0, 32);
        outb(0x20, 32);
    }
}
static u8 cmos(int i) {
    outb(0x70, i);
    return inb(0x71);
}
static int dec(u8 x, int bin) {
    return bin ? x : (x & 15) + (x >> 4) * 10;
}
void rtc(SysInfo *s) {
    int mode = cmos(0xb), bin = mode & 4;
    for (int i = 0; i < 1000 && (cmos(0xa) & 0x80); i++)
        ;
    int h = cmos(4);
    s->hour = dec(h & 127, bin);
    if (!(mode & 2))
        s->hour = s->hour % 12 + (h & 128 ? 12 : 0);
    s->minute = dec(cmos(2), bin);
    s->second = dec(cmos(0), bin);
    s->day = dec(cmos(7), bin);
    s->month = dec(cmos(8), bin);
    s->year = 2000 + dec(cmos(9), bin);
}
int power(int reboot) {
    if(install_writing()){logstr("[power] installation writing; shutdown deferred\n");return -1;}

    if (reboot) {
        logstr("[power] reset via Q35 reset control\n");
        outb(0xcf9,2);outb(0xcf9,6);
        return 0;
    }
    if (!pmctrl || slp < 0)
        return -1;
    logstr("[power] ACPI S5\n");
    outw(pmctrl, (inw(pmctrl) & ~(7 << 10)) | (slp << 10) | (1 << 13));
    return 0;
}

static SysInfo sampled_clock;
void clock_sample(void) {
    rtc(&sampled_clock);
}
void clock_info(SysInfo *s) {
    s->hour = sampled_clock.hour;
    s->minute = sampled_clock.minute;
    s->second = sampled_clock.second;
    s->day = sampled_clock.day;
    s->month = sampled_clock.month;
    s->year = sampled_clock.year;
}
