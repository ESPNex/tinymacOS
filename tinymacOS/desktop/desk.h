#pragma once
#include "gfx.h"
#include "animation_clock.h"
#define BAR 34
#include "window_chrome.h"
#define TITLE TMC_CHROME_TITLE
#define FOOT 3
#define BG INK(I_BG)
#define TX INK(I_TXT)
#define SUB INK(I_SUB)
#define LN INK(I_LINE)
enum {AN_NONE, AN_OPEN, AN_CLOSE, AN_MIN, AN_RESTORE, AN_ZOOM, AN_MC};
typedef struct {
    int kind, dur, age, frames;
    u64 last, start;
    Rect a, b;
} Motion;
typedef struct {
    Win s;
    int used, mini, max, dead, n, stamp, sw, sh, oldw, oldh, contentAge, contentFx, focusWait;
    u64 key, contentTick, surfaceContent;
    Rect box, saved;
    Motion motion;
    u32 *surface, *previous;
    Draw cmd[NCMDS];
} View;
extern View views[NPROC];
extern u32 *desk_ras;        /* C_RASTER payload fetched with each window */
extern int order[NPROC], nz, dirty, cursorDirty, mx, my, buttons;
// Raw relative reports and absolute framebuffer positions must never be mixed.
static inline void pointer_position(const Event *e) {
    mx=clampi(e->type==EV_POINTER_ABS ? e->a : mx+e->a,0,W-1);
    my=clampi(e->type==EV_POINTER_ABS ? e->b : my+e->b,0,H-1);
}
extern SysInfo sys;
int topwin(void);
int focuswin(int);
void launch(int);
void openarg(int, const char *);
void eventto(int, Event);
void action(int);
void dockcenter(int, int *, int *);
int shell_click(int, int);
int shell_key(Event *);
int shell_step(int);
void shell_pop(int);
void shell_mouse(void);
void shell_control_pointer(int);
int shell_overlay_open(void);
void recovery_background(void);
void recovery_draw(void);
int recovery_click(int,int);
void recovery_bar_draw(void);
int recovery_overlay_open(void);
int recovery_bar_click(int,int);
int recovery_key(Event *);
void shell_base(void);
void shell_draw(void);
void overlay_show(int);
extern int mc;
void mc_enter(void);
void mc_exit(void);
int mchit(int, int);
void togglepref(int);
int ease(int);
int lerpi(int, int, int);
Rect mixrect(Rect, Rect, int);
void startmotion(View *, int, Rect, Rect, int);
int motionstep(View *, Rect *, int *, int *);
void motionfinish(View *);
u64 backdrop_key(Rect);
void wintile(int, Rect);
enum {OV_NONE, OV_SEARCH, OV_CONTROL, OV_LIBRARY, OV_OVERVIEW, OV_MENU, OV_HELP, OV_RESTART, OV_OFF};

extern int gate;
void login_enter(void);
void login_draw(void);
void login_input(Event *);
int login_step(void);
void avatar(int, int, int);
void shell_reset(void);

void shell_context(void);void context_ready(Event*);

int shell_drag_source(int,int,char[PATHLEN]);

int shell_drop_folder(int,int);
