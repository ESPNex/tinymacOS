/* Host harness for verbatim kernel/net.c sections. Only lwIP primitives and
 * IRQ locks are mocked; the receive callbacks/read/status code is not copied.
 * This does NOT replace guest or real-network integration tests. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "netabi.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int err_t;
enum { ERR_OK=0, ERR_MEM=-1, ERR_RST=-14, ERR_IF=-12 };
typedef struct { int unused; } Spin;
struct tcp_pcb { unsigned room; };
struct pbuf { struct pbuf *next; void *payload; u16 len,tot_len; };
#define NSOCK 8
#define SRING 8192u
static struct sock {
    struct tcp_pcb *pcb;
    volatile u32 rd,wr;
    int used,dead,eof,connected,failure;
    u8 buf[SRING];
} socks[NSOCK];
static int ready=1;
static Spin net_lock;
static unsigned credited, freed, locked;
static void spin_lock(Spin *s,u32 *fl) { (void)s;*fl=0;assert(!locked);locked=1; }
static void spin_unlock(Spin *s,u32 fl) { (void)s;(void)fl;assert(locked);locked=0; }
static void tcp_recved(struct tcp_pcb *p,u16 n) { assert(p);credited+=n; }
static unsigned tcp_sndbuf(struct tcp_pcb *p) { return p->room; }
static void pbuf_free(struct pbuf *p) { assert(p);++freed; }
/* @KERNEL_STREAM_IMPLEMENTATION@ */
static struct tcp_pcb pcb={8192};
static void reset(void)
{
    memset(socks,0,sizeof socks);socks[0].used=1;socks[0].pcb=&pcb;
    ready=1;credited=freed=locked=0;
    assert(s_conn(&socks[0],&pcb,ERR_OK)==ERR_OK);
}
static void check(void)
{
    char out[SRING+1];char payload[SRING+1];memset(payload,'x',sizeof payload);
    reset();
    assert(net_recv_ex(0,out,8)==TMC_NET_AGAIN);
    assert(net_recv(0,out,8)==0);
    assert(net_status(0)==TMC_NET_WRITABLE);
    struct pbuf p={0,payload,SRING,SRING};
    assert(s_recv(&socks[0],&pcb,&p,ERR_OK)==ERR_OK && freed==1 && credited==0);
    assert(net_status(0)==(TMC_NET_READABLE|TMC_NET_WRITABLE));
    struct pbuf extra={0,payload,1,1};
    assert(s_recv(&socks[0],&pcb,&extra,ERR_OK)==ERR_MEM && freed==1 && socks[0].wr==SRING);
    assert(net_recv_ex(0,out,13)==13 && credited==13);
    assert(s_recv(&socks[0],&pcb,&extra,ERR_OK)==ERR_OK && freed==2);
    assert(net_recv_ex(0,out,sizeof out)==SRING-12 && credited==SRING+1);
    assert(net_recv_ex(0,out,8)==TMC_NET_AGAIN);
    /* FIN is observed only after bytes already in the ring have been delivered. */
    extra.payload="abc";extra.len=extra.tot_len=3;
    assert(s_recv(&socks[0],&pcb,&extra,ERR_OK)==ERR_OK);
    assert(s_recv(&socks[0],&pcb,0,ERR_OK)==ERR_OK);
    assert(net_recv_ex(0,out,8)==3 && memcmp(out,"abc",3)==0);
    assert(net_recv_ex(0,out,8)==0 && net_recv(0,out,8)==-1);
    assert(!socks[0].dead && (net_status(0)&TMC_NET_WRITABLE));
    assert(net_status(0)&TMC_NET_HANGUP);
    /* Reset must never turn into a successful clean EOF. */
    reset();assert(s_recv(&socks[0],&pcb,&extra,ERR_OK)==ERR_OK);
    s_err(&socks[0],ERR_RST);
    assert(net_recv_ex(0,out,8)==3 && credited==0);
    assert(net_recv_ex(0,out,8)==TMC_NET_RESET);
    assert(net_status(0)==(TMC_NET_READABLE|TMC_NET_ERROR));
    reset();s_err(&socks[0],ERR_IF);assert(net_recv_ex(0,out,8)==TMC_NET_IO);
    reset();assert(s_recv(&socks[0],&pcb,&extra,ERR_IF)==ERR_IF && !freed);
    assert(net_recv_ex(-1,out,8)==TMC_NET_EBADHANDLE);
    assert(net_recv_ex(7,out,8)==TMC_NET_EBADHANDLE);
    assert(net_status(8)==TMC_NET_EBADHANDLE);
    assert(net_recv_ex(0,0,8)==TMC_NET_IO);
    ready=0;assert(net_recv_ex(0,out,8)==TMC_NET_UNAVAILABLE);
    assert(net_status(0)==TMC_NET_UNAVAILABLE);
    /* A chained pbuf is accepted atomically, including ring wrap. */
    reset();socks[0].rd=socks[0].wr=SRING-2;
    struct pbuf b={0,"def",3,3},a={&b,"abc",3,6};
    assert(s_recv(&socks[0],&pcb,&a,ERR_OK)==ERR_OK);
    assert(net_recv_ex(0,out,6)==6 && memcmp(out,"abcdef",6)==0);
    reset();socks[0].wr=SRING-5;
    assert(s_recv(&socks[0],&pcb,&a,ERR_OK)==ERR_MEM && !freed && socks[0].wr==SRING-5);
    assert(!credited);
}
static uint32_t prng=0x12345678;
static unsigned next(void) { prng^=prng<<13;prng^=prng>>17;prng^=prng<<5;return prng; }
static void stress(void)
{
    reset();socks[0].rd=socks[0].wr=UINT32_MAX-100;
    unsigned produced=0,consumed=0;unsigned char in[SRING],out[SRING];
    for (unsigned turn=0;turn<12000;turn++) {
        unsigned n=1+next()%SRING;
        for (unsigned i=0;i<n;i++)in[i]=(unsigned char)((produced+i)*131u+17u);
        struct pbuf p={0,in,(u16)n,(u16)n};
        u32 oldwr=socks[0].wr;unsigned oldfree=freed;
        err_t r=s_recv(&socks[0],&pcb,&p,ERR_OK);
        if (r==ERR_OK) {produced+=n;assert(freed==oldfree+1);}
        else assert(r==ERR_MEM && socks[0].wr==oldwr && freed==oldfree);
        int count=net_recv_ex(0,out,1+next()%SRING);
        if (count>0) {
            for (int i=0;i<count;i++)assert(out[i]==(unsigned char)((consumed+(unsigned)i)*131u+17u));
            consumed+=(unsigned)count;
        } else assert(count==TMC_NET_AGAIN);
        assert(produced-consumed==socks[0].wr-socks[0].rd);
        assert(credited==consumed);
    }
    while (produced!=consumed) {
        int count=net_recv_ex(0,out,SRING);assert(count>0);
        for(int i=0;i<count;i++)assert(out[i]==(unsigned char)((consumed+(unsigned)i)*131u+17u));
        consumed+=(unsigned)count;
    }
    assert(credited==produced);
    printf("PASS: stream stress %u bytes; no loss/duplication across ring/counter wrap.\n",produced);
}
int main(void) { check();stress();puts("PASS: kernel callback backpressure, pbuf ownership, receive-window credit, FIN/RESET/status and legacy op6 contracts."); }
