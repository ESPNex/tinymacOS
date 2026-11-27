#include "gfx.h"
#include "mono_wire.h"
extern const u8 icons[];static u8 fontdata[3*1024*1024],compressed[4*1024*1024];
static int asset(int id,u8*b,u32 cap){long n=sc(S_ART,id,0,0,0);if(n<=0||n>cap)return -1;for(int i=0;i<n;i+=65536){int z=MIN(n-i,65536);if(sc(S_ART,id,i,b+i,z)!=z)return -1;}return n;}
int W, H, night, recovery_mode;
/* Physical scanout size vs logical render size: above 1280 wide the desktop
 * renders a 1280-wide logical canvas that is scaled on present, so widgets and
 * text keep the SAME on-screen size (constant readable DPI) in every mode. */
int PW, PH;
u32 canvas[1920 * 1080], back[1920 * 1080];
int x1, y1, x2, y2, lite, lightx, lighty;
static const u8 *bits;
typedef struct PACK {
    u16 adv, w;
    u32 off;
} Glyph;
static struct {
    u32 h;
    const Glyph *g;
} font[NFONTS];
static const u32 *symbols;
static int ng;
static u32 films[NWALL][1280 * 800];
static int loaded[NWALL];
// Every accent carries BOTH a light and a dark variant (mode chosen by the `dark`
// pref, never by the theme index): I_BG I_PANEL I_TXT I_SUB I_LINE I_ACC I_SEL I_ACC_TXT I_CARD I_SOFT.
#include "system_palette.h"
static int palette;
u32 color(u32 i)
{
    if(recovery_mode && (i&0x80000000u)){
        static const u32 neutral[]={0x242424,0x2d2d2d,0xf2f2f2,0xb8b8bc,0x505054,0x8ab4f8,0x3b4d67,0xffffff,0x333336,0x29292c};
        return neutral[(i&255)<10?(i&255):0];
    }
    return tmc_system_color((u32)palette, (u32)night, i);
}
u32 mix(u32 a, u32 b, int o)
{
    int n = 256 - o;
    return (((((a & 0xff00ff) * n + (b & 0xff00ff) * o) >> 8) & 0xff00ff) | ((((a & 0xff00) * n +
            (b & 0xff00) * o) >> 8) & 0xff00));
}
void initgfx(int w, int h)
{
    PW = w;
    PH = h;
    if (w > 1280) {
        W = 1280;                       /* constant logical width above baseline */
        H = (h * 1280 + (w >> 1)) / w;
    } else {
        W = w;
        H = h;
    }
    if(asset(0,fontdata,sizeof fontdata)<16||memcmp(fontdata,"TFN2",4))sc(S_EXIT,0,0,0,0);
    ng = *(const u32 *)(fontdata + 8);
    symbols = (const u32 *)(fontdata + 12);
    const u8 *p = (const u8 *)(symbols + ng);
    for (int i = 0; i < NFONTS; i++) {
        memcpy(&font[i].h, p, 4);
        p += 4;
        font[i].g = (const void *)p;
        p += ng * sizeof(Glyph);
    }
    bits = p;
    resetclip();
}
// QOI RGB stream: decoding happens once per artwork; thumbnails reuse the same decoded texture.
static int unpack(int id)
{
    if (loaded[id])return 1;
    int n=asset(id+1,compressed,sizeof compressed);if(n<22)return 0;const u8 *p=compressed,*end=p+n;
    if (end - p < 22 || memcmp(p, "qoif", 4) || p[4] || p[5] || p[6] != 5 || p[7] || p[8] || p[9] || p[10] != 3 ||
            p[11] != 32)return 0;
    p += 14;
    u32 tab[64] = {0};
    u8 r = 0, g = 0, b = 0, a = 255;
    int run = 0;
    for (int i = 0; i < 1280 * 800; i++) {
        if (run)run--;
        else {
            if (p >= end - 8)return 0;
            u8 op = *p++;
            if (op == 254 || op == 255) {
                int count = op == 255 ? 4 : 3;
                if (end - 8 - p < count)return 0;
                r = *p++;
                g = *p++;
                b = *p++;
                if (count == 4)a = *p++;
            } else if ((op >> 6) == 0) {
                u32 v = tab[op];
                r = v >> 16;
                g = v >> 8;
                b = v;
                a = v >> 24;
            } else if ((op >> 6) == 1) {
                r += ((op >> 4) & 3) - 2;
                g += ((op >> 2) & 3) - 2;
                b += (op & 3) - 2;
            } else if ((op >> 6) == 2) {
                if (p >= end - 8)return 0;
                int dg = (op & 63) - 32;
                u8 q = *p++;
                r += dg + (q >> 4) - 8;
                g += dg;
                b += dg + (q & 15) - 8;
            } else run = op & 63;
            tab[(3 * r + 5 * g + 7 * b + 11 * a) & 63] = ((u32)a << 24) | ((u32)r << 16) | (g << 8) | b;
        }
        films[id][i] = (r << 16) | (g << 8) | b;
    }
    loaded[id] = 1;
    return 1;
}
void background(int alt, int themei, int darki)
{
    palette = themei % NTHEME;
    night = darki & 1;
    alt = clampi(alt, 0, NWALL-1);
    unpack(alt);
    for (int y = 0; y < H; y++) {
        int sy = y * 800 / H;
        for (int x = 0; x < W; x++) {
            u32 c = films[alt][sy * 1280 + x * 1280 / W];
            back[y * W + x] = night ? blend(c, 0x0b1225, 64) : c;
        }
    }
}
void restorebg(void)
{
    memcpy(canvas, back, (u64)W * H * 4);
}
void clip(int x, int y, int w, int h)
{
    x1 = MAX(0, x);
    y1 = MAX(0, y);
    x2 = MIN(W, x + w);
    y2 = MIN(H, y + h);
}
void resetclip(void)
{
    clip(0, 0, W, H);
}
static inline void pix(int x, int y, u32 c, int a)
{
    if (x >= x1 && x < x2 && y >= y1 && y < y2) {
        u32 *p = &canvas[y * W + x];
        *p = a >= 255 ? c : mix(*p, c, a);
    }
}
static void span(int x, int e, int y, u32 c, int a)
{
    if (y < y1 || y >= y2)
        return;
    x = MAX(x, x1);
    e = MIN(e, x2);
    if (e <= x)
        return;
    u32 *p = canvas + y * W + x;
    if (a >= 255) {
        for (int i = x; i < e; i++)
            *p++ = c;
    } else
        for (int i = x; i < e; i++, p++)
            *p = mix(*p, c, a);
}
void rect(int x, int y, int w, int h, u32 col)
{
    u32 c = color(col);
    for (int j = MAX(y, y1); j < MIN(y + h, y2); j++)
        span(x, x + w, j, c, 255);
}
static u8 corner[65][64][64], made[65];
static void makecorner(int r)
{
    if (made[r])
        return;
    made[r] = 1;
    for (int y = 0; y < r; y++)
        for (int x = 0; x < r; x++) {
            int hits = 0;
            for (int k = 0; k < 4; k++)
                for (int j = 0; j < 4; j++) {
                    int X = (r - x) * 8 - 2 * k - 1, Y = (r - y) * 8 - 2 * j - 1;
                    if (X * X + Y * Y <= r * r * 64)
                        hits++;
                }
            corner[r][y][x] = (hits * 255 + 8) / 16;
        }
}
void roundr(int x, int y, int w, int h, int r, u32 col, int a)
{
    if (w <= 0 || h <= 0)
        return;
    u32 c = color(col);
    r = MAX(0, MIN(64, MIN(r, MIN(w, h) / 2)));
    makecorner(r);
    for (int j = MAX(0, y1 - y); j < MIN(h, y2 - y); j++) {
        int cy = MIN(j, h - j - 1);
        if (cy >= r)
            span(x, x + w, y + j, c, a);
        else {
            int k = 0;
            for (; k < r && corner[r][cy][k] < 255; k++) {
                int alpha = (corner[r][cy][k] * a + 127) / 255;
                if (alpha) {
                    pix(x + k, y + j, c, alpha);
                    pix(x + w - k - 1, y + j, c, alpha);
                }
            }
            span(x + k, x + w - k, y + j, c, a);
        }
    }
}
void line(int x, int y, int X, int Y, u32 col)
{
    int dx = X > x ? X - x : x - X, dy = Y > y ? y - Y : Y - y, sx = x < X ? 1 : -1, sy = y < Y ? 1 : -1,
        e = dx + dy;
    for (;;) {
        pix(x, y, color(col), 255);
        if (x == X && y == Y)
            break;
        int t = 2 * e;
        if (t >= dy) {
            e += dy;
            x += sx;
        }
        if (t <= dx) {
            e += dx;
            y += sy;
        }
    }
}
void icon(int x, int y, int size, int id)
{
    if (id <= ICON_SETTINGS_APPEARANCE && id >= ICON_SETTINGS_GENERAL) {
        extern const u8 settings_svg_masks[];
        if (size >= 1 && size <= 128)
            alpha_symbol(settings_svg_masks + (ICON_SETTINGS_APPEARANCE-id)*4096,
                         64, (Rect){x,y,size,size}, 0xffffff);
        return;
    }
    int procedural=id<0;if(procedural)id=-id;
    if (size < 1 || size > 128 || id < 1 || id > 42)
        return;
    // Native-size menu glyph avoids nearest-neighbor edges when shrinking the 64px icon.
    if (id == 31 && size == 24) {
        static const u8 menu_apple[24 * 24] = {
#include "../assets/apple-menu.inc"
        };
        for (int j = 0; j < 24; j++) for (int i = 0; i < 24; i++) {
                int a = menu_apple[j * 24 + i];
                if (a)pix(x + i, y + j, 0xffffff, a);
            }
        return;
    }
    /* Negative IDs request the procedural system symbol in mixed contexts. */
    static const unsigned sizes[]={16,24,32,48,64,128};
    unsigned tier=0;while(tier+1<COUNT(sizes)&&sizes[tier]<(unsigned)size)tier++;
    u32 entry=((const u32 *)icons)[id*COUNT(sizes)+tier];
    if(procedural){extern const u8 systemicons[];
        const u8 *base=systemicons+id*64*64*4;
        for(int j=MAX(0,y1-y);j<MIN(size,y2-y);j++)for(int i=MAX(0,x1-x);i<MIN(size,x2-x);i++){const u8*p=base+4*((j*64/size)*64+i*64/size);if(p[3])pix(x+i,y+j,(p[0]<<16)|(p[1]<<8)|p[2],p[3]);}return;
    }
    unsigned source_size=(entry&0x80000000u)?64:sizes[tier];
    const u8 *base=icons+(entry&0x7fffffffu);
    for (int j = MAX(0, y1 - y); j < MIN(size, y2 - y); j++)
        for (int i = MAX(0, x1 - x); i < MIN(size, x2 - x); i++) {
            const u8 *p = base + 4 * ((j * source_size / size) * source_size + i * source_size / size);
            if (p[3])
                pix(x + i, y + j, (p[0] << 16) | (p[1] << 8) | p[2], p[3]);
        }
}
extern const u8 accountpic[];
void avatar(int x, int y, int size)
{
    for (int j = 0; j < size; j++)for (int i = 0; i < size; i++) {
            const u8 *p = accountpic + 4 * ((j * 128 / size) * 128 + i * 128 / size);
            if (p[3])pix(x + i, y + j, (p[0] << 16) | (p[1] << 8) | p[2], p[3]);
        }
}
static int glyph(const char **s)
{
    const u8 *p = (const u8 *)*s;
    unsigned c = *p++;
    if (c >= 0xc0 && c < 0xe0 && (p[0] & 0xc0) == 0x80)c = ((c & 31) << 6) | (*p++ & 63);
    else if (c >= 0xe0 && c < 0xf0 && (p[0] & 0xc0) == 0x80 && (p[1] & 0xc0) == 0x80) {
        c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63);
        p += 2;
    }
    *s = (const char *)p;
    if (c >= 32 && c <= 126)return c - 32;
    if (c >= 160 && c <= 255)return c - 160 + 95;
    for (int i = 191; i < ng; i++)if (symbols[i] == c)return i;
    return '?' - 32;
}
int textw(const char *s, int f)
{
    if (f < 0 || f >= NFONTS)f = 1;
    int n = 0;
    while (*s)n += font[f].g[glyph(&s)].adv;
    return n;
}
void text(int x, int y, const char *s, int f, u32 col)
{
    if (f < 0 || f >= NFONTS)f = 1;
    u32 ink = color(col);
    while (*s && x < x2) {
        const Glyph *g = font[f].g + glyph(&s);
        const u8 *p = bits + g->off;
        for (int j = MAX(0, y1 - y); j < MIN((int)font[f].h, y2 - y); j++)for (int i = MAX(0, x1 - x); i < MIN(g->w, x2 - x);
                    i++) {
                int a = p[j * g->w + i];
                if (a)pix(x + i, y + j, ink, a);
            }
        x += g->adv;
    }
}
/* Compact Mono references use the same locked glyph table as UTF-8 text.
 * Wire count is bounded before any lookup. Pixel clipping matches text(). */
