#include "ui.h"
static void pair(int x, int y, const char *n, const char *s)
{
    Text(x, y, n, F_SMALL, SUB);
    Text(x, y + 17, s, F_BODY, TX);
}
static void paint(void)
{
    begin();
    int left = 139;
    Photo(4, 1, left - 2, ah - 9, 7, 11);
    Icon(39, 45, 65, 31);
    Center(71, 127, "RIVIERA", F_MICRO, 0xf7f8ff);
    int x = left + 17;
    Text(x, 13, "Informazioni sul sistema", F_SMALL, SUB);
    Text(x - 2, 32, "tinymacOS", F_DISPLAY, TX);
    Text(x, 91, "Versione 2.4.1  ·  Levante", F_CAPTION, SUB);
    Box(x, 130, aw - x - 17, 1, 0, EDGE);
    pair(x, 145, "Processore", si.cpu);
    char s[90] = "";
    addnum(s, si.free >> 20, 90);
    append(s, " MiB liberi / ", 90);
    addnum(s, si.total >> 20, 90);
    append(s, " MiB disponibili", 90);
    pair(x, 194, "Memoria", s);
    pair(x, 243, "Volume dati", si.persistent ? "Macintosh HD - Dati" : "Volume live temporaneo");
    s[0] = 0;
    addnum(s, si.procs, 90);
    append(s, " processi attivi  ·  Build 21B2", 90);
    Text(x, ah - 30, s, F_SMALL, EMP);
    finish();
}
void app_main(void)
{
    win(575, 333, "Info su tinymacOS");
    paint();
    u64 stamp = 0;
    for (;;) {
        Event e;
        int d = 0;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            d = 1;
            if (e.type == EV_CLOSE)quit();
            baseevent(&e);
        }
        sc(S_INFO, &si, 0, 0, 0);
        if (d || si.ticks / 150 != stamp) {
            paint();
            stamp = si.ticks / 150;
        }
        slp(8);
    }
}
