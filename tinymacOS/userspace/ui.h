#pragma once
#include "lib.h"
#include "home_name.h"
/* Query the real inode name; a failed/colliding migration keeps the old home. */
static inline void ui_home(char out[PATHLEN]){Dirent d={0};strset(out,HOME,PATHLEN);if(sc(S_STAT,HOME,&d,0,0)>=0&&d.dir&&home_name_valid(d.name)){strset(out,"/Users/",PATHLEN);append(out,d.name,PATHLEN);}}
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
/* Locked production F_MONO advances are 8px; compact glyph references avoid UTF-8 truncation. */
/* Focus and blink are composited centrally, without republishing app frames. */
void InputText(int, int, int, const char *, int, int, int, u32);
void inputmodal_at(const char *,const char *,const char *,int,int);
void inputmodal(const char *, const char *, const char *, int);
void Caret(int, int, int, int, u32);
void MonoText(int, int, const char *, int, u32);
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