static void mono_run(int x,int y,const Draw *q)
{
    int n=MAX(0,MIN(99,q->w));u32 ink=color(q->ink);
    for(int k=0;k<n&&x<x2;k++,x+=8){
        unsigned ref=(u8)q->text[k];int i=ref&&ref<=207?(int)ref-1:'?'-32;
        if(i>=ng)i='?'-32;
        const Glyph *g=font[F_MONO].g+i;const u8 *p=bits+g->off;
        for(int j=MAX(0,y1-y);j<MIN((int)font[F_MONO].h,y2-y);j++)
            for(int col=MAX(0,x1-x);col<MIN(g->w,x2-x);col++){
                int alpha=p[j*g->w+col];if(alpha)pix(x+col,y+j,ink,alpha);
            }
    }
}
void center(int cx, int y, const char *s, int f, u32 col)
{
    text(cx - textw(s, f) / 2, y, s, f, col);
}
extern u32 *desk_ras;        /* C_RASTER staging buffer (main.c) */
void client(int x, int y, int w, int h, Draw *d, int n)
{
    clip(x, y, w, h);
    for (int i = 0; i < n; i++) {
        Draw *q = d + i;
        int X = x + q->x, Y = y + q->y;
        if (q->type == C_RECT)
            roundr(X, Y, q->w, q->h, q->radius, q->ink, q->ink == INK(I_PANEL) && !lite ? 105 : 255);
        else if (q->type == C_IMAGE) photo((Rect) {
            X, Y, q->w, q->h
        }, q->style, q->radius);
        else if (q->type == C_BUTTON) button_face((Rect) {
            X, Y, q->w, q->h
        }, q->style);
        else if (q->type == C_GLASS) glass((Rect) {
            X, Y, q->w, q->h
        }, q->radius, q->style==GLASS_DIALOG?MAT_DIALOG:MAT_SHEET);
        else if (q->type == C_TEXT) {
            if (q->style & 128)
                center(X, Y, q->text, q->style & 127, q->ink);
            else
                text(X, Y, q->text, q->style, q->ink);
        } else if (q->type == C_MONO)mono_run(X,Y,q);
        else if (q->type == C_ICON)
            icon(X, Y, q->w, (int8_t)q->style);
        else if (q->type == C_LINE)
            line(X, Y, x + q->w, y + q->h, q->ink);
        else if (q->type == C_RASTER && desk_ras) {
            int rw = q->w, rh = q->h;
            if (rw >= 1 && rh >= 1 && rw <= 1280 && rh <= 1024)
                for (int j = 0; j < rh; j++) {
                    int dy = Y + j;
                    if (dy < y1 || dy >= y2) continue;
                    for (int i = 0; i < rw; i++) {
                        int dx = X + i;
                        if (dx < x1 || dx >= x2) continue;
                        /* mask bit31: raw pixels must not hit the INK token path */
                        pix(dx, dy, desk_ras[j * rw + i] & 0x00ffffffu, 255);
                    }
                }
        }
    }
    resetclip();
}

