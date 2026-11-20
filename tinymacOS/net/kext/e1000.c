/* Intel 82540EM (QEMU e1000) driver as a kernel extension.
 * Polling RX/TX from kext_poll (the PIT tick serializes us under net_lock);
 * the real MAC address is read from the RAL/RAH registers the EEPROM seeded. */
#include "kext.h"

#define N 32                        /* ring entries (power of two) */

static struct kext_api *A;
static volatile u32 *mm;
static void *ring_tx, *ring_rx;
static u8 *buf_tx[N], *buf_rx[N];
static u32 rxcur, tdt;
static int ifindex = -1;
static struct netdev_ops ops;

static u32 rr(int reg) { return mm[reg >> 2]; }
static void wr(int reg, u32 v) { mm[reg >> 2] = v; }

/* QEMU/Intel legacy TX descriptor: len@8(16) cso@10 cmd@11 status@12 css@13 sp@14 */
struct txdesc { u64 addr; u16 length; u8 cso, cmd, status, css; u16 special; };
struct rxdesc { u64 addr; u16 len, csum; u8 status, errors; u16 special; };

void kext_poll(void);

static void tx(void *ctx, const u8 *frame, int len)
{
    (void)ctx;
    if (!mm || len < 14 || len > 1518) return;
    u32 hw = rr(0x03810) & (N - 1); /* TDH: hardware consumed up to here */
    u32 next = (tdt + 1) & (N - 1);
    if (next == hw) return;         /* ring full: drop */
    struct txdesc *d = (struct txdesc *)ring_tx + tdt;
    memcpy(buf_tx[tdt], frame, len);
    d->addr = (u64)(unsigned long)buf_tx[tdt];
    d->length = (u16)len;
    d->cso = 0;
    d->cmd = 0x01 | 0x02 | 0x08;    /* EOP | IFCS | RS (cmd = byte 11) */
    d->status = 0;
    d->css = 0;
    d->special = 0;
    tdt = next;
    wr(0x03818, tdt);               /* TDT */
    wr(0x00400, (1u << 1) | (1u << 3) | (0x10u << 4) | (0x40u << 12)); /* TCTL kick: EN(bit1)|PSP|CT|COLD */
}

/* QEMU/Intel semantics: hardware writes the descriptor at RDH then advances
 * RDH; free buffers are [RDH, RDT). We keep a software cursor rxcur (next
 * descriptor to process) and set RDT = rxcur-1, which grants every slot
 * except the pending ones in [rxcur, RDH) — never equal while buffers exist. */
void kext_poll(void)
{
    if (!mm || ifindex < 0) return;
    struct rxdesc *rxr = (struct rxdesc *)ring_rx;
    u32 hw = rr(0x02810) & (N - 1);
    int guard = 0;
    while (rxcur != hw && guard++ < N) {
        u32 i = rxcur;
        if (!(rxr[i].status & 1)) break;
        u32 len = rxr[i].len;
        rxr[i].status = 0;
        rxr[i].len = 0;
        rxcur = (i + 1) & (N - 1);
        wr(0x02818, (rxcur + N - 1) & (N - 1));   /* RDT */
        if (len >= 14 && len <= 1518)
            A->net_rx(ifindex, buf_rx[i], (int)len);
    }
}

void kext_start(struct kext_api *api)
{
    A = api;
    u32 dev = api->pci_find(0x020000);
    if (!dev) {
        api->log("[e1000] nessun adapter\n");
        return;
    }
    mm = api->pci_mmio(dev, 0);
    if (!mm) {
        api->log("[e1000] BAR0 non mappabile\n");
        return;
    }
    /* interrupt mask: we poll from the PIT tick */
    wr(0x00d8, 0xffffffff);
    /* reset */
    wr(0x0000, rr(0x0000) | 0x04000000);
    for (int i = 0; i < 100000 && (rr(0x0000) & 0x04000000); i++) api->udelay(1);
    wr(0x00d8, 0xffffffff);

    /* MAC from RAL/RAH (EEPROM-seeded by QEMU) */
    u8 mac[6];
    u32 ral = rr(0x5400), rah = rr(0x5404);
    mac[0] = (u8)ral;
    mac[1] = (u8)(ral >> 8);
    mac[2] = (u8)(ral >> 16);
    mac[3] = (u8)(ral >> 24);
    mac[4] = (u8)rah;
    mac[5] = (u8)(rah >> 8);
    if (!(rah & 0x80000000u)) {     /* no valid address: build one locally */
        mac[0] = 0x02;
        mac[1] = 0x54;
        mac[2] = 0x65;
        mac[3] = 0xe1;
        mac[4] = 0x00;
        mac[5] = 0x01;
    }

    ring_tx = api->alloc(sizeof(struct txdesc) * N + 16);
    ring_rx = api->alloc(sizeof(struct rxdesc) * N + 16);
    if (!ring_tx || !ring_rx) {
        api->log("[e1000: memoria]\n");
        return;
    }
    memset(ring_tx, 0, sizeof(struct txdesc) * N + 16);
    memset(ring_rx, 0, sizeof(struct rxdesc) * N + 16);
    for (int i = 0; i < N; i++) {
        buf_tx[i] = api->alloc(1536);
        buf_rx[i] = api->alloc(2048);
        ((struct rxdesc *)ring_rx)[i].addr = (u64)(unsigned long)buf_rx[i];
    }
    rxcur = 0;
    tdt = 0;
    wr(0x02818, N - 1);             /* RDT: grant all but the sentinel */

    /* TX ring */
    wr(0x03800, (u32)(u64)(unsigned long)ring_tx);
    wr(0x03804, 0);
    wr(0x03808, sizeof(struct txdesc) * N);
    wr(0x03810, 0);
    wr(0x03818, 0);
    /* RX ring */
    wr(0x02800, (u32)(u64)(unsigned long)ring_rx);
    wr(0x02804, 0);
    wr(0x02808, sizeof(struct rxdesc) * N);
    wr(0x02810, 0);

    /* control registers */
    wr(0x0000, 0x61);          /* CTRL: FD|ASDE|SLU (no LRST/BEM/RST) */
    wr(0x0400, (1u << 1) | (1u << 3) | (0x10u << 4) | (0x40u << 12));      /* TCTL: EN(bit1)|PSP|CT(0x10<<4)|COLD(0x40<<12) */
    wr(0x03828, (1 << 24) | (16 << 8));      /* TXDCTL.GRAN + WTHRESH */
    wr(0x0100, (1 << 1) | (1 << 15) | (1 << 26));  /* RCTL EN|BAM|SECRC, 2048B */
    wr(0x02828, (1 << 24) | (16 << 8));      /* RXDCTL.GRAN */

    ops.tx = tx;
    ops.ctx = 0;
    ifindex = api->net_register("en", mac, &ops);
    if (ifindex >= 0) {
        int link = (rr(0x0008) & 4) ? 1 : 1;   /* QEMU user-net: link up */
        api->net_set_link(ifindex, link);
        api->log("[e1000] en0 collegato, MAC ");
        /* log mac hex */
        static const char *hx = "0123456789abcdef";
        char line[24];
        for (int i = 0; i < 6; i++) {
            line[i * 3] = hx[mac[i] >> 4];
            line[i * 3 + 1] = hx[mac[i] & 15];
            line[i * 3 + 2] = i == 5 ? '\n' : ':';
        }
        line[18] = 0;
        api->log(line);
    }
}

void kext_stop(void)
{
    if (!mm) return;
    wr(0x0100, 0);                  /* RCTL off */
    wr(0x0400, 0);                  /* TCTL off */
    mm = 0;
    ifindex = -1;
}
