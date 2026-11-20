#include "k.h"
#include "netabi.h"
#include "../include/kext.h"
#include "lwip/init.h"
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/ip4.h"
#include "lwip/icmp.h"
#include "lwip/prot/icmp.h"
#include "lwip/raw.h"
#include "lwip/dhcp.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/timeouts.h"
#include "lwip/inet_chksum.h"
#include "netif/ethernet.h"
#include <spinlock.h>

/* In-kernel IPv4 stack glue: driver kexts register netdevs, frames flow in
 * with net_rx() and out through ops->tx(). Everything lwIP-visible runs under
 * net_lock (IRQ-safe): the PIT tick polls drivers + lwIP timeouts, syscalls
 * send pings and read interface state. */

#define MAXDEV 4
static struct netdev_ops *devops[MAXDEV];
static void *devctx[MAXDEV];
static int devlink[MAXDEV];
static char devnm[MAXDEV][8];
static struct netif nif[MAXDEV];
static int ndev, ready, wifi_idx = -1;
static Spin net_lock;
static struct raw_pcb *pingpcb;
static volatile u32 ping_got, ping_ms[8];
static u32 ping_ms_start[8];

static err_t lwip_tx(struct netif *net, struct pbuf *p)
{
    int i = (int)(long)net->state;
    if (i < 0 || i >= ndev || !devops[i] || !devops[i]->tx) return ERR_IF;
    u8 frame[1600];
    int n = 0;
    for (struct pbuf *q = p; q; q = q->next) {
        if (n + q->len > (int)sizeof frame) return ERR_BUF;
        memcpy(frame + n, q->payload, q->len);
        n += q->len;
    }
    if (n >= 14) {
        u32 et = ((u32)frame[12] << 8) | frame[13];
        if (et == 0x0806)
            logstr("[tw] arp\n");
        else if (et == 0x0800 && n >= 24 && frame[23] == 1) {
            static u32 icmpn;
            logstr("[tw] icmp\n");
            if (icmpn++ < 2) {
                char b[110];
                const char *hx = "0123456789abcdef";
                int L = 0;
                b[L++] = '['; b[L++] = 'f'; b[L++] = '='; b[L++] = ' ';
                /* dst mac 0-5, src mac 6-11, et 12-13, ip: ttl22 proto23
                   cksum24-25 src26-29 dst30-33, icmp: type34 ck36-37 id38-39 seq40-41 */
                static const int off[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,
                                          22,23,24,25,26,27,28,29,30,31,
                                          34,36,37,38,39,40,41};
                for (int k = 0; k < (int)(sizeof off / sizeof off[0]) && L < 104; k++) {
                    u8 v = frame[off[k]];
                    b[L++] = hx[v >> 4]; b[L++] = hx[v & 15];
                }
                b[L++] = ']'; b[L] = 0;
                logstr(b); logstr("\n");
            }
        }
    }
    devops[i]->tx(devctx[i], frame, n);
    return ERR_OK;
}

static err_t if_init(struct netif *n)
{
    n->output = etharp_output;
    n->linkoutput = lwip_tx;
    n->hwaddr_len = 6;
    n->mtu = 1500;
    n->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
    return ERR_OK;
}

int net_register(const char *name, const u8 mac[6], struct netdev_ops *ops)
{
    if (!ready || ndev >= MAXDEV || !ops || !ops->tx) return -1;
    u32 rfl;
    spin_lock(&net_lock, &rfl);
    int i = ndev++;
    devops[i] = ops;
    devctx[i] = ops->ctx;
    devlink[i] = 0;
    strset(devnm[i], name, sizeof devnm[i]);
    ip4_addr_t a, g, m;
    IP4_ADDR(&a, 0, 0, 0, 0);
    IP4_ADDR(&g, 0, 0, 0, 0);
    IP4_ADDR(&m, 0, 0, 0, 0);
    if (!netif_add(&nif[i], &a, &m, &g, (void *)(long)i, if_init, ethernet_input)) {
        ndev--;
        return -1;
    }
    memcpy(nif[i].hwaddr, mac, 6);
    nif[i].name[0] = name[0];
    nif[i].name[1] = name[1];
    nif[i].num = 0;                     /* en0 (NIC), wl0 (radio) */
    if (name[0] == 'w' && name[1] == 'l') wifi_idx = i;
    if (i == 0) netif_set_default(&nif[i]);
    netif_set_up(&nif[i]);
    if (i == 0) {
        /* first NIC: slirp DHCP (real address from QEMU user networking) */
        dhcp_start(&nif[i]);
    }
    spin_unlock(&net_lock, rfl);
    return i;
}

