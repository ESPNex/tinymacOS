#include "tmc_descriptors.h"
#include "abi.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned calls,tick,file_at[128],file_closes[128],socket_closes[128];
static int status=0,close_failure=0,native_open=0,native_socket=0;
static unsigned next_status_tick=0;
static long forced_read=-99;
static uint64_t last_length;
long tmc_syscall(long n,uint64_t a,uint64_t b,uint64_t c,uint64_t d)
{
    calls++;
    if(n==S_TICKS)return tick;
    if(n==S_SLEEP){tick+=(unsigned)a;return 0;}
    if(n==S_OPEN){assert(!b);return native_open;}
    if(n==S_READ){
        assert(a<128);last_length=c;
        if(forced_read!=-99)return forced_read;
        unsigned len=100000-file_at[a];if(len>c)len=(unsigned)c;
        for(unsigned i=0;i<len;i++)((char *)(uintptr_t)b)[i]=(char)(file_at[a]+i);
        file_at[a]+=len;return len;
    }
    if(n==S_SEEK){assert(a<128);if(b>100000)return -1;file_at[a]=(unsigned)b;return 0;}
    if(n==S_CLOSE){assert(a<128);file_closes[a]++;return close_failure?-1:0;}
    assert(n==S_NET);
    if(a==4)return native_socket;
    if(a==5){assert(d<128);return c>31?31:(long)c;}
    if(a==7){assert(d<128);socket_closes[d]++;return close_failure?-1:0;}
    if(a==TMC_NET_RECV_EX)return TMC_NET_AGAIN;
    if(a==TMC_NET_STATUS)return tick>=next_status_tick?status:0;
    assert(0);return -1;
}
static void reset(struct tmc_fd_table *t)
{
    tmc_fd_init(t);memset(file_closes,0,sizeof file_closes);memset(socket_closes,0,sizeof socket_closes);
    memset(file_at,0,sizeof file_at);tick=0;status=0;next_status_tick=0;close_failure=0;forced_read=-99;
}
int main(void)
{
    struct tmc_fd_table t;char buf[9000];reset(&t);
    assert(tmc_fd_read(&t,0,buf,1)==-1&&errno==EBADF);
    assert(tmc_fd_adopt(NULL,TMC_FD_FILE,0)==-1&&errno==EINVAL);
    assert(tmc_fd_adopt(&t,7,0)==-1&&errno==EINVAL);
    assert(tmc_fd_adopt(&t,TMC_FD_TCP,-1)==-1&&errno==EINVAL);
    int f=tmc_fd_open_readonly(&t,"/System/test");assert(f==3);
    int s=tmc_fd_connect(&t,"localhost",80);assert(s==4);
    assert(tmc_fd_adopt(&t,TMC_FD_FILE,0)==-1&&errno==EEXIST);
    assert(tmc_fd_adopt(&t,TMC_FD_TCP,0)==-1&&errno==EEXIST);
    int dup=tmc_fd_dup(&t,f,10);assert(dup==10);
    assert(tmc_fd_read(&t,f,buf,7)==7);
    assert(tmc_fd_read(&t,dup,buf,5)==5&&(unsigned char)buf[0]==7&&file_at[0]==12);
    assert(tmc_fd_seek_absolute(&t,f,100)==0);
    assert(tmc_fd_read(&t,dup,buf,2)==2&&(unsigned char)buf[0]==100);
    assert(tmc_fd_seek_absolute(&t,s,0)==-1&&errno==ESPIPE);
    assert(tmc_fd_seek_absolute(&t,dup,100001)==-1&&errno==EINVAL);
    assert(tmc_fd_read(&t,dup,buf,sizeof buf)==8192&&last_length==8192);
    unsigned before=calls;
    assert(tmc_fd_read(&t,dup,NULL,0)==0&&calls==before);
    assert(tmc_fd_read(&t,dup,NULL,2)==-1&&errno==EFAULT&&calls==before);
    assert(tmc_fd_write(&t,f,buf,1)==-1&&errno==EBADF&&calls==before);
    assert(tmc_fd_write(&t,s,buf,40)==31);
    assert(tmc_fd_read(&t,s,buf,2)==-1&&errno==EAGAIN);
    forced_read=9000;assert(tmc_fd_read(&t,f,buf,4)==-1&&errno==EIO);forced_read=-99;
    struct tmc_fd_poll p[]={{f,TMC_NET_READABLE,99},{s,TMC_NET_READABLE,99},{-1,0,99},{63,0,99},{dup,TMC_NET_READABLE,99}};
    assert(tmc_fd_poll(&t,p,5,0)==3);
    assert(p[0].revents==TMC_NET_READABLE&&!p[1].revents&&!p[2].revents&&p[3].revents==TMC_POLL_INVALID&&p[4].revents==TMC_NET_READABLE);
    status=TMC_NET_HANGUP|TMC_NET_ERROR;p[1].events=0;
    assert(tmc_fd_poll(&t,p+1,1,0)==1&&p[1].revents==(TMC_NET_HANGUP|TMC_NET_ERROR));
    status=0;p[1].events=TMC_NET_READABLE;
    assert(tmc_fd_poll(&t,p+1,1,25)==0&&tick==3);
    status=TMC_NET_READABLE;next_status_tick=5;
    assert(tmc_fd_poll(&t,p+1,1,-1)==1&&tick==5);
    status=TMC_NET_EBADHANDLE;
    assert(tmc_fd_poll(&t,p+1,1,0)==1&&p[1].revents==TMC_POLL_INVALID);
    status=TMC_NET_UNAVAILABLE;
    assert(tmc_fd_poll(&t,p+1,1,0)==-1&&errno==ENETDOWN);
    assert(tmc_fd_poll(&t,NULL,0,10)==0&&tick==6);
    assert(tmc_fd_poll(NULL,NULL,0,0)==-1&&errno==EINVAL);
    assert(tmc_fd_poll(&t,NULL,1,0)==-1&&errno==EINVAL);
    assert(tmc_fd_poll(&t,p,65,0)==-1&&errno==EINVAL);
    assert(tmc_fd_poll(&t,p,1,-2)==-1&&errno==EINVAL);
    assert(!tmc_fd_close(&t,f)&&!file_closes[0]);
    assert(tmc_fd_read(&t,f,buf,1)==-1&&errno==EBADF);
    assert(!tmc_fd_close(&t,dup)&&file_closes[0]==1);
    assert(!tmc_fd_close(&t,s)&&socket_closes[0]==1);
    assert(tmc_fd_close(&t,s)==-1&&errno==EBADF);
    /* Filling via dup must not exhaust/double-close native resources. */
    reset(&t);f=tmc_fd_adopt(&t,TMC_FD_FILE,0);assert(f==3);
    for(int i=4;i<64;i++)assert(tmc_fd_dup(&t,f,0)==i);
    before=calls;
    assert(tmc_fd_dup(&t,f,0)==-1&&errno==EMFILE);
    assert(tmc_fd_open_readonly(&t,"/valid")==-1&&errno==EMFILE&&calls==before);
    assert(tmc_fd_connect(&t,"localhost",80)==-1&&errno==EMFILE&&calls==before);
    assert(tmc_fd_adopt(&t,TMC_FD_FILE,1)==-1&&errno==EMFILE&&!file_closes[1]);
    assert(tmc_fd_dup(&t,f,-1)==-1&&errno==EINVAL);
    assert(tmc_fd_dup(&t,f,64)==-1&&errno==EINVAL);
    for(int i=3;i<63;i++)assert(!tmc_fd_close(&t,i)&&!file_closes[0]);
    assert(!tmc_fd_close(&t,63)&&file_closes[0]==1);
    reset(&t);f=tmc_fd_adopt(&t,TMC_FD_TCP,0);close_failure=1;
    assert(tmc_fd_close(&t,f)==-1&&errno==EBADF&&socket_closes[0]==1);
    assert(tmc_fd_close(&t,f)==-1&&socket_closes[0]==1);close_failure=0;
    before=calls;
    assert(tmc_fd_open_readonly(&t,"relative")==-1&&errno==EINVAL&&calls==before);
    memset(buf,'a',PATHLEN);buf[0]='/';buf[PATHLEN]=0;
    assert(tmc_fd_open_readonly(&t,buf)==-1&&errno==ENAMETOOLONG&&calls==before);
    native_open=-1;assert(tmc_fd_open_readonly(&t,"/missing")==-1&&errno==EIO);native_open=0;
    /* Deterministic lifetime stress against independent descriptor/ref model. */
    reset(&t);unsigned model[64]={0},refs[32]={0},expected[32]={0},rng=1234;
    for(unsigned k=0;k<100000;k++) {
        rng=rng*1664525+1013904223;unsigned op=rng%3;
        rng=rng*1664525+1013904223;unsigned fd=3+(rng%61);
        if(op==0) {
            unsigned h=(rng>>12)%32;int want=-1;
            if(!refs[h])for(int i=3;i<64;i++)if(!model[i]){want=i;break;}
            int got=tmc_fd_adopt(&t,TMC_FD_TCP,(int)h);
            assert(got==want);
            if(got>=0){model[got]=h+1;refs[h]++;}
        } else if(op==1) {
            int want=-1;
            if(model[fd])for(int i=3;i<64;i++)if(!model[i]){want=i;break;}
            int got=tmc_fd_dup(&t,(int)fd,0);assert(got==want);
            if(got>=0){model[got]=model[fd];refs[model[fd]-1]++;}
        } else {
            int got=tmc_fd_close(&t,(int)fd);assert(got==(model[fd]?0:-1));
            if(model[fd]){unsigned h=model[fd]-1;model[fd]=0;if(!--refs[h])expected[h]++;}
        }
        for(unsigned h=0;h<32;h++)assert(socket_closes[h]==expected[h]);
    }
    for(int i=3;i<64;i++)if(model[i]){unsigned h=model[i]-1;assert(!tmc_fd_close(&t,i));if(!--refs[h])expected[h]++;}
    for(unsigned h=0;h<32;h++)assert(!refs[h]&&socket_closes[h]==expected[h]);
    puts("PASS: unified read-only-file/TCP descriptors, dup/shared offsets, last-close ownership, poll, caps, invalid/error paths and 100000 lifetime operations.");
}
