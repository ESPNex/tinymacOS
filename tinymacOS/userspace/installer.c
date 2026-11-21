#include "ui.h"
// The assistant is an ordinary Ring-3 window. Only the kernel owns disk addresses.
static InstallInfo info;
static int page, choice = -1, confirm, dirty = 1;
static void refresh(void)
{
    sc(S_INSTALL, 0, &info, 0, 0);
    if (info.state == INSTALL_DONE)
        page = 4;
    else if (info.state == INSTALL_ERROR)
        page = 5;
    else if (info.state >= INSTALL_SCAN && info.state <= INSTALL_VERIFY)
        page = 3;
    if (!info.live)
        page = 6;
}
static void go(int n)
{
    page = n;
    dirty = 1;
    ui_motion(FX_SLIDE);
}
static int can(void)
{
    return choice >= 0 && choice < info.count && info.disk[choice].usable;
}
static void next(void)
{
    if (page == 0) {
        refresh();
        if (info.live)
            go(1);
    } else if (page == 1 && can()) {
        confirm = 0;
        go(2);
    } else if (page == 2 && confirm && can()) {
        sc(S_INSTALL, 1, choice, 0, 0);
        refresh();
        dirty = 1;
    } else if (page == 4) {
        sc(S_POWER, 0, 0, 0, 0);
    } else if (page == 5) {
        sc(S_INSTALL, 3, 0, 0, 0);
        refresh();
        choice = -1;
        go(1);
    } else if (page == 6)
        quit();
}
static void back(void)
{
    if (page == 1)
        go(0);
    else if (page == 2) {
        confirm = 0;
        go(1);
    } else if (page == 3 && info.state == INSTALL_SCAN && sc(S_INSTALL, 3, 0, 0, 0) == 0) {
        refresh();
        go(1);
    } else if (page == 0 || page >= 4)
        quit();
}
static void diskname(char*s, int d)
{
    strset(s, "SATA ", 80);
    addnum(s, d, 80);
    append(s, "  ·  ", 80);
    append(s, info.disk[d].model, 80);
}
static int rail(void)
{
    return aw > 550 ? 154 : 0;
}
static void paint(void)
{
    begin();
    int r = rail(), x = r + 24, w = aw - x - 23, foot = ah - 52;
    Box(0, 0, aw, ah, 0, CARD);
    if (r) {
        Box(0, 0, r, ah, 0, PANEL);
        Box(r - 1, 0, 1, ah, 0, EDGE);
        Icon(15, 16, 45, 38);
        Text(17, 70, "TRAMONTANA", F_MICRO, SUB);
        Text(16, 90, "Installazione", F_CAPTION, TX);
        const char *lab[] = {"Introduzione", "Destinazione", "Riepilogo", "Installazione", "Fine"};
        for (int i = 0; i < 5; i++) {
            int y = 145 + 31 * i, active = MIN(page, 4) == i;
            if (active)
                Box(8, y - 4, r - 17, 26, 7, SEL);
            Box(17, y + 5, 8, 8, 4, active ? ACC : EDGE);
            Text(35, y, lab[i], F_SMALL, active ? EMP : SUB);
        }
        Text(17, ah - 29, "tinymacOS 10.1.2", F_MICRO, SUB);
    }
    if (!page) {
        Photo(x, 16, w, MAX(65, foot - 161), 12, 12);
        int y = MAX(65, foot - 161) + 32;
        Text(x, y, "Installa tinymacOS", F_TITLE, TX);
        Text(x, y + 43, "Sistema e Recupero in una sola ISO.", F_BODY, TX);
        Text(x, y + 76, "L’assistente copia anche l’account e i documenti", F_SMALL, SUB);
        Text(x, y + 96, "di questa sessione. Serve un disco vuoto dedicato.", F_SMALL, SUB);
    } else if (page == 1) {
        Text(x, 19, "Scegli un disco", F_TITLE, TX);
        Text(x, 68, "Destinazione SATA · da 64 MiB a 128 GiB", F_SMALL, SUB);
        if (!info.count) {
            Text(x, 122, "Nessun disco SATA rilevato.", F_BOLD, TX);
            Text(x, 154, "Spegni e collega un disco virtuale vuoto.", F_SMALL, SUB);
        }
        int slots = MAX(1, (foot - 104) / 49), first = MAX(0, choice - slots + 1);
        for (int i = first; i < info.count && i < first + slots; i++) {
            int y = 98 + (i - first) * 49;
            Box(x, y, w, 44, 9, i == choice ? SEL : SOFT);
            Icon(x + 7, y + 4, 35, 33);
            char s[80];
            diskname(s, i);
            Text(x + 53, y + 3, s, F_SMALL, TX);
            s[0] = 0;
            addnum(s, info.disk[i].sectors / 2048, 80);
            append(s, info.disk[i].usable ? " MiB  ·  Da verificare" : " MiB  ·  Non disponibile", 80);
            Text(x + 53, y + 23, s, F_MICRO, SUB);
        }
    } else if (page == 2) {
        Text(x, 20, "Conferma l’installazione", F_TITLE, TX);
        char s[80];
        diskname(s, choice);
        Icon(x, 77, 46, 33);
        Text(x + 58, 82, s, F_BOLD, TX);
        s[0] = 0;
        addnum(s, info.disk[choice].sectors / 2048, 80);
        append(s, " MiB  ·  GPT / UEFI", 80);
        Text(x + 58, 106, s, F_SMALL, SUB);
        Box(x, 146, w, 1, 0, EDGE);
        Text(x, 164, "BaseSystem.dmg verrà copiato nella partizione Recovery.", F_SMALL, TX);
        Text(x, 189, "Per proteggere i tuoi file, su un disco non vuoto", F_SMALL, SUB);
        Text(x, 209, "non verrà scritto nulla.", F_SMALL, SUB);
        Box(x, foot - 66, w, 48, 9, SOFT);
        Box(x + 12, foot - 52, 20, 20, 5, confirm ? ACC : EDGE);
        if (confirm)
            Center(x + 22, foot - 53, "✓", F_SMALL, si.dark ? 0x142525 : 0xffffff);
        Text(x + 42, foot - 51, "Confermo la destinazione selezionata", F_SMALL, TX);
    } else if (page == 3) {
        Icon(x, 23, 52, 38);
        Text(x, 95, "Installazione in corso", F_TITLE, TX);
        Text(x, 150, info.message, F_BODY, SUB);
        int pct = info.total ? (u64)info.done * 100 / info.total : 0;
        Box(x, 198, w, 8, 4, EDGE);
        if (pct)
            Box(x, 198, MAX(5, w * pct / 100), 8, 4, ACC);
        char s[24] = "";
        addnum(s, pct, 24);
        append(s, "%", 24);
        Text(x, 223, s, F_SMALL, EMP);
        Text(x, foot - 30, "Lascia acceso il computer fino alla verifica finale.", F_SMALL, SUB);
    } else if (page == 4) {
        Photo(x, 15, w, MAX(57, foot - 212), 12, 11);
        int y = MAX(57, foot - 212) + 32;
        Text(x, y, "Il disco è pronto", F_TITLE, TX);
        Text(x, y + 47, "Puoi avviare tinymacOS senza il supporto di installazione.", F_SMALL, SUB);
        Text(x, y + 81, "1.  Spegni il computer.", F_BODY, TX);
        Text(x, y + 112, "2.  Rimuovi il supporto di installazione.", F_BODY, TX);
        Text(x, y + 143, "3.  Avvia dal disco e usa lo stesso account.", F_BODY, TX);
    } else {
        Icon(x, 18, 58, page == 5 ? 33 : 38);
        Text(x, 106, page == 5 ? "Installazione interrotta" : "Sistema già installato", F_TITLE, TX);
        if (page == 5) {
            Text(x, 168, "Il disco non è stato attivato dall’assistente.", F_BODY, TX);
            Text(x, 212, info.message, F_SMALL, SUB);
        } else {
            Text(x, 167, "Stai usando il sistema avviato dal disco.", F_BODY, TX);
            Text(x, 209, "Per una nuova installazione, avvia dalla ISO.", F_SMALL, SUB);
        }
    }
    Box(r, foot, aw - r, 1, 0, EDGE);
    if (page == 3) {
        if (info.state == INSTALL_SCAN)
            button(aw - 117, foot + 13, 95, "Annulla", 0);
    } else {
        if (page != 6)
            button(x, foot + 13, 85, page == 0 || page >= 4 ? "Chiudi" : "Indietro", 0);
        const char *label = page == 2 ? "Installa" : page == 4 ? "Spegni" : page == 5 ? "Torna ai dischi" : page == 6 ? "Fine" :
                            "Continua";
        int enabled = (page != 1 || can()) && (page != 2 || confirm);
        if (enabled)
            button(aw - 151, foot + 13, 127, label, 1);
        else {
            Box(aw - 151, foot + 13, 127, 26, 7, SOFT);
            Center(aw - 88, foot + 17, label, F_SMALL, SUB);
        }
    }
    finish();
}
void app_main(void)
{
    win(694, 394, "Installa tinymacOS");
    refresh();
    paint();
    for (;;) {
        Event e;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            dirty = 1;
            if (baseevent(&e))
                continue;
            if (e.type == EV_CLOSE) {
                if (page != 3)
                    quit();
                else if (info.state == INSTALL_SCAN && sc(S_INSTALL, 3, 0, 0, 0) == 0)
                    quit();
            }
            if (e.type == EV_KEY) {
                if (e.a == 27)
                    back();
                else if (e.a == '\n')
                    next();
                else if (page == 2 && e.a == ' ')
                    confirm = !confirm;
                else if (page == 1 && (e.a == K_UP || e.a == K_DOWN)) {
                    if (info.count)
                        choice = MAX(0, MIN(info.count - 1, choice + (e.a == K_UP ? -1 : 1)));
                }
            }
            if (e.type == EV_CLICK) {
                int foot = ah - 52, x = rail() + 24;
                if (e.b >= foot + 9 && e.b <= foot + 44) {
                    if (e.a >= aw - 151) {
                        if (page == 3)
                            back();
                        else
                            next();
                    } else if (e.a >= x && e.a <= x + 85)
                        back();
                } else if (e.a >= x && e.a < aw - 21 && page == 1 && e.b >= 98 && e.b < foot) {
                    int i = MAX(0, choice - MAX(1, (foot - 104) / 49) + 1) + (e.b - 98) / 49;
                    if (i < info.count)
                        choice = i;
                } else if (page == 2 && e.b >= foot - 66 && e.b < foot - 18 && e.a >= x)
                    confirm = !confirm;
            }
        }
        if (page == 3) {
            if (sc(S_INSTALL, 2, 0, 0, 0) >= 0 || info.live) {
                refresh();
                dirty = 1;
            }
        }
        if (dirty) {
            paint();
            dirty = 0;
        }
        slp(page == 3 ? 1 : 4);
    }
}