/* Called from driver poll / lwip paths: the caller already holds net_lock. */
void net_rx(int i, const u8 *frame, int len)
{
    if (!ready || i < 0 || i >= ndev || !frame || len < 14 || len > 1600) return;
    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p) {
        memcpy(p->payload, frame, len);
        p->len = (u16)len;
        if (frame[12] == 0x08 && frame[13] == 0x06)
            logstr("[rw] arp\n");
        else if (frame[12] == 0x08 && frame[13] == 0x00 && len >= 24 && frame[23] == 1)
            logstr("[rw] icmp\n");
        else if (frame[12] == 0x08 && frame[13] == 0x00 && len >= 38 && frame[23] == 17)
            logstr("[rw] udp\n");
        if (ethernet_input(p, &nif[i]) != ERR_OK) pbuf_free(p);
    }
}

void net_set_link(int i, int up)
{
    if (!ready || i < 0 || i >= ndev) return;
    u32 fl;
    spin_lock(&net_lock, &fl);
    if (up) {
        if (i != 0 && !devlink[i]) {
            /* simulated radio: static addressing on the test network */
            ip4_addr_t a, m, g;
            IP4_ADDR(&a, 192, 168, 77, 2);
            IP4_ADDR(&m, 255, 255, 255, 0);
            IP4_ADDR(&g, 192, 168, 77, 1);
            netif_set_addr(&nif[i], &a, &m, &g);
        }
        netif_set_link_up(&nif[i]);
        netif_set_up(&nif[i]);
        devlink[i] = 1;
    } else {
        netif_set_link_down(&nif[i]);
        netif_set_down(&nif[i]);
        devlink[i] = 0;
    }
    spin_unlock(&net_lock, fl);
}

/* PIT tick (CPU0,100 Hz): poll driver hardware, then run lwIP timers. */
void net_tick(void)
{
    if (!ready) return;
    static u32 ntn;
    if ((++ntn % 200) == 1) logstr("[nt]\n");
    u32 fl;
    spin_lock(&net_lock, &fl);    /* serializes driver polls against syscalls */
    kext_net_tick();              /* drivers call net_rx() (lock already held) */
    sys_check_timeouts();
    spin_unlock(&net_lock, fl);
}

static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    (void)arg;
    (void)pcb;
    (void)addr;
    if (!p || p->tot_len < 20 + 8) return 1;
    u8 hdr[28];
    if (pbuf_copy_partial(p, hdr, 28, 0) < 28) return 1;
    if (hdr[20] != ICMP_ER) return 1;          /* echo reply */
    u16 id = (u16)((hdr[24] << 8) | hdr[25]);  /* ICMP id at offset 4 */
    if (id != 0x544d) return 1;
    u16 seq = (u16)((hdr[26] << 8) | hdr[27]); /* ICMP seq at offset 6 */
    if (seq < 8) {
        ping_ms[seq] = ticks - ping_ms_start[seq];
        ping_got++;
        logstr("[ping] recv\n");
    }
    return 1;
}

