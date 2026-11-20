#pragma once
#include "input_test_support.h"
static unsigned ps2_io_writes;
static u8 ps2_fake_status=255,ps2_last_byte;
static inline u8 inb(u16 port){(void)port;return ps2_fake_status;}
static inline void outb(u16 port,u8 value){(void)port;ps2_last_byte=value;ps2_io_writes++;}
static inline void waitio(void){}
