/* WiFi driver kext over a SIMULATED 802.11 radio.
 *
 * QEMU has no Wi-Fi hardware for the PC platform, so this kext owns a
 * documented test radio ("radio simulato"): scan returns a fixed air, and
 * the simulated AP LevanteNet (192.168.77.1) answers ARP and ping on the
 * wlan0 netif. The driver API (register/scan/connect/status) is the same
 * one a real 802.11 device would plug into. */
#include "kext.h"

#define AP_IP0 192                  /* 192.168.77.1 (simulated AP) */

static struct kext_api *A;
static struct netdev_ops ops;
static int ifindex = -1;
static int associated;
static char ssid[32];
static u8 ourmac[6];
static const u8 apmac[6] = {0x02, 0x54, 0x65, 0xe1, 0x00, 0x02};

struct apinfo {
    const char *ssid;
    s8 rssi;
    u8 channel, secure;
};

static const struct apinfo air[] = {
    {"LevanteNet", -42, 6, 1},
    {"Vodafone-77A1", -67, 1, 1},
    {"Tim-Home5C", -71, 11, 1},
    {"Caffè-Gratis", -83, 6, 0},
};

static u16 csum16(const u8 *d, int n, u32 acc)
{
    for (int i = 0; i + 1 < n; i += 2) acc += (u32)((d[i] << 8) | d[i + 1]);
    if (n & 1) acc += (u32)(d[n - 1] << 8);
    while (acc >> 16) acc = (acc & 0xffff) + (acc >> 16);
    return (u16)~acc;
}