int net_ping(u32 ipbe, int count, u32 *rtt, int cap, u32 *okout)
{
    if (!ready || count < 1 || count > 5) return -1;
    logstr("[ping] enter\n");
    u32 fl;
    spin_lock(&net_lock, &fl);
    logstr("[ping] lock\n");
    if (!pingpcb) {
        pingpcb = raw_new(IP_PROTO_ICMP);
        if (pingpcb) raw_recv(pingpcb, ping_recv, 0);
    }
    if (!pingpcb) {
        spin_unlock(&net_lock, fl);
        return -1;
    }
    ping_got = 0;
    ip4_addr_t dst;
    dst.addr = ipbe;
    int sent = 0;
    for (int i = 0; i < count && i < 8; i++) {
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 8 + 32, PBUF_RAM);
        if (!p) break;
        u8 *d = (u8 *)p->payload;
        d[0] = ICMP_ECHO;
        d[1] = 0;
        d[2] = 0;
        d[3] = 0;
        d[4] = 0x54;
        d[5] = 0x4d;
        d[6] = (u8)(i >> 8);
        d[7] = (u8)i;
        for (int k = 0; k < 32; k++) d[8 + k] = (u8)('a' + k);
        p->len = 40;
        p->tot_len = 40;
        u16 c = inet_chksum(d, 40);
        d[2] = (u8)c;             /* lwIP convention: LE store of inet_chksum */
        d[3] = (u8)(c >> 8);
        {
            u16 v = inet_chksum(d, 40);   /* must be 0 when checksum is right */
            char b[28];
            const char *hx = "0123456789abcdef";
            b[0] = '['; b[1] = 'c'; b[2] = 'k'; b[3] = '=';
            b[4] = hx[(c >> 12) & 15]; b[5] = hx[(c >> 8) & 15];
            b[6] = hx[(c >> 4) & 15]; b[7] = hx[c & 15];
            b[8] = ' '; b[9] = 'v'; b[10] = '=';
            b[11] = hx[(v >> 12) & 15]; b[12] = hx[(v >> 8) & 15];
            b[13] = hx[(v >> 4) & 15]; b[14] = hx[v & 15];
            b[15] = ']'; b[16] = 0;
            logstr(b); logstr("\n");
        }
        ping_ms_start[i] = (u32)ticks;
        {
            int r = raw_sendto(pingpcb, p, &dst);
            if (r == ERR_OK) sent++;
            char b[24];
            const char *hx = "0123456789abcdef";
            b[0] = '['; b[1] = 'p'; b[2] = 'i'; b[3] = 'n'; b[4] = 'g';
            b[5] = ' '; b[6] = 's'; b[7] = 'e'; b[8] = 'n'; b[9] = 'd';
            b[10] = ' '; b[11] = 'i'; b[12] = '=';
            b[13] = (char)('0' + i);
            b[14] = ' '; b[15] = 'r'; b[16] = '=';
            b[17] = (char)('0' + (r < 0 ? -r : r));
            b[18] = (r < 0 ? '-' : '+');
            b[19] = ']'; b[20] = 0;
            logstr(b); logstr("\n");
        }
        pbuf_free(p);
        /* Release the lock so the PIT tick can poll the wire; replies land
         * through net_rx() from that same tick. */
        u32 end = ticks + 25;
        {
            int wfirst = 1;
            while (ticks < end && ping_got <= (u32)i) {
                if (wfirst) { logstr("[ping] w0\n"); wfirst = 0; }
                spin_unlock(&net_lock, fl);
                asm volatile("sti" ::: "memory"); /* block with IRQs on: PIT must run */
                spin_lock(&net_lock, &fl);
            }
            logstr("[ping] wend\n");
        }
    }
    u32 got = ping_got;
    spin_unlock(&net_lock, fl);
    logstr("[ping] done\n");
    if (okout) *okout = got;
    if (rtt) {
        u32 n = got < (u32)cap ? got : (u32)cap;
        for (u32 i = 0; i < n; i++) rtt[i] = ping_ms[i] * 10;  /* ticks(100Hz) -> ms */
    }
    return sent;
}

