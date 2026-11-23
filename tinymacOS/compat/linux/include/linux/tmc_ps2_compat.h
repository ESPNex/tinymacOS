/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <linux/tmc_compat.h>
/* Only libps2's serio/command services, not Linux's device bus or i8042 HCD.
 * Runtime is CPU0-polled. Native command mutexes serialize whole sequences;
 * RX locks retain the caller's IRQ state across pause/continue pairs.
 */
#define BIT(n) (1UL<<(n))
#define EAGAIN 11
#define EPROTO 71
#define EXPORT_SYMBOL(name)
#define __releases(x)
#define __acquires(x)
#define WARN_ON(condition) WARN_ONCE(condition,"PS/2 protocol invariant\n")
#define lockdep_assert_held(lock) do {WARN_ON(!__atomic_load_n(&(lock)->word,__ATOMIC_RELAXED));}while(0)
/* Optional lockdep subclass metadata, not an omitted mutual-exclusion op. */
#define lockdep_set_subclass(lock,depth) do {(void)(lock);(void)(depth);}while(0)
#define SERIO_TIMEOUT 1
#define SERIO_PARITY 2
#define IRQ_HANDLED 1
typedef int irqreturn_t;
struct mutex {unsigned word;};
static inline void mutex_init(struct mutex *m){__atomic_store_n(&m->word,0,__ATOMIC_RELAXED);}
static inline void mutex_lock(struct mutex *m)
{
 while(__atomic_exchange_n(&m->word,1,__ATOMIC_ACQUIRE))tmc_lx_os_spin_wait();
}
static inline void mutex_unlock(struct mutex *m){__atomic_store_n(&m->word,0,__ATOMIC_RELEASE);}
static inline void init_waitqueue_head(wait_queue_head_t *w){__atomic_store_n(&w->sequence,0,__ATOMIC_RELAXED);}
static inline void kmsan_unpoison_memory(const void *p,size_t n){(void)p;(void)n;} /* optional CONFIG_KMSAN off */
void *tmc_lx_memchr(const void *,int,size_t);
#define memchr tmc_lx_memchr
struct serio {
 struct device dev;void *port_data,*driver_data;unsigned depth;
 struct mutex *ps2_cmd_mutex;spinlock_t lock;unsigned long rx_flags;
 int (*write)(struct serio *,unsigned char);
};
static inline void *serio_get_drvdata(struct serio *s){return s->driver_data;}
static inline void serio_set_drvdata(struct serio *s,void *p){s->driver_data=p;}
static inline int serio_write(struct serio *s,u8 value){return s->write?s->write(s,value):-ENODEV;}
static inline void serio_pause_rx(struct serio *s){s->rx_flags=tmc_lx_spin_lock_irqsave(&s->lock);}
static inline void serio_continue_rx(struct serio *s){tmc_lx_spin_unlock_irqrestore(&s->lock,s->rx_flags);}
void tmc_lx_ps2_wait_progress(void);
u32 tmc_lx_os_irq_save(void);
void tmc_lx_os_irq_restore(u32);
