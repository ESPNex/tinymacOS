#ifdef TMC_INPUT_TEST
#include "../tests/input_test_support.h"
#else
#include "../kernel/k.h"
#endif
#include "keyboard_it.h"
// Validated USB HID and native PS/2 decoders -> input events. No BIOS input calls.
static Event q[128];
static int head, tail, caps;
static volatile int ledpend = -1;
static u8 prior_keys[32],legacy_keys[32],linux_keys[32];
static unsigned legacy_mods,linux_mods;
static int repeat, mods;
static u64 due;
static Spin input_lock;
static int mergeable_tail, pointer_buttons;
InputDiag input_diag = {.version=TMC_INPUTDIAG_VERSION,.bytes=sizeof(InputDiag)};
void input_diagnostics(InputDiag *out)
{
    u32 fl; spin_lock(&input_lock,&fl);
    *out=input_diag;
    spin_unlock(&input_lock,fl);
}
static void put(Event e)
{
    u32 fl;
    spin_lock(&input_lock, &fl);
    int mergeable=0;
    if(e.type==EV_MOUSE||e.type==EV_POINTER_ABS) {
        mergeable=e.c==pointer_buttons && !e.d;
        pointer_buttons=e.c;
        input_diag.delivered++;
        input_diag.last_x=e.a;input_diag.last_y=e.b;input_diag.last_buttons=e.c;
        /* Collapse only consecutive motion, never a button transition, wheel
         * or key. Preserve the sum (relative) or newest position (absolute). */
        int last=(head+127)%128;
        if(mergeable && mergeable_tail && head!=tail && q[last].type==e.type && q[last].c==e.c &&
           !q[last].d && !e.d) {
            i64 x=(i64)q[last].a+e.a,y=(i64)q[last].b+e.b;
            if(e.type==EV_POINTER_ABS || (x>=-2147483647 && x<=2147483647 && y>=-2147483647 && y<=2147483647)) {
                q[last].a=e.type==EV_MOUSE?(int)x:e.a;
                q[last].b=e.type==EV_MOUSE?(int)y:e.b;
                input_diag.merged++;
                spin_unlock(&input_lock,fl);return;
            }
        }
    }
    int n = (head + 1) % 128;
    if (n != tail) {
        q[head] = e;
        head = n;mergeable_tail=mergeable;
    } else {input_diag.dropped++;mergeable_tail=0;}
    spin_unlock(&input_lock, fl);
}
int input_pop(Event *e)
{
    u32 fl;
    spin_lock(&input_lock, &fl);
    int have = head != tail;
    if (have) {
        *e = q[tail];
        memset(q + tail, 0, sizeof q[tail]);
        tail = (tail + 1) % 128;
    }
    spin_unlock(&input_lock, fl);
    return have;
}
static void emit(int k, int m)
{
    int shift=!!(m&0x22),ctrl=!(m&0x40)&&!!(m&0x99),v=it_character(k,m,caps);
    if(v){} else if (k >= 58 && k <= 67)
        v = K_F1 + k - 58;
    else if (k == 68)
        v = K_F11;
    else if (k == 74)
        v = K_HOME;
    else if (k == 77)
        v = K_END;
    else if (k == 76)
        v = K_DEL;
    else if (k == 79)
        v = K_RIGHT;
    else if (k == 80)
        v = K_LEFT;
    else if (k == 81)
        v = K_DOWN;
    else if (k == 82)
        v = K_UP;
    if (v)
        put((Event) {
        .type = EV_KEY, .a = v, .b = ctrl, .c = shift
    });
}
void input_key_usage(int usage,int modifiers)
{
    if(usage==57){caps=!caps;ledpend=caps?2:0;}
    else emit(usage,modifiers);
}
/* Merge Linux input devices and any allocation-fallback Boot stream. Keep
 * the whole physical bitmap: six-key USB reports from two keyboards must not
 * truncate each other, lose modifiers, or release a key still held elsewhere.
 */
static void keyboard_sync(void)
{
    mods=legacy_mods|linux_mods;
    for(unsigned k=4;k<224;k++){
        unsigned bit=1u<<(k%8),at=k/8;
        int held=!!((legacy_keys[at]|linux_keys[at])&bit);
        if(held&&!(prior_keys[at]&bit)){
            if(k==57)input_key_usage(57,mods);
            else {emit(k,mods);repeat=k;due=ticks+48;}
        }
        if(held)prior_keys[at]|=bit;else prior_keys[at]&=~bit;
    }
    if(repeat&&!(prior_keys[repeat/8]&(1u<<(repeat%8))))repeat=0;
}
void hid_linux_keys(const u8 bitmap[32],unsigned modifiers)
{
    memcpy(linux_keys,bitmap,32);linux_mods=modifiers&255;keyboard_sync();
}
void hid_keys(const u8 r[8])
{
    for(unsigned i=2;i<8;i++)if(r[i]>=1&&r[i]<=3)return;
    memset(legacy_keys,0,sizeof legacy_keys);legacy_mods=r[0];
    for(unsigned i=2;i<8;i++)if(r[i]>=4&&r[i]<224)
        legacy_keys[r[i]/8]|=1u<<(r[i]%8);
    keyboard_sync();
}
void hid_mouse(const u8 *r)
{
    put((Event) {
        .type = EV_MOUSE, .a = (signed char)r[1], .b = (signed char)r[2], .c = r[0] & 7
    });
}
void hid_absolute(int x, int y, int mask, int wheel)
{
    put((Event){.type=EV_POINTER_ABS, .a=x, .b=y, .c=mask, .d=wheel});
}
void input_repeat(void)
{
    if (repeat && ticks >= due) {
        emit(repeat, mods);
        due = ticks + 5;
    }
}
// Pending Caps Lock LED byte for the xHCI side; -1 = nothing to send.
int input_led_take(void)
{
    int v = ledpend;
    ledpend = -1;
    return v;
}

/* EV_SCROLL.b carries horizontal wheel, independent of vertical .a. */
void hid_horizontal(int delta){if(delta)put((Event){.type=EV_SCROLL,.b=delta});}

void hid_relative(int x, int y, int mask, int wheel)
{
    put((Event){.type=EV_MOUSE, .a=x, .b=y, .c=mask});
    if (wheel) put((Event){.type=EV_SCROLL, .a=wheel});
}