int net_ifinfo(NetIfInfo *out, int cap)
{
    if (!ready) return 0;
    int n = ndev < cap ? ndev : cap;
    u32 fl;
    spin_lock(&net_lock, &fl);
    for (int i = 0; i < n; i++) {
        out[i].name[0] = nif[i].name[0];
        out[i].name[1] = nif[i].name[1];
        out[i].name[2] = (char)('0' + (nif[i].num % 10));
        out[i].name[3] = 0;
        memcpy(out[i].mac, nif[i].hwaddr, 6);
        out[i].up = (netif_is_up(&nif[i]) ? 1 : 0) | (netif_is_link_up(&nif[i]) ? 2 : 0);
        out[i].wifi = i != 0;
        out[i].ip = ip4_addr_get_u32(netif_ip4_addr(&nif[i]));
        out[i].mask = ip4_addr_get_u32(netif_ip4_netmask(&nif[i]));
        out[i].rx = 0;
        out[i].tx = 0;
    }
    spin_unlock(&net_lock, fl);
    return n;
}

int net_wifi_op(int sub, void *buf, int cap, const char *ssid)
{
    struct LoadedKext *w = kext_find_wifi();
    if (!w) return -1;
    u32 fl;
    spin_lock(&net_lock, &fl);
    int r, needlink = 0;
    if (sub == 0) r = w->wifi_status ? w->wifi_status(buf) : -1;
    else if (sub == 1) r = w->wifi_scan ? w->wifi_scan(buf, cap) : -1;
    else if (sub == 2) {
        r = w->wifi_connect ? w->wifi_connect(ssid ? ssid : "") : -1;
        needlink = (r == 0);
    } else r = -1;
    spin_unlock(&net_lock, fl);
    if (needlink && wifi_idx >= 0) net_set_link(wifi_idx, 1);
    return r;
}

/* ---- lwIP NO_SYS glue ----
 * Every lwIP entry point above runs with net_lock already held, so the
 * lightweight protect hooks are no-ops; sys_now() maps the 100 Hz PIT. */
u32 sys_now(void)
{
    return (u32)ticks * 10u;
}

sys_prot_t sys_arch_protect(void)
{
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    (void)pval;
}

/* netif_find() parses interface indexes with strtol(); freestanding kernel. */
long strtol(const char *s, char **end, int base)
{
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    int neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') s++;
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }
    if (base == 0) base = 10;
    long v = 0;
    for (;;) {
        char c = *s;
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
        s++;
    }
    if (end) *end = (char *)s;
    return neg ? -v : v;
}

/* ---- TCP sockets + zone-aware DNS resolver (S_NET ops 3..7) ----
 * Resolution: dotted quad is direct; *.levante.test goes to the simulated
 * AP (192.168.77.1, wifi kext answers), everything else to the QEMU slirp
 * resolver10.0.2.3. Sockets run under net_lock; waits release the lock with
 * IRQs on so the PIT tick can poll the wire (same pattern as net_ping). */
#define NSOCK 8
#define SRING 8192u
static struct sock {
    struct tcp_pcb *pcb;
    volatile u32 rd, wr;              /* monotonic; index = x & (SRING-1) */
    int used, dead, eof, connected, failure;
    u8 buf[SRING];
} socks[NSOCK];
static struct udp_pcb *dnspcb;
static volatile int dns_done;
static volatile u32 dns_ip;
static volatile u16 dns_xid;

static u32 rand32(void)
{
    static u32 s = 0x2545f491u;
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s ^ ticks ^ (ticks << 16);
}

static int parse_ip(const char *str, u32 *out)
{
    u32 o[4];
    int k = 0;
    const char *p = str;
    while (k < 4) {
        if (*p < '0' || *p > '9') return -1;
        u32 v = 0;
        int dig = 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10 + (u32)(*p - '0');
            if (v > 255) return -1;
            p++;
            dig++;
        }
        if (!dig) return -1;
        o[k++] = v;
        if (*p == '.') { p++; if (!*p) return -1; }
        else break;
    }
    if (*p || k != 4) return -1;
    *out = (o[3] << 24) | (o[2] << 16) | (o[1] << 8) | o[0];   /* lwIP LE order */
    return 0;
}

