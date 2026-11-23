#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "linux_ehci.h"
static void one(u64 dma,size_t length,unsigned packet,u32 flags)
{
    unsigned char storage[96];memset(storage,0xa5,sizeof storage);
    int n=tmc_linux_ehci_qtd_build(storage,sizeof storage,dma,length,flags,packet);
    size_t limit=5u*4096u-(dma&4095u),expected=length;
    if(expected>limit)expected=limit-limit%packet;
    assert(n>=0&&(size_t)n==expected);
    TmcLinuxEhciQtd q;memcpy(&q,storage,sizeof q);
    assert(!q.next&&!q.alt&&q.token==((unsigned)expected<<16|flags));
    assert(q.buf[0]==(u32)dma&&q.hi[0]==(u32)(dma>>32));
    size_t covered=4096u-(dma&4095u);unsigned used=1;
    while(covered<length&&used<5){
        u64 address=(dma&~4095ull)+used*4096ull;
        assert(q.buf[used]==(u32)address&&q.hi[used]==(u32)(address>>32));
        covered+=4096;used++;
    }
    for(unsigned i=used;i<5;i++)assert(!q.buf[i]&&!q.hi[i]);
    for(unsigned i=sizeof q;i<sizeof storage;i++)assert(storage[i]==0xa5);
}
int main(void)
{
    const size_t lengths[]={0,1,3,8,64,512,4095,4096,8193,16384,20480,65536};
    const unsigned packets[]={1,3,8,64,512,1023,1024};unsigned cases=0;
    const u64 bases[]={0,0xffffe000ull,0x123456780000ull};
    for(unsigned base=0;base<3;base++)for(unsigned offset=0;offset<4096;offset+=7)
     for(unsigned i=0;i<sizeof lengths/sizeof lengths[0];i++)
      for(unsigned j=0;j<sizeof packets/sizeof packets[0];j++){
        one(bases[base]+offset,lengths[i],packets[j],0x80000d80u);cases++;
      }
    unsigned char q[96],before[96];memset(q,0x5a,sizeof q);memcpy(before,q,sizeof q);
    assert(tmc_linux_ehci_qtd_build(0,sizeof q,0,1,0,8)<0);
    assert(tmc_linux_ehci_qtd_build(q,51,0,1,0,8)<0);
    assert(tmc_linux_ehci_qtd_build(q,sizeof q,0,1,0,0)<0);
    assert(tmc_linux_ehci_qtd_build(q,sizeof q,0,1,0,1025)<0);
    assert(tmc_linux_ehci_qtd_build(q,sizeof q,~0ull-4,8,0,8)<0);
    assert(tmc_linux_ehci_qtd_build(q,sizeof q,0,1,1u<<16,8)<0);
    assert(!memcmp(q,before,sizeof q));
    printf("PASS Linux EHCI qtd_fill: %u page/alignment/DMA64/maxpacket cases, zero/status, bounded writes and invalid-input rollback\n",cases);
    return 0;
}