/* Frame from the stack: answer ARP and ICMP echo as the simulated AP. */
static void tx(void *ctx, const u8 *f, int len)
{
    (void)ctx;
    if (!A || len < 14) return;
    u16 et = (u16)((f[12] << 8) | f[13]);
    u8 out[1514];
    if (et == 0x0806 && len >= 42) {                 /* ARP request? */
        if (f[21] != 1) return;                      /* op = request */
        u32 tip = (u32)((f[38] << 24) | (f[39] << 16) | (f[40] << 8) | f[41]);
        if (tip != ((AP_IP0 << 24) | (168 << 16) | (77 << 8) | 1)) return;
        memset(out, 0, 42);
        memcpy(out, f + 6, 6);                       /* dst = requester */
        memcpy(out + 6, apmac, 6);
        out[12] = 0x08;
        out[13] = 0x06;
        out[14] = 0;
        out[15] = 1;
        out[16] = 0x08;
        out[17] = 0x00;
        out[18] = 6;
        out[19] = 4;
        out[20] = 0;
        out[21] = 2;                                 /* reply */
        memcpy(out + 22, apmac, 6);
        out[28] = AP_IP0;
        out[29] = 168;
        out[30] = 77;
        out[31] = 1;
        memcpy(out + 32, f + 6, 6);                  /* target hw = requester */
        memcpy(out + 38, f + 28, 4);                 /* target ip = requester ip */
        A->net_rx(ifindex, out, 42);
        return;
    }
    if (et == 0x0800 && len >= 42 && associated) {   /* IPv4 */
        u32 dst = (u32)((f[30] << 24) | (f[31] << 16) | (f[32] << 8) | f[33]);
        if (dst != ((AP_IP0 << 24) | (168 << 16) | (77 << 8) | 1)) return;
        if (f[23] == 17) {                           /* UDP: DNS on port 53 */
            int iplen = (int)((f[16] << 8) | f[17]);
            if (iplen < 20 + 8 + 12 || 14 + iplen > len || 14 + iplen > (int)sizeof out)
                return;
            int udp = 34;
            if (f[udp + 2] != 0 || f[udp + 3] != 53) return;   /* dport 53 */
            int dns = udp + 8;
            int off = dns + 12;                      /* first label */
            char name[256];
            int ni = 0, okname = 1;
            while (off < len && f[off]) {
                if ((f[off] & 0xc0) == 0xc0) { okname = 0; break; }
                int lab = f[off];
                if (ni + lab + 2 >= (int)sizeof name) { okname = 0; break; }
                if (ni) name[ni++] = '.';
                memcpy(name + ni, f + off + 1, (size_t)lab);
                ni += lab;
                off += 1 + lab;
            }
            if (!okname || off >= len) return;
            name[ni] = 0;
            off++;                                   /* root label */
            if (off + 4 > len) return;
            u16 qtype = (u16)((f[off] << 8) | f[off + 1]);
            u16 qclass = (u16)((f[off + 2] << 8) | f[off + 3]);
            off += 4;                                /* end of question */
            int zone = eq(name, "levante.test");
            for (int i = 0; !zone && name[i]; i++)
                if (name[i] == '.' && eq(name + i + 1, "levante.test")) zone = 1;
            {
                char lg[80];
                int li = 0;
                const char *pre = "[wifi] dns ";
                while (pre[li]) { lg[li] = pre[li]; li++; }
                for (int k = 0; name[k] && li < 60; k++) lg[li++] = name[k];
                lg[li++] = ' ';
                lg[li++] = zone ? 'Z' : '-';
                lg[li++] = (qtype == 1 && qclass == 1) ? 'A' : 'x';
                lg[li++] = 0;
                A->log(lg);
                A->log("\n");
            }
            if (!zone || qtype != 1 || qclass != 1) return;   /* A IN only */
            A->log("[wifi] dns reply\n");
            int qlen = off - dns;                    /* header + question */
            int dnstot = qlen + 16;                  /* + one A answer */
            int udplen = 8 + dnstot;
            int niplen = 20 + udplen;
            memcpy(out, f + 6, 6);
            memcpy(out + 6, apmac, 6);
            out[12] = 0x08;
            out[13] = 0x00;
            memcpy(out + 14, f + 14, 20);            /* IP header */
            out[16] = (u8)(niplen >> 8);             /* total length: reply is */
            out[17] = (u8)niplen;                    /* longer than the query */
            memcpy(out + 26, f + 30, 4);             /* src <- request dst */
            memcpy(out + 30, f + 26, 4);             /* dst <- request src */
            out[22] = 64;                            /* TTL */
            out[24] = 0;
            out[25] = 0;
            u16 ipc = csum16(out + 14, 20, 0);
            out[24] = (u8)(ipc >> 8);
            out[25] = (u8)ipc;
            out[udp + 0] = 0;                        /* sport 53 */
            out[udp + 1] = 53;
            out[udp + 2] = f[udp + 0];               /* dport <- request sport */
            out[udp + 3] = f[udp + 1];
            out[udp + 4] = (u8)(udplen >> 8);
            out[udp + 5] = (u8)udplen;
            out[udp + 6] = 0;                        /* UDP checksum optional */
            out[udp + 7] = 0;
            memcpy(out + dns, f + dns, (size_t)qlen);
            out[dns + 2] = 0x81;                     /* QR | RD | RA */
            out[dns + 3] = 0x80;
            out[dns + 6] = 0;
            out[dns + 7] = 1;                        /* ANCOUNT = 1 */
            out[dns + 8] = 0;
            out[dns + 9] = 0;
            out[dns + 10] = 0;
            out[dns + 11] = 0;
            int ans = dns + qlen;
            out[ans + 0] = 0xc0;                     /* pointer to QNAME */
            out[ans + 1] = 0x0c;
            out[ans + 2] = 0;
            out[ans + 3] = 1;                        /* A */
            out[ans + 4] = 0;
            out[ans + 5] = 1;                        /* IN */
            out[ans + 6] = 0;
            out[ans + 7] = 0;
            out[ans + 8] = 0;
            out[ans + 9] = 60;                       /* TTL */
            out[ans + 10] = 0;
            out[ans + 11] = 4;                       /* RDLENGTH */
            out[ans + 12] = AP_IP0;                  /* 192.168.77.50 */
            out[ans + 13] = 168;
            out[ans + 14] = 77;
            out[ans + 15] = 50;
            A->net_rx(ifindex, out, 14 + niplen);
            return;
        }
        if (f[23] != 1) return;                      /* ICMP */
        if (f[34] != 8) return;                      /* echo request */
        int iplen = (int)((f[16] << 8) | f[17]);
        if (iplen < 8 || 14 + iplen > (int)sizeof out || 14 + iplen > len) return;
        memcpy(out, f + 6, 6);
        memcpy(out + 6, apmac, 6);
        out[12] = 0x08;
        out[13] = 0x00;
        memcpy(out + 14, f + 14, iplen);             /* copy IP header + body */
        /* out: eth(14) ip(20@14) icmp(@34): type34 code35 cksum36-37 */
        out[34] = 0;                                 /* echo reply */
        out[35] = 0;
        out[36] = 0;
        out[37] = 0;
        memcpy(out + 26, f + 30, 4);                 /* IP src <- request dst */
        memcpy(out + 30, f + 26, 4);                 /* IP dst <- request src */
        u16 ic = csum16(out + 34, iplen - 20, 0);
        out[36] = (u8)(ic >> 8);
        out[37] = (u8)ic;
        out[24] = 0;
        out[25] = 0;
        u16 ipc = csum16(out + 14, 20, 0);
        out[24] = (u8)(ipc >> 8);
        out[25] = (u8)ipc;
        A->net_rx(ifindex, out, 14 + iplen);
    }
}