static int has_suffix(const char *str, const char *suf)
{
    size_t n = strlen(str), m = strlen(suf);
    return n >= m && eq(str + n - m, suf);
}

static void dns_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                     const ip_addr_t *addr, u16 port)
{
    (void)arg; (void)pcb; (void)addr; (void)port;
    if (!p) { logstr("[dns] rx null\n"); return; }
    logstr("[dns] rx\n");
    u8 h[512];
    u16 tot = p->tot_len;
    u16 take = tot < (u16)sizeof h ? tot : (u16)sizeof h;
    int n = pbuf_copy_partial(p, h, take, 0);
    pbuf_free(p);
    if (n < 12) return;
    if ((u16)((h[0] << 8) | h[1]) != dns_xid) { logstr("[dns] xid mismatch\n"); return; }
    if (!(h[2] & 0x80)) return;                 /* must be a response */
    if (h[3] & 0x0f) { logstr("[dns] rcode\n"); return; }
    u16 qd = (u16)((h[4] << 8) | h[5]);
    u16 an = (u16)((h[6] << 8) | h[7]);
    if (qd < 1 || an < 1) { logstr("[dns] empty\n"); return; }
    {
        char b[72];
        const char *hx = "0123456789abcdef";
        int k = 0;
        const char *pre = "[dns] msg n=";
        while (pre[k]) { b[k] = pre[k]; k++; }
        if (n >= 100) b[k++] = '0' + (n / 100);
        b[k++] = '0' + ((n / 10) % 10);
        b[k++] = '0' + (n % 10);
        b[k++] = ' ';
        for (int i = 34; i < 50 && i < n; i++) {
            b[k++] = hx[(h[i] >> 4) & 15];
            b[k++] = hx[h[i] & 15];
        }
        b[k++] = 0;
        logstr(b);
        logstr("\n");
    }
    int off = 12;
    /* skip questions */
    for (int q = 0; q < qd; q++) {
        while (off < n && h[off]) {
            if ((h[off] & 0xc0) == 0xc0) { off += 2; goto qskip; }
            off += 1 + h[off];
        }
        off++;                                 /* root label */
        off += 4;                              /* type + class */
    }
qskip:
    /* first A answer */
    for (int a = 0; a < an && off < n; a++) {
        if ((h[off] & 0xc0) == 0xc0) off += 2;
        else {
            while (off < n && h[off]) off += 1 + h[off];
            off++;
        }
        if (off + 10 > n) break;
        u16 type = (u16)((h[off] << 8) | h[off + 1]);
        u16 rdlen = (u16)((h[off + 8] << 8) | h[off + 9]);
        off += 10;
        if (off + rdlen > n) break;
        if (type == 1 && rdlen == 4) {
            dns_ip = (u32)((h[off + 3] << 24) | (h[off + 2] << 16) |
                           (h[off + 1] << 8) | h[off]);
            dns_done = 1;
            logstr("[dns] answer\n");
        }
        off += rdlen;
    }
}