// Cached coverage is shared by optical materials and transformed window surfaces.
int coverage(int px, int py, int w, int h, int r)
{
    r = clampi(r, 0, MIN(64, MIN(w, h) / 2));
    int a = MIN(px, w - 1 - px), b = MIN(py, h - 1 - py);
    if (a < 0 || b < 0)return 0;
    if (a >= r || b >= r)return 255;
    makecorner(r);
    return corner[r][b][a];
}
void photo(Rect q, int id, int rad)
{
    if (id < 0 || id >= NWALL || q.w <= 0 || q.h <= 0 || !unpack(id))return;
    int step = MIN((1280 << 14) / q.w, (800 << 14) / q.h), sx = ((1280 << 14) - q.w * step) / 2,
        sy = ((800 << 14) - q.h * step) / 2;
    for (int j = MAX(0, y1 - q.y); j < MIN(q.h, y2 - q.y); j++) {
        const u32 *row = films[id] + 1280 * ((sy + j * step) >> 14);
        int k = MAX(0, x1 - q.x), e = MIN(q.w, x2 - q.x);
        u32 *dst = canvas + (q.y + j) * W + q.x + k;
        for (int i = k, t = sx + k * step; i < e; i++, t += step, dst++) {
            int a = coverage(i, j, q.w, q.h, rad);
            if (a == 255)*dst = row[t >> 14];
            else if (a)*dst = blend(*dst, row[t >> 14], a);
        }
    }
}
void copyout(Rect q, u32 *dst)
{
    for (int j = 0; j < q.h; j++)memcpy(dst + j * q.w, canvas + (q.y + j)*W + q.x, q.w * 4);
}
void copyin(Rect q, const u32 *src)
{
    for (int j = 0; j < q.h; j++)memcpy(canvas + (q.y + j)*W + q.x, src + j * q.w, q.w * 4);
}
void button_face(Rect q, int on)
{
    int r = MIN(8, q.h / 2);
    roundr(q.x, q.y + 1, q.w, q.h, r, night ? 0x121215 : 0xadadb5, 120);
    roundr(q.x, q.y, q.w, q.h - 1, r, on ? INK(I_ACC) : (night ? 0x48484e : 0xf8f8fa), 252);
    stroke((Rect) {
        q.x, q.y, q.w, q.h - 1
    }, r, on ? 0xadcaff : (night ? 0x85858a : 0xbcbcc3), 88);
}

/* SVG-derived monochrome alpha, tinted with the authoritative panel palette. */
void alpha_symbol(const u8 *mask,int size,Rect q,u32 ink)
{
    if(!mask||size<1||q.w<1||q.h<1)return;
    for(int j=MAX(0,y1-q.y);j<MIN(q.h,y2-q.y);j++)
        for(int i=MAX(0,x1-q.x);i<MIN(q.w,x2-q.x);i++){
            int a=mask[(j*size/q.h)*size+i*size/q.w];
            if(a)pix(q.x+i,q.y+j,ink,a);
        }
}
