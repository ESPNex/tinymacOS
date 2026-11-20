#include "tmc_backend.h"
#include "abi.h"
#include <errno.h>

int tmc_socket_poll(struct tmc_pollfd *items, size_t count, int timeout_ms)
{
    if (count > 8 || (count && !items) || timeout_ms < -1) { errno = EINVAL; return -1; }
    uint64_t start = tmc_monotonic_ns();
    uint64_t limit = timeout_ms < 0 ? 0 : (uint64_t)timeout_ms * UINT64_C(1000000);
    for (;;) {
        int ready_count = 0;
        for (size_t i = 0; i < count; ++i) {
            items[i].revents = 0;
            if (items[i].handle < 0) continue;
            int status = tmc_tcp_status(items[i].handle);
            if (status < 0) {
                if (errno != EBADF) return -1;
                items[i].revents = TMC_POLL_INVALID;
            } else {
                unsigned wanted = items[i].events & (TMC_NET_READABLE | TMC_NET_WRITABLE);
                items[i].revents = (unsigned)status & (wanted | TMC_NET_ERROR | TMC_NET_HANGUP);
            }
            if (items[i].revents) ++ready_count;
        }
        if (ready_count) return ready_count;
        if (timeout_ms >= 0 && tmc_monotonic_ns() - start >= limit) return 0;
        /* Yield for one 10ms tick. No fake readiness and no busy-spin loop.
         * Interruptible waits/cancellation require a later kernel ABI addition. */
        if (tmc_syscall(S_SLEEP, 1, 0, 0, 0) < 0) { errno = EIO; return -1; }
    }
}
