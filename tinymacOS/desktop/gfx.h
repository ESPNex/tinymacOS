#pragma once
#include "lib.h"
typedef struct {
    int x, y, w, h;
} Rect;
static inline int clampi(int n, int lo, int hi) {
    return MAX(lo, MIN(hi, n));
}
static inline int inside(int x, int y, Rect q) {
    return x >= q.x && x < q.x + q.w && y >= q.y && y < q.y + q.h;
}
static inline u32 blend(u32 a, u32 b, int t) {
    u32 z = 256 - t;
    return (((((a & 0xff00ff) * z + (b & 0xff00ff) * t) >> 8) & 0xff00ff) | ((((a & 0xff00) * z +
            (b & 0xff00) * t) >> 8) & 0xff00));
}
enum {MAT_WINDOW, MAT_BAR, MAT_DOCK, MAT_MENU, MAT_WIDGET, MAT_SHEET, MAT_DIALOG = MAT_DOCK};
extern int W, H, PW, PH, night, lite, lightx, lighty;
extern u32 canvas[], back[];
extern int x1, y1, x2, y2;
void initgfx(int, int);
void background(int, int, int);
void restorebg(void);
void clip(int, int, int, int);
void resetclip(void);
void rect(int, int, int, int, u32);
void roundr(int, int, int, int, int, u32, int);
void line(int, int, int, int, u32);
void icon(int, int, int, int);
int textw(const char *, int);
void text(int, int, const char *, int, u32);
void center(int, int, const char *, int, u32);
void client(int, int, int, int, Draw *, int);
u32 color(u32);
u32 mix(u32, u32, int);
void photo(Rect, int, int);
int coverage(int, int, int, int, int);
void shadow(Rect, int, int);
void glass(Rect, int, int);
void glass_sheet_backdrop(Rect, int);
void stroke(Rect, int, u32, int);
void copyout(Rect, u32 *);
void copyin(Rect, const u32 *);
void sprite(const u32 *, int, int, Rect, int, int, int, int);
void button_face(Rect, int);

void alpha_symbol(const u8 *,int,Rect,u32);
