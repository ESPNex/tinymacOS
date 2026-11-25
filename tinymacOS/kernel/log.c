#include "io.h"
#include "spinlock.h"
#include "handoff.h"
#include "bootconsole.h"
#include "bootconfig.h"
#include "../boot/splash.h"
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
static Spin slog;
static BootConsole console;
static int console_live,log_capture=1,splash_live=1;
static char logring[16384];static unsigned loghead,logused;
void boot_loginit(void){console_live=!!(boot.boot_flags&TMC_BOOT_VERBOSE);if(console_live)bootconsole_clear(&boot,&console);}
/* The Recovery compositor can present on an AP before BSP smp_start()
 * returns. Serialize all kernel splash writes with the first-present handoff:
 * a late 100% update must never paint over a userspace frame. */
void boot_progress(int percent)
{
    u32 f;spin_lock(&slog,&f);
    if(splash_live)splash_bar(&boot,percent);
    spin_unlock(&slog,f);
}
void boot_console_stop(void){u32 f;spin_lock(&slog,&f);console_live=0;log_capture=0;splash_live=0;spin_unlock(&slog,f);}
static void com1(const char *s)
{
#ifdef TMC_DEBUG
    for(;*s;s++){unsigned limit=100000;while(!(inb(0x3fd)&0x20)&&--limit);if(limit)outb(0x3f8,(u8)*s);}
#else
    (void)s;
#endif
}
void boot_logstr(const char *s)
{
    u32 f;spin_lock(&slog,&f);
    if(log_capture)for(const char *p=s;*p;p++){logring[loghead]=*p;loghead=(loghead+1)%sizeof logring;if(logused<sizeof logring)logused++;}
    if(console_live)bootconsole_put(&boot,&console,s);
    com1(s);spin_unlock(&slog,f);
}
int boot_logcopy(char *out,unsigned cap,unsigned offset)
{
    u32 f;spin_lock(&slog,&f);unsigned n=offset<logused?MIN(cap,logused-offset):0,at=(loghead+sizeof logring-logused+offset)%sizeof logring;
    for(unsigned j=0;j<n;j++)out[j]=logring[(at+j)%sizeof logring];spin_unlock(&slog,f);return n;
}
void boot_lognum(u64 n){char b[24];number(b,n);boot_logstr(b);}
void logstr(const char *s){boot_logstr(s);}
void lognum(u64 n){char b[24];number(b,n);logstr(b);}
void loghex(u64 n){char b[24];int i=23;b[i]=0;do{b[--i]="0123456789abcdef"[n&15];n>>=4;}while(n);logstr(b+i);}
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
