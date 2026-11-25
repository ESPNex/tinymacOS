#pragma once
#include "base.h"
#include <sched.h>
typedef struct {volatile u32 v;} Spin;
static inline void spin_lock(Spin*s,u32*f){*f=0;while(__sync_lock_test_and_set(&s->v,1))sched_yield();}
static inline void spin_unlock(Spin*s,u32 f){(void)f;__sync_lock_release(&s->v);}
