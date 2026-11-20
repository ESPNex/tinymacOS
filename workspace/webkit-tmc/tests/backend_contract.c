#include "tmc_backend.h"
#include "abi.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned calls;
static long result;
static int poll_mode;
static unsigned fake_tick, status_at;
static long last_n;
static uint64_t last_a,last_c,last_d;
long tmc_syscall(long n, uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
    ++calls; last_n=n; last_a=a; last_c=c; last_d=d;
    if (n==S_NET && a==3 && result>=0) *(uint32_t *)(uintptr_t)c=0x04030201;
    (void)b;
    if (poll_mode) {
        if (n == S_TICKS) return fake_tick;
        if (n == S_SLEEP) { fake_tick += (unsigned)a; return 0; }
        if (n == S_NET && a == TMC_NET_STATUS) {
            if (d == 6) return TMC_NET_EBADHANDLE;
            return fake_tick >= status_at ? result : 0;
        }
    }
    return result;
}
int main(void)
{
    uint32_t ip=42; char buf[16]={0};
    result=0; assert(tmc_resolve_ipv4("example.org",&ip)==0 && ip==0x04030201);
    assert(last_n==S_NET && last_a==3);
    result=-1; assert(tmc_resolve_ipv4("example.org",&ip)==-1 && ip==0 && errno==EHOSTUNREACH);
    unsigned before=calls;
    assert(tmc_resolve_ipv4(NULL,&ip)==-1 && errno==EINVAL);
    assert(tmc_resolve_ipv4("bad/host",&ip)==-1 && errno==EINVAL);
    assert(tmc_resolve_ipv4("https://example.org",&ip)==-1);
    assert(tmc_resolve_ipv4("example.org",NULL)==-1);
    char host[257]; memset(host,'a',256);host[256]=0;
    assert(tmc_resolve_ipv4(host,&ip)==-1);
    assert(calls==before);
    result=7; assert(tmc_tcp_open("example.org",443)==7 && last_c==443 && last_a==4);
    assert(tmc_tcp_open("example.org",0)==-1 && errno==EINVAL);
    result=2;assert(tmc_tcp_write(7,buf,4)==2 && last_a==5 && last_d==7);
    assert(tmc_tcp_write(7,buf,70000)==2 && last_c==65536);
    before=calls;assert(tmc_tcp_write(7,NULL,0)==0 && calls==before);
    assert(tmc_tcp_write(7,NULL,4)==-1 && errno==EFAULT);
    assert(tmc_tcp_write(-1,buf,4)==-1 && errno==EBADF);
    result=-1;assert(tmc_tcp_write(7,buf,4)==-1 && errno==EIO);
    result=TMC_NET_AGAIN;assert(tmc_tcp_read(7,buf,4)==-1 && errno==EAGAIN);
    result=TMC_NET_EBADHANDLE;assert(tmc_tcp_read(7,buf,4)==-1 && errno==EBADF);
    result=TMC_NET_RESET;assert(tmc_tcp_read(7,buf,4)==-1 && errno==ECONNRESET);
    result=TMC_NET_IO;assert(tmc_tcp_read(7,buf,4)==-1 && errno==EIO);
    result=TMC_NET_UNAVAILABLE;assert(tmc_tcp_read(7,buf,4)==-1 && errno==ENETDOWN);
    result=0;assert(tmc_tcp_read(7,buf,4)==0);
    result=3;assert(tmc_tcp_read(7,buf,4)==3 && last_a==TMC_NET_RECV_EX);
    result=5;assert(tmc_tcp_read(7,buf,4)==-1 && errno==EIO);
    result=0;assert(tmc_tcp_close(7)==0 && last_a==7 && last_d==7);
    result=-1;assert(tmc_tcp_close(7)==-1 && errno==EBADF);
    result=123;assert(tmc_monotonic_ns()==UINT64_C(1230000000) && last_n==S_TICKS);
    memset(buf,0x5a,sizeof buf);before=calls;
    assert(tmc_secure_random(buf,sizeof buf)==-1 && errno==ENOSYS && calls==before && buf[0]==0x5a);
    assert(tmc_secure_random(NULL,0)==0);
    result=TMC_NET_READABLE|TMC_NET_WRITABLE;
    assert(tmc_tcp_status(7)==result && last_a==TMC_NET_STATUS);
    poll_mode=1;fake_tick=0;status_at=0;
    struct tmc_pollfd fds[]={{7,TMC_NET_READABLE,99},{-1,0,99}};
    assert(tmc_socket_poll(fds,2,0)==1 && fds[0].revents==TMC_NET_READABLE && !fds[1].revents);
    status_at=100;assert(tmc_socket_poll(fds,2,0)==0 && !fds[0].revents);
    assert(tmc_socket_poll(fds,2,25)==0 && fake_tick==3);
    status_at=5;result=TMC_NET_READABLE;
    assert(tmc_socket_poll(fds,2,-1)==1 && fake_tick==5);
    result=TMC_NET_HANGUP;fds[0].events=0;
    assert(tmc_socket_poll(fds,2,0)==1 && fds[0].revents==TMC_NET_HANGUP);
    fds[0].handle=6;assert(tmc_socket_poll(fds,2,0)==1 && fds[0].revents==TMC_POLL_INVALID);
    assert(tmc_socket_poll(NULL,1,0)==-1 && errno==EINVAL);
    assert(tmc_socket_poll(fds,9,0)==-1 && errno==EINVAL);
    assert(tmc_socket_poll(fds,2,-2)==-1 && errno==EINVAL);
    assert(tmc_socket_poll(NULL,0,10)==0 && fake_tick==6);
    puts("PASS: DNS/TCP contracts, RESET vs EOF, polling, deadlines, clock, entropy fail-closed.");
}
