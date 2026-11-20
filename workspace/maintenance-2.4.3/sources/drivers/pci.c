#include "../kernel/k.h"
u32 pci_reg(u32 dev, int off)
{
    outl(0xcf8, dev | (off & 252));
    return inl(0xcfc);
}
u32 pci_find_after(u32 want,u32 after)
{
    for (int b = 0; b < 256; b++)
        for (int d = 0; d < 32; d++) {
            u32 a = 0x80000000u | (b << 16) | (d << 11);
            if ((pci_reg(a, 0) & 65535) == 65535)
                continue;
            int n = pci_reg(a, 12) & 0x800000 ? 8 : 1;
            for (int f = 0; f < n; f++) {
                u32 t = a | (f << 8);
                if (t>after && (pci_reg(t, 8) >> 8) == want)
                    return t;
            }
        }
    return 0;
}
u32 pci_find(u32 want){return pci_find_after(want,0);}
void *pci_mmio(u32 dev, int bar)
{
    static int next;
    u32 lo = pci_reg(dev, 16 + 4 * bar);
    if ((lo & 1) || !lo || lo == 0xffffffff)
        return 0;
    u64 p = lo & ~15u;
    if ((lo & 6) == 4)
        p |= (u64)pci_reg(dev, 20 + 4 * bar) << 32;
    u32 cmd = pci_reg(dev, 4);
    outl(0xcf8, dev | 4);
    outw(0xcfc, cmd | 0x406);
    u64 *pdpt = (void*)(kroot[0] & ADDR);
    if (!pdpt[64]) {
        u64 a = page_get();
        if (!a)
            return 0;
        pdpt[64] = a | 3;
    }
    u64 *pd = (void*)(pdpt[64] & ADDR);
    int i = next;
    next += 2;
    if (next > 512)
        return 0;
    u64 v = (64ull << 30) + ((u64)i << 21) + (p & 0x1fffff);
    pd[i] = (p & ~0x1fffffull) | NX | 0x9b;
    pd[i + 1] = ((p & ~0x1fffffull) + 0x200000) | NX | 0x9b;
    return (void*)v;
}
