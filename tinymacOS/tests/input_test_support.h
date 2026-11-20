#pragma once
#include "abi.h"
#include "inputdiag.h"
#include <assert.h>
#include <stdio.h>
typedef int Spin;
static inline void spin_lock(Spin *s,u32 *f){(void)s;*f=0;}
static inline void spin_unlock(Spin *s,u32 f){(void)s;(void)f;}
extern u64 ticks;
extern InputDiag input_diag;
static struct {int w,h;} boot={1280,800};
static inline void logstr(const char *s){(void)s;}
static inline void lognum(u64 n){(void)n;}
static inline void device_add(const char *a,const char *b,int c){(void)a;(void)b;(void)c;}
static inline u32 pci_find_after(u32 a,u32 b){(void)a;(void)b;return 0;}
static inline void *pci_mmio(u32 a,int b){(void)a;(void)b;return 0;}
static inline u64 page_get(void){return 0;}
void hid_keys(const u8 [8]);
void hid_relative(int,int,int,int);
void hid_absolute(int,int,int,int);
void input_repeat(void);
int input_led_take(void);

static inline void ps2_init(void){}
static inline void ps2_poll(void){}
static inline void ehci_init(void){}
static inline void ehci_poll(void){}
static inline void uhci_init(void){}
static inline void uhci_poll(void){}

static inline void ps2_setleds(int value){(void)value;}