void kext_start(struct kext_api *api)
{
    A = api;
    ops.tx = tx;
    ops.ctx = 0;
    ourmac[0] = 0x02;
    ourmac[1] = 0x54;
    ourmac[2] = 0x65;
    ourmac[3] = 0xe1;
    ourmac[4] = 0x00;
    ourmac[5] = 0x03;
    ifindex = api->net_register("wl", ourmac, &ops);
    if (ifindex >= 0) {
        api->net_set_link(ifindex, 1);   /* static 192.168.77.2/24, gw .1 */
        api->log("[wifi] radio simulato 802.11 pronto (nessun hardware Wi-Fi in QEMU)\n");
    }
}

void kext_stop(void)
{
    associated = 0;
    ifindex = -1;
}

int kext_wifi_status(void *buf)
{
    WifiStatus *w = (WifiStatus *)buf;
    memset(w, 0, sizeof *w);
    w->state = associated;
    strset(w->ssid, ssid, sizeof w->ssid);
    w->ip = associated ? ((2u << 24) | (77u << 16) | (168u << 8) | 192u) : 0;  /* lwIP addr value */
    w->rssi = associated ? -42 : 0;
    w->channel = associated ? 6 : 0;
    strset(w->note, "radio simulato: nessun hardware Wi-Fi in QEMU", sizeof w->note);
    return 0;
}

int kext_wifi_scan(void *buf, int cap)
{
    WifiAp *out = (WifiAp *)buf;
    int n = (int)(sizeof air / sizeof air[0]);
    if (cap < n) n = cap;
    for (int i = 0; i < n; i++) {
        strset(out[i].ssid, air[i].ssid, sizeof out[i].ssid);
        out[i].rssi = air[i].rssi;
        out[i].channel = air[i].channel;
        out[i].secure = air[i].secure;
    }
    return n;
}

int kext_wifi_connect(const char *want)
{
    if (!want || !want[0]) {           /* empty = disconnect */
        associated = 0;
        ssid[0] = 0;
        return 0;
    }
    for (int i = 0; i < (int)(sizeof air / sizeof air[0]); i++)
        if (eq(want, air[i].ssid)) {
            strset(ssid, want, sizeof ssid);
            associated = 1;
            return 0;
        }
    return -1;                          /* AP not in the simulated air */
}
