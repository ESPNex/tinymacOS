#include "ui.h"
// Decimal fixed point: four fractional positions; no dependence on host floating point.
#define SCALE 10000ll
#define LIM 999999999999ll
static char entry[35] = "0", expression[90] = "Pronto", hist[4][90];
static i64 acc, last;
static int op, repeat, fresh = 1, fail, countHist;
static void fmt(char *s, i64 n)
{
    int neg = n < 0;
    if (neg)n = -n;
    s[0] = 0;
    if (neg)append(s, "-", 35);
    addnum(s, n / SCALE, 35);
    int f = n % SCALE;
    if (f) {
        char d[6] = ".0000";
        for (int j = 4; j; j--) {
            d[j] = '0' + f % 10;
            f /= 10;
        }
        int j = 4;
        while (d[j] == '0')d[j--] = 0;
        append(s, d, 35);
    }
}
static i64 parse(void)
{
    int neg = entry[0] == '-', i = neg, dec = 0, k = 1000;
    i64 v = 0;
    for (; entry[i]; i++) {
        char c = entry[i];
        if (c == '.') {
            dec = 1;
            continue;
        }
        if (dec) {
            v += (c - '0') * k;
            k /= 10;
        } else v = v * 10 + (c - '0') * SCALE;
    }
    return neg ? -v : v;
}
static const char *symbol(int c)
{
    return c == '*' ? "×" : c == '/' ? "÷" : c == '-' ? "−" : "+";
}
static void clear(void)
{
    strset(entry, "0", 35);
    strset(expression, "Pronto", 90);
    acc = last = op = repeat = fail = 0;
    fresh = 1;
}
static int operate(i64 a, int c, i64 b, i64 *z)
{
    i64 A = a < 0 ? -a : a, B = b < 0 ? -b : b;
    if (c == '*') {
        if (B && A > LIM * SCALE / B)return 0;
        *z = a * b / SCALE;
    } else if (c == '/') {
        if (!b)return 0;
        *z = a * SCALE / b;
    } else *z = c == '+' ? a + b : a - b;
    return *z <= LIM && *z >= -LIM;
}
static int calc(i64 b)
{
    i64 z;
    char left[35], right[35];
    fmt(left, acc);
    fmt(right, b);
    strset(expression, left, 90);
    append(expression, " ", 90);
    append(expression, symbol(op), 90);
    append(expression, " ", 90);
    append(expression, right, 90);
    if (!operate(acc, op, b, &z)) {
        strset(entry, "Errore", 35);
        strset(expression, b == 0 && op == '/' ? "Divisione per zero" : "Numero fuori intervallo", 90);
        trace(expression);
        fail = 1;
        return 0;
    }
    acc = z;
    fmt(entry, z);
    return 1;
}
static void key(int k)
{
    if (k == 27 || k == 'c' || k == 'C') {
        clear();
        return;
    }
    if (fail) {
        if ((k >= '0' && k <= '9') || k == '.')clear();
        else return;
    }
    if ((k >= '0' && k <= '9') || k == '.' || k == ',') {
        if (fresh) {
            strset(entry, "0", 35);
            fresh = 0;
        }
        if (k == ',')k = '.';
        int l = strlen(entry), dot = -1;
        for (int i = 0; i < l; i++)if (entry[i] == '.')dot = i;
        if (k == '.' && dot >= 0)return;
        if (dot >= 0 && l - dot > 4)return;
        if (dot < 0 && l - (entry[0] == '-') >= 8 && k != '.')return;
        if (k != '.' && eq(entry, "0")) {
            entry[0] = k;
            entry[1] = 0;
        } else {
            entry[l] = k;
            entry[l + 1] = 0;
        }
        return;
    }
    if (k == 8 || k == K_DEL) {
        if (fresh)return;
        int l = strlen(entry);
        if (l > 1)entry[l - 1] = 0;
        else strset(entry, "0", 35);
        if (eq(entry, "-"))strset(entry, "0", 35);
        return;
    }
    if (k == '%' || k == 'n') {
        i64 a = parse();
        fmt(entry, k == 'n' ? -a : a / 100);
        fresh = 0;
        return;
    }
    if (k == '+' || k == '-' || k == '*' || k == '/') {
        if (op && !fresh && !calc(parse()))return;
        if (!op)acc = parse();
        op = k;
        fresh = 1;
        repeat = 0;
        fmt(entry, acc);
        strset(expression, entry, 90);
        append(expression, " ", 90);
        append(expression, symbol(k), 90);
        return;
    }
    if (k == '=' || k == '\n') {
        if (!op && !repeat)return;
        if (!op) {
            op = repeat;
            acc = parse();
        } else last = parse();
        repeat = op;
        if (!calc(last))return;
        op = 0;
        fresh = 1;
        for (int j = 3; j > 0; j--)strset(hist[j], hist[j - 1], 90);
        strset(hist[0], expression, 90);
        append(hist[0], " = ", 90);
        append(hist[0], entry, 90);
        countHist = MIN(4, countHist + 1);
    }
}
static const char *labels[] = {"AC", "±", "%", "÷", "7", "8", "9", "×", "4", "5", "6", "−", "1", "2", "3", "+", "0", ".", "⌫", "="};
static const char keys[] = "cn%/789*456-123+0.\b=";
static int active = -1;
static u64 press;
static int padw(void)
{
    return aw >= 595 ? aw - 198 : aw;
}
static void paint(void)
{
    begin();
    int W = padw(), cw = (W - 32) / 4, ch = (ah - 133) / 5;
    // Liquid gloss: a full-bleed frosted sheet (respects Opaco via lite),
    // a translucent display well, and card keys edged with the theme line.
    Sheet(0, 0, W, ah, 0);
    Box(12, 4, W - 24, 100, 14, PANEL);
    Text(28, 13, "Standard", F_SMALL, SUB);
    Text(28, 37, expression, F_BODY, SUB);
    Text(26, 56, entry, strlen(entry) > 12 ? F_TITLE : F_DISPLAY, TX);
    for (int j = 0; j < 20; j++) {
        int x = 12 + j % 4 * cw, y = 115 + j / 4 * ch;
        int strong = j % 4 == 3, kr = MIN(18, (ch - 5) / 2);
        int lit = j == active && si.ticks - press < 15;
        u32 fill = strong ? 0xe88b21 : j < 3 ? SOFT : CARD;
        if (lit)fill = strong ? 0xf6b66a : SEL;
        Box(x - 1, y - 1, cw - 3, ch - 3, kr + 1, EDGE);
        Box(x, y, cw - 5, ch - 5, kr, fill);
        Center(x + (cw - 5) / 2, y + (ch - 29) / 2, j == 18 ? "←" : labels[j], F_CAPTION,
               strong || lit ? 0xffffff : TX);
    }
    if (W < aw) {
        Sheet(W, 4, aw - W - 6, ah - 12, 12);
        Text(W + 15, 20, "Cronologia", F_BOLD, TX);
        if (!countHist)Text(W + 15, 53, "Nessun risultato", F_SMALL, SUB);
        for (int j = 0; j < countHist; j++)Text(W + 14, 54 + j * 41, hist[j], F_SMALL, TX);
    }
    finish();
}
void app_main(void)
{
    win(607, 377, "Calcolatrice");
    paint();
    for (;;) {
        Event e;
        int d = 0;
        sc(S_INFO, &si, 0, 0, 0);
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            d = 1;
            if (baseevent(&e))continue;
            if (e.type == EV_CLOSE)quit();
            int k = 0;
            if (e.type == EV_KEY)k = e.a;
            if (e.type == EV_CLICK && e.a >= 12 && e.b >= 115) {
                int cw = (padw() - 32) / 4, ch = (ah - 133) / 5;
                int c = (e.a - 12) / cw, r = (e.b - 115) / ch;
                if (c < 4 && r < 5)k = keys[r * 4 + c];
            }
            if (k) {
                if (k == '\n')k = '=';
                key(k);
                active = -1;
                for (int i = 0; i < 20; i++)if (keys[i] == k)active = i;
                press = si.ticks;
            }
        }
        if (active >= 0 && si.ticks - press >= 15) {
            active = -1;
            d = 1;
        }
        if (d)paint();
        slp(2);
    }
}
