#ifdef TMC_INPUT_TEST
#include "../tests/input_test_support.h"
#else
#include "../kernel/k.h"
#endif
// Validated USB HID reports -> native input events. No 8042 or BIOS input calls.
static Event q[128];
static int head, tail, caps;
static volatile int ledpend = -1;
static u8 prior[8];
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
    int shift = !!(m & 0x22), ctrl = !!(m & 0x99), v = 0;
    if (k >= 4 && k <= 29)
        v = (shift ^ caps ? 'A' : 'a') + k - 4;
    else if (k >= 30 && k <= 39)
        v = (shift ? "!@#$%^&*()" : "1234567890")[k - 30];
    else if (k >= 40 && k <= 56) {
        static const char a[] = {10, 27, 8, 9, ' ', '-', '=', '[', ']', '\\', '#', ';', '\'', '`', ',', '.', '/'};
        static const char b[] = {10, 27, 8, 9, ' ', '_', '+', '{', '}', '|', '~', ':', '"', '~', '<', '>', '?'};
        v = (shift ? b : a)[k - 40];
    } else if (k >= 58 && k <= 67)
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
void hid_keys(const u8 r[8])
{
    int still = 0;
    mods = r[0];
    for (int i = 2; i < 8; i++)
        if (r[i] == 1)
            return;
    for (int i = 2; i < 8; i++) {
        int k = r[i];
        if (k < 4)
            continue;
        int old = 0;
        for (int j = 2; j < 8; j++)
            old |= prior[j] == k;
        if (!old) {
            if (k == 57) {
                caps = !caps;
                ledpend = caps ? 2 : 0;   // queue HID LED report for xHCI
            } else {
                emit(k, mods);
                repeat = k;
                due = ticks + 48;
            }
        }
        if (k == repeat)
            still = 1;
    }
    if (!still)
        repeat = 0;
    memcpy(prior, r, 8);
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

void hid_relative(int x, int y, int mask, int wheel)
{
    put((Event){.type=EV_MOUSE, .a=x, .b=y, .c=mask});
    if (wheel) put((Event){.type=EV_SCROLL, .a=wheel});
}
