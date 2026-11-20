#include "io.h"
#include "spinlock.h"
#include "handoff.h"
extern struct Boot boot;
static const u8 font[] = {
#include "../assets/panicfont.inc"
};
static void txt(int x, int y, const char *s, u32 c) {
    for (; *s; s++, x += 16) {
        int ch = (u8) * s;
        if (ch < 32 || ch > 126)
            continue;
        for (int j = 0; j < 15; j++)
            for (int i = 0; i < 8; i++)
                if (font[(ch - 32) * 15 + j] & (1 << i))
                    for (int a = 0; a < 2; a++)
                        for (int b = 0; b < 2; b++)
                            if ((unsigned)(x + i * 2 + b) < boot.w && (unsigned)(y + j * 2 + a) < boot.h)
                                ((volatile u32 *)boot.fb)[(y + j * 2 + a)*boot.stride + x + i * 2 + b] = c;
    }
}
#ifdef TMC_DEBUG
static void com1(const char *s)
{
    for (; *s; s++) {
        while (!(inb(0x3fd) & 0x20))
            ;
        outb(0x3f8, (u8)*s);
    }
}
static Spin slog; // SMP: one logical line per call, no interleaving with the other CPU
void logstr(const char *s) { u32 f; spin_lock(&slog, &f); com1(s); spin_unlock(&slog, f); }
void lognum(u64 v)
{
    char b[25];
    int i = 24;
    b[24] = 0;
    if (!v) { i = 23; b[23] = '0'; }
    else while (v) { b[--i] = '0' + (v % 10); v /= 10; }
    u32 f; spin_lock(&slog, &f); com1(b + i); spin_unlock(&slog, f);
}
void loghex(u64 v)
{
    char b[25];
    int i = 24;
    b[24] = 0;
    if (!v) { i = 23; b[23] = '0'; }
    else while (v) { int d = v & 15; b[--i] = d < 10 ? '0' + d : 'a' + d - 10; v >>= 4; }
    u32 f; spin_lock(&slog, &f); com1(b + i); spin_unlock(&slog, f);
}
#endif
__attribute__((noreturn)) void retail_stop(void) {
    asm volatile("cli");
    if (boot.fb) {
        for (unsigned y=0; y<boot.h; y++)
            for (unsigned x=0; x<boot.w; x++)
                ((volatile u32 *)boot.fb)[y*boot.stride+x]=0x24283a;
        txt(48, 90, "Il computer deve essere riavviato.", 0xffffff);
        txt(48, 156, "Se il problema si ripete, premi R durante l'avvio", 0xe3e4ea);
        txt(48, 192, "per aprire Recupero tinymacOS.", 0xe3e4ea);
    }
    for (;;)
        asm volatile("hlt");
}
