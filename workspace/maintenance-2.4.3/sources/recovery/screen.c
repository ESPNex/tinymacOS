#include "screen.h"
#include "window_chrome.h"
extern const u8 recfont[], recicons[];
typedef struct PACK {
    u16 adv, w;
    u32 off;
} G;
static const G *tab[4];
static u32 ht[4];
static const u8 *bits;
static const u32 *chars;
static int ng;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *go;
static u32 *pix;
int rw, rh;
static u32 blend(u32 a, u32 b, int k)
{
    u32 c = 0;
    for (int i = 0; i < 24; i += 8)c |= (((((a >> i) & 255) * (255 - k) + ((b >> i) & 255) * k + 127) / 255) << i);
    return c;
}
int screen_init(EFI_SYSTEM_TABLE *st)
{
    EFI_GUID g = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    if (EFI_ERROR(st->BootServices->LocateProtocol(&g, 0, (void **)&go)))return 0;
    int best = -1, score = 0;
    for (u32 i = 0; i < go->Mode->MaxMode; i++) {
        UINTN z;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *m;
        if (EFI_ERROR(go->QueryMode(go, i, &z, &m)))continue;
        int w = m->HorizontalResolution, h = m->VerticalResolution, s = w * h + (w == 1280 && h == 800 ? 2000000 : 0);
        if (w >= 800 && w <= 1280 && h >= 600 && h <= 1024 && s > score) {
            score = s;
            best = i;
        }
        st->BootServices->FreePool(m);
    }
    if (best < 0 || EFI_ERROR(go->SetMode(go, best)))return 0;
    rw = go->Mode->Info->HorizontalResolution;
    rh = go->Mode->Info->VerticalResolution;
    if (EFI_ERROR(st->BootServices->AllocatePool(EfiLoaderData, rw * rh * 4, (void **)&pix)))return 0;
    ng = *(u32 *)(recfont + 8);
    chars = (void *)(recfont + 12);
    const u8 *p = (void *)(chars + ng);
    for (int i = 0; i < 4; i++) {
        memcpy(ht + i, p, 4);
        p += 4;
        tab[i] = (void *)p;
        p += 8 * ng;
    }
    bits = p;
    return 1;
}
void fill(int x, int y, int w, int h, int r, u32 c)
{
    if(w<=0||h<=0)return;
    r=MAX(0,MIN(r,MIN(w,h)/2));
    for(int j=MAX(0,-y);j<MIN(h,rh-y);j++)
        for(int i=MAX(0,-x);i<MIN(w,rw-x);i++) {
            int a=tmc_chrome_coverage(i,j,w,h,r);
            if(a){u32 *p=pix+(y+j)*rw+x+i;*p=a==255?c:blend(*p,c,a);}
        }
}
void window_frame(int x,int y,int w,int h)
{
    fill(x-6,y+1,w+12,h+12,TMC_CHROME_RADIUS+6,0xdcdee4);
    fill(x-3,y+2,w+6,h+6,TMC_CHROME_RADIUS+3,0xc8cbd3);
    for(int j=MAX(0,-y);j<MIN(h,rh-y);j++)
        for(int i=MAX(0,-x);i<MIN(w,rw-x);i++){
            int a=tmc_chrome_coverage(i,j,w,h,TMC_CHROME_RADIUS);
            if(!a)continue;
            u32 c=j<TMC_CHROME_TITLE?0xedeef2:0xf5f5f7;
            if(j==TMC_CHROME_TITLE-1)c=0xd7d9df;
            if(j==0||i==0||i==w-1||j==h-1)c=0xffffff;
            u32 *p=pix+(y+j)*rw+x+i;*p=a==255?c:blend(*p,c,a);
        }
}
static int sym(const char **sp)
{
    const u8 *p = (void *)*sp;
    int c = *p++;
    if (c >= 192 && c < 224 && (*p & 192) == 128)c = (c & 31) * 64 + (*p++ & 63);
    else if (c >= 224 && c < 240 && (p[0] & 192) == 128 && (p[1] & 192) == 128) {
        c = (c & 15) * 4096 + (p[0] & 63) * 64 + (p[1] & 63);
        p += 2;
    }
    *sp = (void *)p;
    for (int i = 0; i < ng; i++)if (chars[i] == (u32)c)return i;
    return 0;
}
int tw(int f, const char *s)
{
    int w = 0;
    while (*s)w += tab[f][sym(&s)].adv;
    return w;
}
void label(int x, int y, int f, const char *s, u32 c)
{
    while (*s) {
        const G *g = tab[f] + sym(&s);
        for (int j = 0; j < (int)ht[f] && y + j < rh; j++)for (int i = 0; i < g->w && x + i < rw; i++)if (x + i >= 0 &&
                        y + j >= 0) {
                    int a = bits[g->off + j * g->w + i];
                    if (a) {
                        u32 *p = pix + (y + j) * rw + x + i;
                        *p = blend(*p, c, a);
                    }
                }
        x += g->adv;
    }
}
void glyph(int x, int y, int sz, int id)
{
    const u8 *b = recicons + id * 16384;
    for (int j = 0; j < sz; j++)for (int i = 0; i < sz; i++)if ((u32)(x + i) < (u32)rw && (u32)(y + j) < (u32)rh) {
                const u8 *p = b + 4 * (j * 64 / sz * 64 + i * 64 / sz);
                u32 *q = pix + (y + j) * rw + x + i;
                if (p[3])*q = blend(*q, (p[0] << 16) | (p[1] << 8) | p[2], p[3]);
            }
}
void show(int x, int y)
{
    glyph(x - 3, y - 2, 23, 6);
    go->Blt(go, (void *)pix, EfiBltBufferToVideo, 0, 0, 0, 0, rw, rh, 0);
}