/* Under net_lock: send one A query. 0 = done (literal), 1 = pending, -1 = err. */
static int dns_query_locked(const char *host, u32 *out)
{
    if (parse_ip(host, out) == 0) return 0;
    if (!dnspcb || strlen(host) > 200) return -1;
    u8 q[256];
    int ql = 0;
    dns_xid = (u16)rand32();
    dns_done = 0;
    q[0] = (u8)(dns_xid >> 8);
    q[1] = (u8)dns_xid;
    q[2] = 1; q[3] = 0;                       /* RD */
    q[4] = 0; q[5] = 1;                       /* QDCOUNT */
    q[6] = q[7] = q[8] = q[9] = q[10] = q[11] = 0;
    ql = 12;
    const char *p = host;
    while (*p) {
        const char *dot = p;
        while (*dot && *dot != '.') dot++;
        int lab = (int)(dot - p);
        if (lab < 1 || lab > 63 || ql + lab + 7 >= (int)sizeof q) return -1;
        q[ql++] = (u8)lab;
        memcpy(q + ql, p, (size_t)lab);
        ql += lab;
        p = *dot ? dot + 1 : dot;
    }
    q[ql++] = 0;
    q[ql++] = 0; q[ql++] = 1;                 /* A */
    q[ql++] = 0; q[ql++] = 1;                 /* IN */
    ip4_addr_t srv;
    if (eq(host, "levante.test") || has_suffix(host, ".levante.test"))
        IP4_ADDR(&srv, 192, 168, 77, 1);      /* simulated AP zone */
    else
        IP4_ADDR(&srv, 10, 0, 2, 3);          /* QEMU slirp resolver */
    struct pbuf *pb = pbuf_alloc(PBUF_TRANSPORT, (u16)ql, PBUF_RAM);
    if (!pb) return -1;
    memcpy(pb->payload, q, (size_t)ql);
    if (udp_sendto(dnspcb, pb, &srv, 53) != ERR_OK) {
        pbuf_free(pb);
        logstr("[dns] send fail\n");
        return -1;
    }
    pbuf_free(pb);
    logstr(srv.addr == ((1u << 24) | (77u << 16) | (168u << 8) | 192u)
               ? "[dns] query zone -> ap\n"
               : "[dns] query -> slirp\n");
    return 1;
}

/* Wait for dns_done; caller holds net_lock and passes its flag word. */
static int dns_wait(u32 *flp)
{
    u32 end = ticks + 150;                    /* 1.5 s @100 Hz */
    while (!dns_done && ticks < end) {
        spin_unlock(&net_lock, *flp);
        asm volatile("sti" ::: "memory");     /* PIT must run to poll the wire */
        spin_lock(&net_lock, flp);
    }
    return dns_done;
}

int net_resolve(const char *host, u32 *ipout)
{
    if (!ready || !host || !host[0] || !ipout) return -1;
    u32 fl;
    spin_lock(&net_lock, &fl);
    int r = dns_query_locked(host, ipout);
    if (r == 1) {
        if (dns_wait(&fl)) { *ipout = dns_ip; r = 0; }
        else r = -1;
    }
    spin_unlock(&net_lock, fl);
    return r;
}

/* STREAM_CALLBACKS_BEGIN: compiled verbatim by host contract tests. */
static err_t s_conn(void *arg, struct tcp_pcb *pcb, err_t err)
{
    (void)pcb;
    struct sock *s = arg;
    if (err != ERR_OK) {
        s->dead = 1;
        s->failure = err == ERR_RST ? TMC_NET_RESET : TMC_NET_IO;
    } else s->connected = 1;
    return ERR_OK;
}

static void s_err(void *arg, err_t err)
{
    struct sock *s = arg; /* lwIP already freed the pcb. Buffered bytes survive. */
    s->dead = 1;
    s->failure = err == ERR_RST ? TMC_NET_RESET : TMC_NET_IO;
    s->pcb = 0;
}

static err_t s_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    (void)pcb;
    struct sock *s = arg;
    if (err != ERR_OK) return err; /* no ownership transfer */
    if (!p) {
        s->eof = 1; /* peer FIN: local writes remain legal (TCP half-close). */
        return ERR_OK;
    }
    u32 avail = s->wr - s->rd;
    /* Refuse the WHOLE chain without freeing/modifying it. lwIP retains and
     * retries refused_data. Never acknowledge bytes discarded by a full ring. */
    if (avail > SRING || p->tot_len > SRING - avail) return ERR_MEM;
    for (struct pbuf *q = p; q; q = q->next) {
        u32 idx = s->wr & (SRING - 1);
        u32 first = SRING - idx;
        if (first > q->len) first = q->len;
        memcpy(s->buf + idx, q->payload, first);
        if (q->len > first) memcpy(s->buf, (u8 *)q->payload + first, q->len - first);
        s->wr += q->len;
    }
    /* The receive window is credited only when the application reads. */
    pbuf_free(p);
    return ERR_OK;
}
/* STREAM_CALLBACKS_END */

