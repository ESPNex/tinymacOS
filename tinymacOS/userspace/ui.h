#pragma once
#include "lib.h"
extern int aw, ah;
extern SysInfo si;
extern char wintitle[48];
#define BG INK(I_BG)
#define PANEL INK(I_PANEL)
#define TX INK(I_TXT)
#define SUB INK(I_SUB)
#define EDGE INK(I_LINE)
#define ACC INK(I_ACC)
#define SEL INK(I_SEL)
#define EMP INK(I_ACC_TXT)
#define CARD INK(I_CARD)
#define SOFT INK(I_SOFT)
void win(int, int, const char *);
void begin(void);
void finish(void);
void Box(int, int, int, int, int, u32);
void Text(int, int, const char *, int, u32);
int TextWidth(const char *, int);
void Center(int, int, const char *, int, u32);
void Icon(int, int, int, int);
void Line(int, int, int, int, u32);
void button(int, int, int, const char *, int);
void modal(const char *, const char *, const char *);
int baseevent(Event *);
void quit(void);
void addnum(char *, u64, int);
int absolutepath(char *, const char *, const char *);
void parentpath(char *);
const char *leaf(const char *);
int readfile(const char *, char *, int);
int writefile(const char *, const char *, int);
int copyfile(const char *, const char *);
void title(const char *, const char *);
void ui_motion(int);
void Photo(int, int, int, int, int, int);
void Sheet(int, int, int, int, int);
void DialogSheet(int, int, int, int, int);
void Keyface(int, int, int, int, int);
