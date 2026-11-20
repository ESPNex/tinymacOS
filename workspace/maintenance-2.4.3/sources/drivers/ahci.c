#include "../kernel/k.h"
// Register FIS, one command slot + one physical region per request, 512-byte SATA drives.
#define ND 8
#define MAXS 256
struct PACK Cmd {
    u16 flags, n;
    u32 count;
    u64 table;
    u32 reserved[4];
};
struct PACK Table {
    u8 fis[64], acmd[16], r[48];
    u64 buf;
    u32 res, size;
    u8 pad[112];
};
static struct Cmd list[ND][32] ALIGNED(1024);
static u8 rx[ND][256] ALIGNED(256);
static struct Table table[ND] ALIGNED(128);
static u8 bounce[MAXS * 512] ALIGNED(4096);
static struct {
    volatile u32 *p;
    u32 sectors;
    char model[41];
    int dead;
} ds[ND];
static int nd;
static int waitclr(volatile u32 *p, u32 mask)
{
    for (unsigned i = 0; i < 4000000; i++) {
        if (!(*p & mask))
            return 1;
        asm volatile("pause");
    }
    return 0;
}
static int issue(int d, int op, u32 lba, u32 n, void *b, int wr)
{
    volatile u32 *p = ds[d].p;
    if (ds[d].dead || !waitclr(p + 8, 0x88))
        return -1;
    struct Cmd *h = list[d];
    struct Table *t = table + d;
    memset(t, 0, sizeof * t);
    memset(h, 0, sizeof * h);
    h->flags = 5 | (wr ? 64 : 0);
    h->n = n ? 1 : 0;
    h->table = (u64)t;
    u8 *f = t->fis;
    f[0] = 0x27;
    f[1] = 0x80;
    f[2] = op;
    f[4] = lba;
    f[5] = lba >> 8;
    f[6] = lba >> 16;
    f[7] = 0x40;
    f[8] = lba >> 24;
    f[12] = n;
    f[13] = n >> 8;
    if (n) {
        t->buf = (u64)bounce;
        t->size = n * 512 - 1;
        if (wr)
            memcpy(bounce, b, n * 512);
    }
    p[4] = ~0u;
    p[12] = ~0u;
    asm volatile("mfence":::"memory");
    p[14]=1;
    if (!waitclr(p + 14, 1)) {
        ds[d].dead = 1;
        return -1;
    }
    asm volatile("lfence":::"memory");
    if ((p[4]&(1u<<30))||(p[8]&1))
        return -1;
    if (n&&!wr)
        memcpy(b, bounce, n*512);
    return 0;
}
void ahci_init(void)
{
    u32 dev = pci_find(0x010601);
    volatile u32 *h = dev ? pci_mmio(dev, 5) : 0;
    if (!h) {
        logstr("[ahci] absent\n");
        return;
    }
    if (h[9] & 1) {
        h[10] |= 2;
        if (!waitclr(h + 10, 17))
            return;
    }
    h[1] = (h[1] | 0x80000000u) & ~2u;
    u32 pi = h[3];
    device_add("SATA / AHCI", "storage", 0);
    for (int i = 0; i < 32 && nd < ND; i++)
        if (pi & (1u << i)) {
            volatile u32 *p = h + 64 + 32 * i;
            if ((p[10] & 15) != 3 || p[9] != 0x101)
                continue;
            p[6] &= ~1u;
            if (!waitclr(p + 6, 1 << 15))
                continue;
            p[6] &= ~16u;
            if (!waitclr(p + 6, 1 << 14))
                continue;
            int d = nd;
            ds[d].p = p;
            p[0] = (u64)list[d];
            p[1] = 0;
            p[2] = (u64)rx[d];
            p[3] = 0;
            p[5] = 0;
            p[4] = ~0u;
            p[12] = ~0u;
            p[6] |= 16;
            p[6] |= 1;
            u16 id[256];
            if (issue(d, 0xec, 0, 1, id, 0))
                continue;
            if ((id[106] & 0xd000) == 0x5000 && (id[117] != 256 || id[118]))
                continue;
            u64 z = 0;
            memcpy(&z, id + 100, 8);
            if (!z)
                z = (u32)id[60] | (u32)id[61] << 16;
            if (!z || z >= 0xffffffffu)
                continue;
            ds[d].sectors = z;
            for (int j = 0; j < 20; j++) {
                ds[d].model[j * 2] = id[27 + j] >> 8;
                ds[d].model[j * 2 + 1] = id[27 + j];
            }
            for (int j = 39; j >= 0 && ds[d].model[j] == ' '; j--)
                ds[d].model[j] = 0;
            logstr("[ahci] SATA ");
            lognum(d);
            logstr(" sectors=");
            lognum(z);
            logstr(" ");
            logstr(ds[d].model);
            logstr("\n");
            nd++;
        }
}
int sata_count(void)
{
    return nd;
}
u32 sata_size(int d)
{
    return d >= 0 && d < nd ? ds[d].sectors : 0;
}
const char *sata_model(int d)
{
    return d >= 0 && d < nd ? ds[d].model : "";
}
int sata_io(int d, u32 l, u32 n, void *b, int wr)
{
    if (d < 0 || d >= nd || !n || n > MAXS || l > ds[d].sectors || n > ds[d].sectors - l)
        return -1;
    return issue(d, wr ? 0x35 : 0x25, l, n, b, wr);
}
int sata_flush(int d)
{
    return d >= 0 && d < nd ? issue(d, 0xea, 0, 0, 0, 0) : -1;
}
