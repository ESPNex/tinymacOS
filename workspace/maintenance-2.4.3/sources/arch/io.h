#pragma once
#include "base.h"
static inline void outb(u16 p, u8 v) {
    asm volatile("outb %0,%1"::"a"(v), "Nd"(p));
}
static inline u8 inb(u16 p) {
    u8 v;
    asm volatile("inb %1,%0":"=a"(v):"Nd"(p));
    return v;
}
static inline void outw(u16 p, u16 v) {
    asm volatile("outw %0,%1"::"a"(v), "Nd"(p));
}
static inline u16 inw(u16 p) {
    u16 v;
    asm volatile("inw %1,%0":"=a"(v):"Nd"(p));
    return v;
}
static inline u32 inl(u16 p) {
    u32 v;
    asm volatile("inl %1,%0":"=a"(v):"Nd"(p));
    return v;
}
static inline void outl(u16 p, u32 v) {
    asm volatile("outl %0,%1"::"a"(v), "Nd"(p));
}
static inline void waitio(void) {
    outb(0x80, 0);
}
static inline u64 rdmsr(u32 i) {
    u32 l, h;
    asm volatile("rdmsr":"=a"(l), "=d"(h):"c"(i));
    return l|((u64)h<<32);
}
static inline void wrmsr(u32 i, u64 v) {
    asm volatile("wrmsr"::"c"(i), "a"((u32)v), "d"((u32)(v>>32)));
}
static inline void cpuid(u32 i, u32 *r) {
    asm volatile("cpuid":"=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]):"a"(i), "c"(0));
}
