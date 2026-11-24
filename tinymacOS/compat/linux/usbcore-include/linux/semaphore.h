#pragma once
#include <tmc_kernel.h>
struct semaphore {unsigned count;};
#define DEFINE_SEMAPHORE(n,c) struct semaphore n={c}
static inline void sema_init(struct semaphore *s,unsigned n){s->count=n;}
void down(struct semaphore *);int down_interruptible(struct semaphore *);int down_trylock(struct semaphore *);void up(struct semaphore *);
