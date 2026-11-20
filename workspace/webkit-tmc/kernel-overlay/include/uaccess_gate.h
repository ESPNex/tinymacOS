#pragma once
#include "base.h"
/* Nonblocking syscall reader / address-space writer gate. Failed acquisition
 * must yield/retry; NEVER spin with IRQs disabled behind a preempted reader. */
static inline int uaccess_enter(u32 *state,int writer)
{
    u32 value=__atomic_load_n(state,__ATOMIC_ACQUIRE);
    if(writer){u32 empty=0;return __atomic_compare_exchange_n(state,&empty,0x80000000u,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED);}
    while(!(value&0x80000000u) && value<0x7fffffffu)
        if(__atomic_compare_exchange_n(state,&value,value+1,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return 1;
    return 0;
}
static inline void uaccess_leave(u32 *state,int writer)
{
    if(writer)__atomic_store_n(state,0,__ATOMIC_RELEASE);
    else __atomic_fetch_sub(state,1,__ATOMIC_RELEASE);
}
