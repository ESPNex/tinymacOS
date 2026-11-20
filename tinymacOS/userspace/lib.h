#pragma once
#include "abi.h"
#include "text_input.h"
static inline long call(long n, u64 a, u64 b, u64 c, u64 d) {
    register u64 r10 asm("r10")=d;
    asm volatile("syscall":"+a"(n):"D"(a), "S"(b), "d"(c), "r"(r10):"rcx", "r11", "memory", "cc");
    return n;
}
#define sc(n,a,b,c,d) call(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#define slp(t) sc(S_SLEEP,t,0,0,0)
#ifdef TMC_DEBUG
#define trace(s) sc(S_TRACE, (u64)(s), 0, 0, 0)
#else
#define trace(...) ((void)0)
#endif
