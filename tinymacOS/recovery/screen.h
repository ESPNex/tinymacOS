#pragma once
#include <efi.h>
#include <efilib.h>
#include "disklayout.h"
extern int rw, rh;
int screen_init(EFI_SYSTEM_TABLE *);
void fill(int, int, int, int, int, u32);
void label(int, int, int, const char *, u32);
void glyph(int, int, int, int);
void show(int, int);
int tw(int, const char *);

void window_frame(int,int,int,int);

void menu_apple(int,int);
