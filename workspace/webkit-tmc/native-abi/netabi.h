#ifndef TMC_NETABI_H
#define TMC_NETABI_H
/* Additive S_NET operations. Legacy operations 0..7 retain their contracts.
 * Kernel and userspace must share this header. No Linux errno ABI assumption. */
enum { TMC_NET_RECV_EX = 8, TMC_NET_STATUS = 9 };
enum {
    TMC_NET_EBADHANDLE = -1,
    TMC_NET_AGAIN = -2,
    TMC_NET_RESET = -3,
    TMC_NET_IO = -4,
    TMC_NET_UNAVAILABLE = -5
};
/* RECV_EX: >0 payload length; 0 clean FIN after buffered bytes drained;
 * otherwise a negative result above. RESET/IO are never clean EOF. */
enum {
    TMC_NET_READABLE = 1,
    TMC_NET_WRITABLE = 2,
    TMC_NET_HANGUP = 4,
    TMC_NET_ERROR = 8
};
#endif