static int conn_wait(struct sock *s, u32 *flp)
{
    u32 end = ticks + 300;                    /* 3 s */
    while (!s->dead && s->pcb && !s->connected && ticks < end) {
        spin_unlock(&net_lock, *flp);
        asm volatile("sti" ::: "memory");
        spin_lock(&net_lock, flp);
    }
    return !s->dead && s->pcb && s->connected;
}

int net_connect(const char *host, u16 port)
{
    if (!ready || !host || !host[0] || port < 1) return -1;
    u32 fl;
    spin_lock(&net_lock, &fl);
    int h = -1;
    for (int i = 0; i < NSOCK; i++)
        if (!socks[i].used) { h = i; break; }
    if (h < 0) { spin_unlock(&net_lock, fl); return -1; }
    u32 ip = 0;
    int rr = dns_query_locked(host, &ip);
    if (rr == 1) {
        if (dns_wait(&fl)) { ip = dns_ip; rr = 0; }
        else rr = -1;
    }
    if (rr < 0) { spin_unlock(&net_lock, fl); return -1; }
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) { spin_unlock(&net_lock, fl); return -1; }
    struct sock *s = &socks[h];
    s->used = 1;
    s->dead = s->eof = s->connected = s->failure = 0;
    s->rd = s->wr = 0;
    s->pcb = pcb;
    tcp_arg(pcb, s);
    tcp_recv(pcb, s_recv);
    tcp_err(pcb, s_err);
    tcp_nagle_disable(pcb);                   /* small requests, snappy fetches */
    ip4_addr_t dst;
    dst.addr = ip;
    if (tcp_connect(pcb, &dst, port, s_conn) != ERR_OK) {
        tcp_arg(pcb, 0);
        tcp_recv(pcb, 0);
        tcp_err(pcb, 0);
        tcp_abort(pcb);
        s->used = 0;
        s->pcb = 0;
        spin_unlock(&net_lock, fl);
        return -1;
    }
    if (!conn_wait(s, &fl)) {
        if (s->pcb) {
            tcp_arg(s->pcb, 0);
            tcp_recv(s->pcb, 0);
            tcp_err(s->pcb, 0);
            tcp_abort(s->pcb);
        }
        s->used = 0;
        s->pcb = 0;
        spin_unlock(&net_lock, fl);
        return -1;
    }
    spin_unlock(&net_lock, fl);
    return h;
}

int net_send(int h, const void *buf, int len)
{
    if (!ready || h < 0 || h >= NSOCK || !buf || len < 1 || len > 65536) return -1;
    u32 fl;
    spin_lock(&net_lock, &fl);
    struct sock *s = &socks[h];
    if (!s->used || s->dead || !s->pcb) {
        spin_unlock(&net_lock, fl);
        return -1;
    }
    const u8 *b = buf;
    int sent = 0;
    u32 end = ticks + 500;                    /* 5 s */
    while (sent < len && ticks < end && !s->dead && s->pcb) {
        int room = (int)tcp_sndbuf(s->pcb);
        int n = len - sent;
        if (n > room) n = room;
        if (n > 0) {
            if (tcp_write(s->pcb, b + sent, (u16)n, TCP_WRITE_FLAG_COPY) != ERR_OK)
                break;
            tcp_output(s->pcb);
            sent += n;
        } else {
            spin_unlock(&net_lock, fl);
            asm volatile("sti" ::: "memory");
            spin_lock(&net_lock, &fl);
        }
    }
    spin_unlock(&net_lock, fl);
    return sent > 0 ? sent : -1;
}

