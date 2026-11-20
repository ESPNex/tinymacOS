/* QA-only executable: linked into an EXTERNAL disposable test volume, never
 * shipped as a browser or copied into the production source tree's command.c.
 * Exercises the actual target backend and kernel/lwIP with a controlled peer. */
#include "lib.h"
#include "tmc_tls.h"
static struct tmc_tls_block probe_tls ALIGNED(16);
#include "tmc_backend.h"
#include "tmc_descriptors.h"
#include "sha256.h"
#include <errno.h>
static char output[2048];
static unsigned used;
static struct tmc_fd_table descriptors;
static unsigned long transferred;
static unsigned char buffer[12001];
static void text(const char *s) { while (*s && used < sizeof(output)-1) output[used++]=*s++; }
static void decimal(unsigned long n) {
    char b[24]; unsigned i=0; do { b[i++]='0'+n%10; n/=10; } while(n);
    while(i && used<sizeof(output)-1) output[used++]=b[--i];
}
static int bad(const char *step) { text("WKNET FAIL "); text(step); text(" errno="); decimal((unsigned)errno);text("\n");return 1; }
static int send_all(int h,const void *p,unsigned n) {
    unsigned k=0; u64 deadline=tmc_monotonic_ns()+UINT64_C(10000000000);
    while(k<n) {
        long r=tmc_fd_write(&descriptors,h,(const char *)p+k,n-k);
        if(r>0)k+=(unsigned)r;
        else if(errno!=EAGAIN)return 0;
        if(tmc_monotonic_ns()>deadline)return 0;
    }
    return 1;
}
static int run(void) {
    uint32_t ip=0;
    if(tmc_resolve_ipv4("10.0.2.2",&ip) || ip!=0x0202000a)return bad("IPv4-literal");
    if(tmc_tcp_read(99,buffer,1)!=-1 || errno!=EBADF)return bad("invalid-read");
    struct tmc_pollfd invalid={99,TMC_NET_READABLE,0};
    if(tmc_socket_poll(&invalid,1,0)!=1 || invalid.revents!=TMC_POLL_INVALID)return bad("invalid-poll");
    int h=tmc_fd_connect(&descriptors,"10.0.2.2",18791);
    if(h<0)return bad("stream-connect");
    int alias=tmc_fd_dup(&descriptors,h,10);
    if(alias<10 || tmc_fd_close(&descriptors,h))return bad("socket-dup-close");
    h=alias;
    if(!send_all(h,"STREAM\n",7))return bad("stream-request");
    /* Let the producer fill the 8 KiB receive ring while the application sleeps. */
    slp(100);
    SHA256 sha;sha_init(&sha);
    u64 deadline=tmc_monotonic_ns()+UINT64_C(45000000000);
    unsigned reads=0;
    for(;;) {
        struct tmc_fd_poll p={h,TMC_NET_READABLE,0};
        if(tmc_fd_poll(&descriptors,&p,1,3000)<1)return bad("stream-poll");
        long n=tmc_fd_read(&descriptors,h,buffer,sizeof buffer);
        if(n<0) { if(errno==EAGAIN)continue;return bad("stream-read"); }
        if(!n)break;
        for(long i=0;i<n;i++) {
            unsigned long at=transferred+(unsigned long)i;
            unsigned char expected=(unsigned char)((at*37+(at>>8)*11+17)&255);
            if(buffer[i]!=expected)return bad("payload-mismatch");
        }
        sha_update(&sha,buffer,(unsigned)n);transferred+=(unsigned long)n;reads++;
        if(!(reads%8))slp(2);
        if(tmc_monotonic_ns()>deadline)return bad("stream-deadline");
    }
    if(transferred!=1048893UL)return bad("payload-length");
    struct tmc_fd_poll fin={h,TMC_NET_READABLE|TMC_NET_WRITABLE,0};
    if(tmc_fd_poll(&descriptors,&fin,1,0)!=1)return bad("FIN-poll");
    if(!(fin.revents&TMC_NET_HANGUP))return bad("FIN-status");
    if(!(fin.revents&TMC_NET_WRITABLE))return bad("half-close-writable");
    if(tmc_fd_read(&descriptors,h,buffer,1)!=0)return bad("repeat-EOF");
    unsigned char digest[32];char ack[65];const char hex[]="0123456789abcdef";
    sha_final(&sha,digest);
    for(unsigned i=0;i<32;i++){ack[2*i]=hex[digest[i]>>4];ack[2*i+1]=hex[digest[i]&15];}
    ack[64]='\n';
    if(!send_all(h,ack,sizeof ack))return bad("write-after-FIN");
    if(tmc_fd_close(&descriptors,h))return bad("close");
    if(tmc_fd_read(&descriptors,h,buffer,1)!=-1 || errno!=EBADF)return bad("closed-read");
    text("STREAM PASS bytes=");decimal(transferred);text(" reads=");decimal(reads);text("\nSHA256 ");
    for(unsigned i=0;i<64;i++) { char s[2]={ack[i],0};text(s); }text("\n");
    h=tmc_fd_connect(&descriptors,"10.0.2.2",18791);
    if(h<0 || !send_all(h,"WAIT\n",5))return bad("wait-connect");
    struct tmc_fd_poll p={h,TMC_NET_READABLE,0};
    u64 start=tmc_monotonic_ns();
    if(tmc_fd_poll(&descriptors,&p,1,120)!=0 || p.revents)return bad("timeout-readiness");
    u64 elapsed=tmc_monotonic_ns()-start;
    if(elapsed<UINT64_C(120000000) || elapsed>UINT64_C(2000000000))return bad("timeout-duration");
    if(tmc_fd_read(&descriptors,h,buffer,1)!=-1 || errno!=EAGAIN)return bad("would-block");
    if(tmc_fd_close(&descriptors,h))return bad("wait-close");
    text("WAIT PASS timeout_ms=");decimal(elapsed/1000000);text(" EAGAIN\n");
    h=tmc_fd_connect(&descriptors,"10.0.2.2",18791);
    if(h<0 || !send_all(h,"RESET\n",6))return bad("reset-connect");
    p=(struct tmc_fd_poll){h,TMC_NET_READABLE,0};
    if(tmc_fd_poll(&descriptors,&p,1,3000)<1)return bad("reset-poll");
    long n=tmc_fd_read(&descriptors,h,buffer,1);
    if(n!=-1 || errno!=ECONNRESET)return bad("reset-not-ECONNRESET");
    if(!(p.revents&TMC_NET_ERROR))return bad("reset-status");
    tmc_fd_close(&descriptors,h);text("RESET PASS ECONNRESET not EOF\n");
    int f=tmc_fd_open_readonly(&descriptors,"/System/Library/Fonts/Interface.tfn");
    if(f<0)return bad("file-open");
    alias=tmc_fd_dup(&descriptors,f,12);
    if(alias<12)return bad("file-dup");
    unsigned char prefix[32];
    if(tmc_fd_read(&descriptors,f,prefix,sizeof prefix)!=sizeof prefix)return bad("file-prefix");
    if(tmc_fd_read(&descriptors,alias,buffer,sizeof prefix)!=sizeof prefix)return bad("file-shared-read");
    if(tmc_fd_seek_absolute(&descriptors,alias,sizeof prefix))return bad("file-seek-shared");
    unsigned char check[32];
    if(tmc_fd_read(&descriptors,f,check,sizeof check)!=sizeof check)return bad("file-shared-offset");
    for(unsigned i=0;i<sizeof check;i++)if(check[i]!=buffer[i])return bad("file-dup-offset-mismatch");
    if(tmc_fd_seek_absolute(&descriptors,alias,0) || tmc_fd_close(&descriptors,f))return bad("file-seek-close");
    struct tmc_fd_poll fp={alias,TMC_NET_READABLE,0};
    if(tmc_fd_poll(&descriptors,&fp,1,0)!=1 || fp.revents!=TMC_NET_READABLE)return bad("file-poll");
    if(tmc_fd_write(&descriptors,alias,buffer,1)!=-1 || errno!=EBADF)return bad("readonly-rejection");
    sha_init(&sha);unsigned long file_bytes=0;
    for(;;) {
        long z=tmc_fd_read(&descriptors,alias,buffer,sizeof buffer);
        if(z<0)return bad("file-read");
        if(!file_bytes && z!=8192)return bad("file-syscall-cap");
        if(!z)break;
        file_bytes+=(unsigned long)z;sha_update(&sha,buffer,(unsigned)z);
    }
    sha_final(&sha,digest);
    const char expected_font_hash[]="f8c1b1b5b598d97eedd1765040076f516c8522a7dc8250febd3f914544e6d174";
    for(unsigned i=0;i<32;i++)
        if(hex[digest[i]>>4]!=expected_font_hash[i*2] || hex[digest[i]&15]!=expected_font_hash[i*2+1])return bad("file-SHA256");
    if(file_bytes!=2226987 || tmc_fd_close(&descriptors,alias))return bad("file-length-close");
    text("FILE PASS bytes=");decimal(file_bytes);text(" dup/seek/SHA256\nWKNET PASS\n");
    return 0;
}
void app_main(void) {
 if(tmc_tls_install(&probe_tls)){sc(S_RETURN,42,0,0,0);return;}
    int r=run();
    if(used)sc(S_STDOUT,output,used,0,0);
    sc(S_RETURN,r,0,0,0);
}
