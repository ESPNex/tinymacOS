#pragma once
#include "screen.h"
static inline u8 inb(u16 p){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void outb(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline void waitio(void){__asm__ volatile("pause");}
static u64 ticks;
static inline void device_add(const char *a,const char *b,int c){(void)a;(void)b;(void)c;}
static inline void hid_relative(int x,int y,int b,int w){(void)x;(void)y;(void)b;(void)w;}
static void input_key_usage(int,int);
