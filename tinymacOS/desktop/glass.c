#include "gfx.h"
/* Capture the actual canvas before drawing any pixel of this material. Quarter-scale
   separable [1 4 6 4 1] convolution, bilinear reconstruction and a rounded lens
   normal with wavelength-dependent edge displacement. No GPU or host UI calls. */
#define SW 512   /* quarter-scale capture of up to 1920px + 20px halo */
#define SH 288   /* up to 1080px + halo */
static u32 raw[SW * SH], pass[SW * SH], blur[SW * SH];
/* Modal sheets sample the actual scene beneath their parent window, not the
 * opaque document/sidebar that the client has just painted over that scene. */
static u32 sheet_underlay[1920*1080];
static int sheet_valid;
void glass_sheet_backdrop(Rect q,int enabled)
{
    sheet_valid=enabled;
    if(!enabled)return;
    int l=MAX(0,q.x-24),t=MAX(0,q.y-24),rr=MIN(W,q.x+q.w+24),bb=MIN(H,q.y+q.h+24);
    if(l>=rr||t>=bb){sheet_valid=0;return;}
    for(int y=t;y<bb;y++)memcpy(sheet_underlay+y*W+l,canvas+y*W+l,(rr-l)*sizeof(u32));
}
static u8 roots[8400];
static int ready;
static void initroot(void)
{
    if (ready)return;
    int k = 0;
    for (int n = 0; n < 8400; n++) {
        while ((k + 1) * (k + 1) <= n)k++;
        roots[n] = k;
    }
    ready = 1;
}
static inline u32 kernel5(u32 a, u32 b, u32 c, u32 d, u32 e)
{
    u32 rb = (a & 0xff00ff) + (e & 0xff00ff) + 4 * ((b & 0xff00ff) + (d & 0xff00ff)) + 6 * (c & 0xff00ff);
    u32 g = (a & 0xff00) + (e & 0xff00) + 4 * ((b & 0xff00) + (d & 0xff00)) + 6 * (c & 0xff00);
    return ((rb >> 4) & 0xff00ff) | ((g >> 4) & 0xff00);
}
static inline u32 sample(const u32 *v, int x, int y)
{
    int i = (y >> 2) * SW + (x >> 2), fx = (x & 3) * 64, fy = (y & 3) * 64;
    return blend(blend(v[i], v[i + 1], fx), blend(v[i + SW], v[i + SW + 1], fx), fy);
}
static int dist(int i, int j, int w, int h, int r, int *nx, int *ny)
{
    int a = MIN(i, w - 1 - i), b = MIN(j, h - 1 - j), sx = i < w / 2 ? -1 : 1, sy = j < h / 2 ? -1 : 1;
    *nx = *ny = 0;
    if (a >= r || b >= r) {
        if (a < b)*nx = sx * 16;
        else *ny = sy * 16;
        return MIN(a, b);
    }
    int xx = r - a, yy = r - b, l = MAX(1, roots[xx * xx + yy * yy]);
    *nx = sx * xx * 16 / l;
    *ny = sy * yy * 16 / l;
    return r - l;
}
void stroke(Rect q, int r, u32 ink, int alpha)
{
    r = clampi(r, 1, MIN(62, MIN(q.w, q.h) / 2));
    if (q.w < 3 || q.h < 3)return;
    u32 c = color(ink);
    for (int j = MAX(0, y1 - q.y); j < MIN(q.h, y2 - q.y); j++) {
        int start = MAX(0, x1 - q.x), end = MIN(q.w, x2 - q.x);
        for (int i = start; i < end;) {
            int a = coverage(i, j, q.w, q.h, r) - coverage(i - 1, j - 1, q.w - 2, q.h - 2, r - 1);
            if (a > 0) {
                u32 *d = canvas + (q.y + j) * W + q.x + i;
                *d = blend(*d, c, (a * alpha) >> 8);
            }
            if (j > 1 && j < q.h - 2 && i == r + 1 && q.w - r - 2 > i)i = q.w - r - 2;
            else i++;
        }
    }
}
void shadow(Rect q, int r, int strength)
{
    initroot();
    int pad = 16;
    Rect b = {q.x - pad, q.y - pad + 5, q.w + 2 * pad, q.h + 2 * pad};
    for (int Y = MAX(y1, b.y); Y < MIN(y2, b.y + b.h); Y++) {
        int dy = MAX(0, MAX(q.y + 5 + r - Y, Y - (q.y + 5 + q.h - r))), iy = Y - q.y;
        int l = MAX(x1, b.x), e = MIN(x2, b.x + b.w);
        for (int X = l; X < e; X++) {
            if (iy >= r && iy < q.h - r && X >= q.x + r && X < q.x + q.w - r) {
                X = q.x + q.w - r - 1;
                continue;
            }
            int dx = MAX(0, MAX(q.x + r - X, X - (q.x + q.w - r))), sum = dx * dx + dy * dy;
            int d = sum < 8400 ? (int)roots[sum] - r : pad;
            if (d < pad) {
                int v = pad - MAX(0, d);
                u32 *p = canvas + Y * W + X;
                *p = blend(*p, 0x0b1730, (strength * v * v) >> 8);
            }
        }
    }
}
void glass(Rect q, int r, int kind)
{
    if (q.w < 2 || q.h < 2)return;
    /* Recovery has no Liquid Glass: no capture, blur, refraction, shine or
     * backdrop blending. This applies equally to native app windows/dialogs. */
    if(recovery_mode){roundr(q.x,q.y,q.w,q.h,r,INK(I_PANEL),255);return;}
    /* Desktop menu bar: leave the actual backdrop untouched, including when
     * Reduce Transparency is enabled. Recovery stays on the opaque path above. */
    if(kind==MAT_BAR)return;
    initroot();
    r = clampi(r, 0, MIN(60, MIN(q.w, q.h) / 2));
    int sheet=kind==MAT_SHEET,flat=kind==MAT_LIBRARY;
    kind=MAT_DOCK; /* one Control Center material for every Liquid Glass surface */
    int dark = night;
    u32 tint = dark ? (kind == MAT_WIDGET ? 0x34363d : 0x28282c) : (kind == MAT_DOCK ? 0xe1e1e8 : 0xf5f5f7);
    if(kind!=MAT_BAR&&kind!=MAT_WIDGET)tint=color(INK(kind==MAT_DOCK?I_PANEL:I_BG));
    int strength = kind == MAT_WINDOW ? (dark ? 204 : 211) : kind == MAT_MENU ? (dark ? 203 : 213) :
                   kind == MAT_SHEET ? (dark ? 154 : 148) : kind == MAT_WIDGET ? 143 : kind == MAT_BAR ? 97 : kind==MAT_DOCK ? (dark?152:145) : 63;
    if (lite) {
        roundr(q.x, q.y, q.w, q.h, r, kind==MAT_BAR||kind==MAT_WIDGET?0x303035:color(INK(kind==MAT_DOCK?I_PANEL:I_BG)), 255);
        if (kind != MAT_BAR)stroke(q, r, dark ? 0xa5a5aa : 0xffffff, 112);
        return;
    }
// Clip potentially large client-provided draw commands to the render target.
    int L = MAX(q.x, x1), T = MAX(q.y, y1), R = MIN(q.x + q.w, x2), B = MIN(q.y + q.h, y2);
    if (L >= R || T >= B)return;
    int left = L - 20, top = T - 20, nx = (R - L + 40 + 3) / 4 + 2, ny = (B - T + 40 + 3) / 4 + 2;
    if (nx > SW || ny > SH)return;
    const u32 *source=sheet&&sheet_valid?sheet_underlay:canvas;
    for (int y = 0; y < ny; y++) {
        int sy = clampi(top + y * 4, 0, H - 1), sy2 = MIN(H - 1, sy + 2);
        for (int x = 0; x < nx; x++) {
            int sx = clampi(left + x * 4, 0, W - 1), sx2 = MIN(W - 1, sx + 2);
            raw[y * SW + x] = blend(blend(source[sy * W + sx], source[sy * W + sx2], 128), blend(source[sy2 * W + sx],
                                    source[sy2 * W + sx2], 128), 128);
        }
    }
    for (int y = 0; y < ny; y++)for (int x = 2; x < nx - 2; x++) {
            const u32 *p = raw + y * SW + x;
            pass[y * SW + x] = kernel5(p[-2], p[-1], p[0], p[1], p[2]);
        }
    for (int y = 2; y < ny - 2; y++)for (int x = 2; x < nx - 2; x++) {
            const u32 *p = pass + y * SW + x;
            blur[y * SW + x] = kernel5(p[-2 * SW], p[-SW], *p, p[SW], p[2 * SW]);
        }
    for (int Y = T; Y < B; Y++) {
        int j = Y - q.y, sy = Y - top;
        u32 *p = canvas + Y * W + L;
        int rowShade = clampi(strength + (q.h / 2 - j) * 16 / MAX(1, q.h), 0, 246);
        for (int X = L; X < R; X++, p++) {
            int i = X - q.x, sx = X - left, ex = MIN(i, q.w - 1 - i), ey = MIN(j, q.h - 1 - j);
            /* Library retains real backdrop blur/tint but has no lens bevel,
             * displaced edge samples, dark inset rim or specular border. */
            if(flat){
                int mask=coverage(i,j,q.w,q.h,r);
                if(mask){u32 col=blend(sample(blur,sx,sy),tint,rowShade);*p=mask==255?col:blend(*p,col,mask);}
                continue;
            }
            if (kind == MAT_BAR || (kind!=MAT_DOCK && kind!=MAT_SHEET && ex >= r + 7 && ey >= 7)) {
                *p = blend(sample(blur, sx, sy), tint, rowShade);
                continue;
            }
            /* Interior: full coverage, zero refraction, no rim/shine. This is
             * algebraically the same material, without per-pixel divisions,
             * normal calculation, coverage lookup and clamping. */
            if (ex >= MAX(r, 18) && ey >= MAX(r, 18)) {
                *p = blend(sample(blur, sx, sy), tint, clampi(rowShade, 16, 245));
                continue;
            }
            int dx, dy, d = dist(i, j, q.w, q.h, r, &dx, &dy);
            int mask = coverage(i, j, q.w, q.h, r);
            if (!mask)continue;
            int rim = clampi(11 - d, 0, 11), off = (rim * (15 - rim)) >> 2;
            if(kind==MAT_DOCK||kind==MAT_SHEET){int depth=MIN(MAX(0,d),18);off+=((18-depth)*3)/8;}
            int rx = sx + (dx * off >> 4), ry = sy + (dy * off >> 4);
            rx = clampi(rx, 8, (nx - 4) * 4);
            ry = clampi(ry, 8, (ny - 4) * 4);
            u32 base = sample(blur, rx, ry);
            if (rim) {
                u32 sharp = sample(raw, rx, ry);
                u32 rr = sample(raw, rx + (dx >> 3), ry + (dy >> 3)), bb = sample(raw, rx - (dx >> 3), ry - (dy >> 3));
                sharp = (rr & 0xff0000) | (sharp & 0xff00) | (bb & 255);
                base = blend(base, sharp, rim * 12);
            }
            u32 col = blend(base, tint, clampi(rowShade - rim * 4, 16, 245));
            if (d < 4) {
                int shine = (d <= 0 ? 106 : d == 1 ? 45 : d == 2 ? 14 : 5);
                int near = clampi(152 - (X - lightx < 0 ? lightx - X : X - lightx) / 3, 0, 128);
                int facing = clampi(130 - 3 * dy - 2 * dx + near, 35, 255);
                col = blend(col, dy > 6 ? 0xd5e4fc : 0xffffff, shine * facing / 255);
            } else if (d < 9)col = blend(col, 0x263d67, (9 - d) * (dark ? 1 : 1));
            *p = mask == 255 ? col : blend(*p, col, mask);
        }
    }
}
