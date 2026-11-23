/* SPDX-License-Identifier: GPL-2.0-or-later
 * Linux EHCI queue DMA primitive: qtd_fill, David Brownell, 2001-2004.
 * Complete exact upstream function is included from pinned Linux v6.12.
 * The validation/ABI adapter and counters are tinymacOS additions.
 */
#include "linux_ehci.h"
typedef u64 dma_addr_t;
/* Only descriptor endianness is consumed by this upstream function. These
 * are private adapter types, not fake general Linux HCD/core definitions.
 */
struct ehci_hcd {unsigned big_endian_desc;};
struct ehci_qtd {
    u32 hw_next,hw_alt_next,hw_token,hw_buf[5],hw_buf_hi[5];
    size_t length;
};
static inline u32 cpu_to_hc32(const struct ehci_hcd *h,u32 value)
{
    return h->big_endian_desc?__builtin_bswap32(value):value;
}
#define likely(x) __builtin_expect(!!(x),1)
static unsigned int qtd_fill(struct ehci_hcd *,struct ehci_qtd *,dma_addr_t,size_t,int,int) __attribute__((noinline,noclone));
#include "../vendor/linux-v6.12/ehci-qtd-fill.inc"
_Static_assert(sizeof(TmcLinuxEhciQtd)==52,"EHCI hardware qTD prefix");
_Static_assert(offsetof(struct ehci_qtd,hw_buf_hi)==32,"EHCI extended buffer pointers");
TmcLinuxEhciQtdStats tmc_linux_ehci_qtd_stats;
int tmc_linux_ehci_qtd_build(void *destination,size_t bytes,u64 dma,size_t n,u32 token,unsigned maxpacket)
{
    if(!destination||bytes<sizeof(TmcLinuxEhciQtd)||!maxpacket||maxpacket>1024||
       (token&0x7fff0000u)||n>~(u64)0-dma){
        __atomic_add_fetch(&tmc_linux_ehci_qtd_stats.rejected,1,__ATOMIC_RELAXED);
        return -22;
    }
    struct ehci_hcd h={0}; /* x86 EHCI descriptors are little-endian. */
    struct ehci_qtd qtd={0};
    unsigned count=qtd_fill(&h,&qtd,dma,n,(int)token,(int)maxpacket);
    TmcLinuxEhciQtd out={0};
    out.token=qtd.hw_token;
    memcpy(out.buf,qtd.hw_buf,sizeof(out.buf));
    memcpy(out.hi,qtd.hw_buf_hi,sizeof(out.hi));
    memcpy(destination,&out,sizeof(out));
    __atomic_add_fetch(&tmc_linux_ehci_qtd_stats.calls,1,__ATOMIC_RELAXED);
    __atomic_add_fetch(&tmc_linux_ehci_qtd_stats.bytes,count,__ATOMIC_RELAXED);
    return (int)count;
}
