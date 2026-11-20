#pragma once

/* Kernel kernel/memory.c owns mem_init() in the shared ELF namespace;
 * keep lwIP's mem manager symbols distinct. */
#define mem_init lwip_mem_init
#define mem_calloc lwip_mem_calloc
#define mem_malloc lwip_mem_malloc
#define mem_free lwip_mem_free
#define mem_trim lwip_mem_trim
#define mem_size lwip_mem_size
/* lwIP options for the tinymacOS in-kernel IPv4 stack (NO OS layer). */
#define NO_SYS 1
#define NO_SYS_NO_STDLIB 1
#define LWIP_NETCONN 0
#define LWIP_SOCKET 0
#define LWIP_COMPAT_SOCKETS 0
#define LWIP_ETHERNET 1
#define LWIP_ARP 1
#define LWIP_IPV4 1
#define LWIP_IPV6 0
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_DHCP 1
#define LWIP_AUTOIP 0
#define LWIP_DNS 0           /* resolution lives in kernel/net.c (zone-aware) */
#define LWIP_STATS 0
#define LWIP_NETIF_API 0
#define LWIP_NETIF_LOOP 0
#define LWIP_HAVE_LOWPDRIVER 0
#define LWIP_TCP_KEEPALIVE 0
#define IP_REASSEMBLY 0
#define IP_FRAG 0
#define ARP_QUEUEING 0
#define ETH_PAD_SIZE 0
#define MEM_ALIGNMENT 4
#define MEM_SIZE (48 * 1024)
#define MEM_LIBC_MALLOC 0
#define MEMP_MEM_MALLOC 0
#define MEMP_NUM_ARP_QUEUE 8
#define MEMP_NUM_NETCONN 4
#define MEMP_NUM_RAW_PCB 4
#define MEMP_NUM_UDP_PCB 6
#define MEMP_NUM_TCP_PCB 8
#define MEMP_NUM_TCP_SEG 32
#define PBUF_POOL_SIZE 64
#define PBUF_POOL_BUFSIZE 1600
#define LWIP_TCP_SND_BUF_DEFAULT (8 * 1024)
#define LWIP_RCV_WND_DEFAULT (8 * 1024)
#define TCP_WND (8 * 1024)
#define TCP_SND_QUEUELEN 8
#define TCP_MSS 1460
#define LWIP_SINGLE_NETIF 0
#define LWIP_TIMEVAL_PRIVATE 0
#define LWIP_PLATFORM_BYTESWAP 0
#define LWIP_CHECKSUM_ON_COPY 0
#define LWIP_NETIF_HOSTNAME 0
#define LWIP_DHCP_MAX_NTP_SERVERS 0
#define LWIP_DHCP_DOES_ACD_CHECK 0
#define DHCP_DOES_CHECK_ARP 0
#define LWIP_STATS 0
