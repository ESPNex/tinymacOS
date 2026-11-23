/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <linux/tmc_ps2_compat.h>
/* The original libps2 expects IRQ-driven RX while waiting in process
 * context. This kernel's i8042 has exactly one CPU0-polled consumer instead.
 * Each wait services native RX only after upstream has continued/unlocked
 * serio RX, and uses actual monotonic timeout, not fake ACKs or success.
 * Other Linux users retain their ordinary URB/anchor wait implementation.
 */
#undef wait_event_timeout
#define wait_event_timeout(q,condition,timeout) ({u64 _ms=(timeout),_start=tmc_lx_os_clock_ms(),_now=_start;long _left=0;for(;;){if(condition){_left=_now-_start<_ms?(long)(_ms-(_now-_start)):1;break;}if(_now-_start>=_ms)break;(void)__atomic_load_n(&(q).sequence,__ATOMIC_ACQUIRE);tmc_lx_ps2_wait_progress();_now=tmc_lx_os_clock_ms();}_left;})
#include "../../vendor/linux-v6.12/libps2.c"