/* STREAM_READ_BEGIN: compiled verbatim by host contract tests. */
static int net_read_locked(struct sock *s, void *buf, int cap)
{
    u32 avail = s->wr - s->rd;
    if (!avail) {
        if (s->failure) return s->failure;
        if (s->eof) return 0;
        if (s->dead) return TMC_NET_IO;
        return TMC_NET_AGAIN;
    }
    int n = cap < (int)avail ? cap : (int)avail;
    u32 idx = s->rd & (SRING - 1);
    u32 first = SRING - idx;
    if (first > (u32)n) first = (u32)n;
    memcpy(buf, s->buf + idx, first);
    if ((u32)n > first) memcpy((u8 *)buf + first, s->buf, (u32)n - first);
    s->rd += (u32)n;
    if (s->pcb && !s->dead) tcp_recved(s->pcb, (u16)n);
    return n;
}

int net_recv_ex(int h, void *buf, int cap)
{
    if (!ready) return TMC_NET_UNAVAILABLE;
    if (h < 0 || h >= NSOCK) return TMC_NET_EBADHANDLE;
    if (!buf || cap < 1 || cap > 65536) return TMC_NET_IO;
    u32 fl;
    spin_lock(&net_lock, &fl);
    int r = socks[h].used ? net_read_locked(&socks[h], buf, cap) : TMC_NET_EBADHANDLE;
    spin_unlock(&net_lock, fl);
    return r;
}

int net_recv(int h, void *buf, int cap)
{
    int r = net_recv_ex(h, buf, cap);
    /* Original op 6: 0 means no data; negative means closed/failed. */
    if (r == TMC_NET_AGAIN) return 0;
    return r > 0 ? r : -1;
}

int net_status(int h)
{
    if (!ready) return TMC_NET_UNAVAILABLE;
    if (h < 0 || h >= NSOCK) return TMC_NET_EBADHANDLE;
    u32 fl;
    spin_lock(&net_lock, &fl);
    struct sock *s = &socks[h];
    int r = TMC_NET_EBADHANDLE;
    if (s->used) {
        r = 0;
        if (s->wr != s->rd || s->eof || s->dead) r |= TMC_NET_READABLE;
        if (s->pcb && s->connected && !s->dead && tcp_sndbuf(s->pcb)) r |= TMC_NET_WRITABLE;
        if (s->eof) r |= TMC_NET_HANGUP;
        if (s->dead || s->failure) r |= TMC_NET_ERROR;
    }
    spin_unlock(&net_lock, fl);
    return r;
}
/* STREAM_READ_END */

int net_closesock(int h)
{
    if (!ready || h < 0 || h >= NSOCK) return -1;
    u32 fl;
    spin_lock(&net_lock, &fl);
    struct sock *s = &socks[h];
    if (!s->used) {
        spin_unlock(&net_lock, fl);
        return -1;
    }
    struct tcp_pcb *pcb = s->pcb;
    s->used = 0;
    s->pcb = 0;
    s->dead = 1;
    if (pcb) {
        tcp_arg(pcb, 0);
        tcp_recv(pcb, 0);
        tcp_err(pcb, 0);
        if (tcp_close(pcb) != ERR_OK) tcp_abort(pcb);
    }
    spin_unlock(&net_lock, fl);
    return 0;
}

void net_init(void)
{
    net_lock.v = 0;                    /* Spin is a bare ticket word (BSS-safe) */
    lwip_init();
    dnspcb = udp_new();
    if (dnspcb) {
        udp_bind(dnspcb, IP_ADDR_ANY, 53000);
        udp_recv(dnspcb, dns_recv, 0);
        logstr(dnspcb->local_port == 53000 ? "[dns] pcb ok\n" : "[dns] pcb BAD port\n");
    } else logstr("[dns] pcb alloc fail\n");
    ready = 1;
    logstr("[net] lwIP ");
    logstr(LWIP_VERSION_STRING);
    logstr(" pronto\n");
}
