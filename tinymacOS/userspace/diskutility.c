#include "volume.h"
void vault_panel(void);
static DiskInfo d;
static SnapInfo sn[8];
static FsInfo fi;
static EncInfo ei;
static int sel, view, dialog, op, nsn;
static char msg[130], snapname[32];
static void refresh(void)
{
    sc(S_DISK, 0, &d, 0, 0);
    if (d.count && sel >= d.count)sel = d.count - 1;
    if (sc(S_SNAP, 6, 0, 0, &ei))
        memset(&ei, 0, sizeof ei);
    nsn = sc(S_SNAP, 0, 0, sn, 8);
    if (nsn < 0 || sc(S_SNAP, 5, 0, &fi, 0)) {
        nsn = 0;
        memset(&fi, 0, sizeof fi);
    }
    if (view >= 100 && view - 100 >= nsn)view = -1;
}
static int installed(void)
{
    return sel >= 0 && sel < d.count && d.disk[sel].layout == 2 && sel == d.current && !si.live_cd;
}
static int nvol(void)
{
    if (sel < 0 || sel >= d.count || !d.disk[sel].layout)return 0;
    return d.disk[sel].layout == 2 ? 4 : 2;
}
static void d2(char *s, int cap, unsigned v)
{
    addnum(s, ((v >> 25) & 127) + 1980, cap);
    append(s, "-", cap);
    char z[4];
    z[0] = "0123456789"[(((v >> 21) & 15) / 10)];
    z[1] = "0123456789"[(((v >> 21) & 15) % 10)];
    z[2] = 0;
    append(s, z, cap);
    append(s, "-", cap);
    z[0] = "0123456789"[(((v >> 16) & 31) / 10)];
    z[1] = "0123456789"[(((v >> 16) & 31) % 10)];
    append(s, z, cap);
    append(s, " ", cap);
    z[0] = "0123456789"[(((v >> 11) & 31) / 10)];
    z[1] = "0123456789"[(((v >> 11) & 31) % 10)];
    append(s, z, cap);
    append(s, ":", cap);
    z[0] = "0123456789"[(((v >> 5) & 63) / 10)];
    z[1] = "0123456789"[(((v >> 5) & 63) % 10)];
    append(s, z, cap);
}
static void start(int a)
{
    if (sel < 0 || sel >= d.count || !d.disk[sel].layout) {
        strset(msg, "Seleziona un disco con un volume tinymacOS.", sizeof msg);
        return;
    }
    if (a != 1 && !installed()) {
        strset(msg, "Questa funzione richiede l’avvio dal disco installato.", sizeof msg);
        return;
    }
    if (a == 4) {
        // Automatic APFS-style name: snapshot-YYYY-MM-DD-HHMM
        char z[4];
        strset(snapname, "snapshot-", sizeof snapname);
        addnum(snapname, si.year, sizeof snapname);
        append(snapname, "-", sizeof snapname);
        z[0] = "0123456789"[si.month / 10];
        z[1] = "0123456789"[si.month % 10];
        z[2] = 0;
        append(snapname, z, sizeof snapname);
        append(snapname, "-", sizeof snapname);
        z[0] = "0123456789"[si.day / 10];
        z[1] = "0123456789"[si.day % 10];
        append(snapname, z, sizeof snapname);
        append(snapname, "-", sizeof snapname);
        z[0] = "0123456789"[si.hour / 10];
        z[1] = "0123456789"[si.hour % 10];
        append(snapname, z, sizeof snapname);
        z[0] = "0123456789"[si.minute / 10];
        z[1] = "0123456789"[si.minute % 10];
        append(snapname, z, sizeof snapname);
    }
    if ((a == 5 || a == 7) && (view < 100 || view - 100 >= nsn)) {
        strset(msg, "Seleziona uno snapshot nella colonna laterale.", sizeof msg);
        return;
    }
    dialog = 1;
    op = a;
}
static void verify_now(void)
{
    if (view < 100 || view - 100 >= nsn) {
        strset(msg, "Seleziona uno snapshot nella colonna laterale.", sizeof msg);
        return;
    }
    int r = sc(S_SNAP, 3, sn[view - 100].name, 0, 0);
    strset(msg, r ? "Verifica fallita: immagine danneggiata." : "Snapshot valido: integrità crc32c verificata.",
           sizeof msg);
}
static void yes(void)
{
    if (dialog != 1)return;
    dialog = 2;
    strset(msg, "Operazione in corso…", sizeof msg);
}
static const char *names[] = {"Macintosh HD", "Macintosh HD - Dati", "Recovery", "Sistema locale"};
static u32 colors[] = {0x65a9f3, 0x85c4bb, 0xd5aff0, 0xa6b5c6};
static int rail(void)
{
    return aw < 600 ? 145 : 176;
}
static void pair(int x, int y, const char *k, const char *v)
{
    Text(x, y, k, F_SMALL, SUB);
    Text(x + 116, y - 1, v, F_BODY, TX);
}
static void paint(void)
{
    begin();
    int r = rail(), x = r + 22, w = aw - x - 19;
    int sh = view >= 100 && view - 100 < nsn;
    Box(0, 0, r, ah, 0, PANEL);
    Box(r, 0, 1, ah, 0, EDGE);
    Box(r + 1, 0, aw - r - 1, ah, 0, CARD);
    Text(15, 18, "Interni", F_BOLD, SUB);
    int y = 54;
    for (int i = 0; i < d.count; i++) {
        if (i == sel && view < 0)Box(7, y - 3, r - 14, 30, 7, SEL);
        Icon(12, y, 25, 33);
        char s[30] = "Disco ";
        addnum(s, i + 1, 30);
        Text(46, y + 3, s, F_BODY, TX);
        y += 33;
        if (i == sel && d.disk[i].layout) {
            int n = d.disk[i].layout == 2 ? 4 : 2;
            for (int j = 0; j < n; j++, y += 29) {
                if (j == view)Box(10, y - 2, r - 17, 27, 7, SEL);
                Box(23, y + 8, 7, 7, 3, colors[j]);
                Text(38, y + 3, j == 1 ? "Dati" : names[j], F_SMALL, TX);
            }
            if (i == sel && d.disk[i].layout == 2) {
                for (int k = 0; k < nsn; k++, y += 26) {
                    if (100 + k == view)Box(10, y - 2, r - 17, 24, 7, SEL);
                    Box(31, y + 6, 6, 6, 3, 0xd5aff0);
                    char t[24];
                    int q = 0;
                    for (; sn[k].name[q] && q < 17; q++)t[q] = sn[k].name[q];
                    if (sn[k].name[q]) {
                        t[q++] = '.';
                        t[q++] = '.';
                        t[q++] = '.';
                    }
                    t[q] = 0;
                    Text(44, y + 2, t, F_MICRO, TX);
                }
            }
        }
    }
    button(x, 13, 57, "SOS", 0);
    button(x + 65, 13, 89, "Copia dati", 0);
    button(x + 162, 13, 92, "Recupero…", 0);
    button(x + 262, 13, 104, "Nuovo snapshot", 0);
    Box(r + 1, 52, aw - r - 1, 1, 0, EDGE);
    if (!d.count) {
        Icon(x, 88, 50, 39);
        Text(x, 160, "Nessun disco collegato", F_CAPTION, TX);
        Text(x, 202, "Collega un disco e riapri Utility Disco.", F_SMALL, SUB);
        finish();
        return;
    }
    if (sh) {
        int k = view - 100;
        Icon(x, 75, 57, 39);
        Text(x + 69, 77, "Snapshot", F_CAPTION, TX);
        Text(x + 70, 109, sn[k].name, F_SMALL, SUB);
        char s[64] = "";
        addnum(s, sn[k].id, 64);
        pair(x, 155, "ID", s);
        s[0] = 0;
        d2(s, 64, sn[k].date);
        pair(x, 188, "Data", s);
        s[0] = 0;
        sizebadge(s, 1680 * 512, 64);
        pair(x, 221, "Dimensione", s);
        pair(x, 254, "Stato", "Immutabile · crc32c");
        button(x, 282, 110, "Ripristina", 1);
        button(x + 118, 282, 96, "Verifica", 0);
        button(x + 222, 282, 96, "Elimina", 0);
        Box(r + 1, ah - 45, aw - r - 1, 1, 0, EDGE);
        Text(x, ah - 31, msg[0] ? msg : "Lo snapshot è una copia puntuale del volume.", F_SMALL, SUB);
        if (dialog) {
            int tw = MIN(440, aw - 48), xx = (aw - tw) / 2, yy = (ah - 196) / 2;
            Sheet(xx, yy, tw, 196, 14);
            Icon(xx + 20, yy + 17, 39, 39);
            Text(xx + 73, yy + 23, op == 5 ? "Ripristina snapshot" : op == 7 ? "Elimina snapshot" : "Nuovo snapshot",
                 F_CAPTION, TX);
            Text(xx + 20, yy + 75, op == 5 ? "Il volume torna allo stato puntuale dello snapshot." :
                 op == 7 ? "L’immagine verrà eliminata definitivamente." : "Viene creata una copia puntuale del volume.",
                 F_SMALL, TX);
            Text(xx + 20, yy + 99, op == 5 ? "I file creati dopo lo snapshot verranno persi." : op == 7 ?
                 "I dati dei file non vengono modificati." : "Il contenuto non cambia più al variare dei file.",
                 F_SMALL, SUB);
            button(xx + tw - 218, yy + 149, 88, "Annulla", 0);
            button(xx + tw - 119, yy + 149, 97, dialog == 2 ? "Attendi…" : "Continua", 1);
        }
        finish();
        return;
    }
    Icon(x, 75, 57, view == -1 ? 33 : 39);
    Text(x + 69, 77, view < 0 ? "Disco interno" : names[view], F_CAPTION, TX);
    Text(x + 70, 109, view < 0 ? d.disk[sel].model : "Volume del disco interno", F_SMALL, SUB);
    int mb = d.disk[sel].sectors / 2048, z[] = {20, MAX(0, mb - 45), 4, 20};
    if (d.disk[sel].layout != 2)z[1] = MAX(0, mb - 21);
    char s[64] = "";
    if (view == 1 && installed()) {
        addnum(s, fi.used / 1024, 64);
        append(s, " KB usati", 64);
    } else
        addnum(s, view >= 0 ? z[view] : mb, 64);
    append(s, " MiB", 64);
    pair(x, 155, view == 1 && installed() ? "Utilizzo" : "Capacità", s);
    pair(x, 188, "Formato", view < 0 ? (d.disk[sel].layout ? "Mappa partizione GUID" : "Non riconosciuto") : view == 1 ?
         "BTRFS subset (struttura APFS)" : view == 3 ? "Immagine di ripristino" : "Volume EFI (FAT16)");
    if (view == 1 && installed()) {
        char k[24] = "";
        addnum(k, fi.snaps, 24);
        pair(x, 221, "Snapshot", k);
        pair(x, 254, "Contenitore", "cont0");
        pair(x + w / 2, 254, "Crittografia",
             ei.state ? (ei.locked ? "Bloccata" : "Attiva") : "Disattiva");
    }
    int sw = w - 2;
    if (d.disk[sel].layout) {
        int n = d.disk[sel].layout == 2 ? 4 : 2, off = 0;
        for (int j = 0; j < n; j++) {
            int len = j == n - 1 ? sw - off : MAX(4, sw * z[j] / MAX(1, mb));
            Box(x + off, 275, len, 14, 2, colors[j]);
            off += len;
        }
        for (int j = 0; j < n; j++) {
            int xx = x + (j % 2) * (w / 2), yy = 300 + j / 2 * 24;
            Box(xx, yy + 5, 8, 8, 4, colors[j]);
            Text(xx + 14, yy, names[j], F_SMALL, SUB);
        }
    } else {
        Box(x, 275, sw, 14, 6, SOFT);
        Text(x, 305, "Nessun volume tinymacOS riconosciuto.", F_SMALL, SUB);
    }
    Box(r + 1, ah - 45, aw - r - 1, 1, 0, EDGE);
    Text(x, ah - 31, msg[0] ? msg : d.disk[sel].backup ? "Copia dei dati disponibile in Recupero." :
         view == 1 ? "Crea uno snapshot con «Nuovo snapshot»: resta incolonnato a sinistra." :
         "Seleziona un volume per vederne i dettagli.", F_SMALL, SUB);
    if (dialog) {
        int tw = MIN(440, aw - 48), xx = (aw - tw) / 2, yy = (ah - 196) / 2;
        Sheet(xx, yy, tw, 196, 14);
        Icon(xx + 20, yy + 17, 39, 39);
        Text(xx + 73, yy + 23, op == 1 ? "Esegui SOS" : op == 2 ? "Crea una copia locale" : op == 3 ? "Apri Recupero" :
             "Nuovo snapshot", F_CAPTION, TX);
        Text(xx + 20, yy + 75, op == 1 ? "Controlla i dati e ripara la mappa delle partizioni." : op == 2 ?
             "Salva documenti, account e preferenze sul disco." : op == 3 ?
             "Il computer si riavvierà in Recupero tinymacOS." :
             "Crea una copia puntuale del volume con nome automatico.", F_SMALL, TX);
        Text(xx + 20, yy + 99, op == 2 ? "L’eventuale copia precedente verrà sostituita." : op == 3 ?
             "Prima di continuare, salva i documenti aperti." : op == 4 ? snapname : "Non scollegare il disco.",
             F_SMALL, SUB);
        button(xx + tw - 218, yy + 149, 88, "Annulla", 0);
        button(xx + tw - 119, yy + 149, 97, dialog == 2 ? "Attendi…" : "Continua", 1);
    }
    finish();
}
void app_main(void)
{
    char argument[PATHLEN];sc(S_STARTARG,argument,0,0,0);if(eq(argument,"@vault")){vault_panel();return;}
    win(693, 384, "Utility Disco");
    refresh();
    sel = d.current >= 0 ? d.current : 0;
    view = -1;
    paint();
    for (;;) {
        Event e;
        int changed = 0;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            changed = 1;
            if (baseevent(&e))continue;
            if (e.type == EV_CLOSE)quit();
            if (dialog) {
                if (e.type == EV_KEY) {
                    if (e.a == 27)dialog = 0;
                    if (e.a == '\n')yes();
                }
                if (e.type == EV_CLICK) {
                    int tw = MIN(440, aw - 48), xx = (aw - tw) / 2, yy = (ah - 196) / 2;
                    if (e.b >= yy + 145 && e.b < yy + 181) {
                        if (e.a >= xx + tw - 119 && e.a < xx + tw - 22)yes();
                        else if (e.a >= xx + tw - 220 && e.a < xx + tw - 130)dialog = 0;
                    }
                }
                continue;
            }
            if (e.type == EV_KEY) {
                if (e.a == 's')start(1);
                if (e.a == 'b')start(2);
                if (e.a == 'r')start(3);
                if (e.a == 'n')start(4);
                if (e.a == K_UP || e.a == K_DOWN) {
                    int rows = 1 + nvol() + (nvol() == 4 ? nsn : 0);
                    int cur = view + 1;
                    cur += e.a == K_DOWN ? 1 : -1;
                    if (cur < 0)cur = 0;
                    if (cur >= rows)cur = rows - 1;
                    view = cur - 1;
                    msg[0] = 0;
                }
            }
            if (e.type == EV_CLICK) {
                int r = rail(), x = r + 22;
                if (e.a < r) {
                    int y = 54;
                    for (int i = 0; i < d.count; i++) {
                        if (e.b >= y - 3 && e.b < y + 28) {
                            sel = i;
                            view = -1;
                            msg[0] = 0;
                            break;
                        }
                        y += 33;
                        if (i == sel && d.disk[i].layout) {
                            int n = d.disk[i].layout == 2 ? 4 : 2;
                            if (e.b >= y - 2 && e.b < y + n * 29 - 2) {
                                view = (e.b - y + 2) / 29;
                                msg[0] = 0;
                                break;
                            }
                            y += n * 29;
                            if (i == sel && d.disk[i].layout == 2 && nsn) {
                                if (e.b >= y - 2 && e.b < y + nsn * 26 - 2) {
                                    view = 100 + (e.b - y + 2) / 26;
                                    if (view - 100 >= nsn)view = 100 + nsn - 1;
                                    msg[0] = 0;
                                    break;
                                }
                                y += nsn * 26;
                            }
                        }
                    }
                } else if (e.b >= 10 && e.b < 43) {
                    if (e.a >= x && e.a < x + 58)start(1);
                    else if (e.a >= x + 65 && e.a < x + 155)start(2);
                    else if (e.a >= x + 162 && e.a < x + 255)start(3);
                    else if (e.a >= x + 262 && e.a < x + 366)start(4);
                } else if (view >= 100 && e.b >= 278 && e.b < 316) {
                    if (e.a >= x && e.a < x + 110)start(5);
                    else if (e.a >= x + 118 && e.a < x + 214)verify_now();
                    else if (e.a >= x + 222 && e.a < x + 318)start(7);
                }
            }
        }
        if (changed)paint();
        if (dialog == 2) {
            int rr;
            const char *okmsg;
            if (op >= 4) {
                const char *nm = op == 4 ? snapname : sn[view - 100].name;
                rr = sc(S_SNAP, op - 3, nm, 0, 0);
                okmsg = op == 4 ? "Snapshot creato." : op == 5 ? "Volume ripristinato." : "Snapshot eliminato.";
            } else {
                rr = sc(S_DISK, op, sel, 0, 0);
                okmsg = op == 2 ? "Copia locale salvata e verificata." : "SOS completato. Il volume è valido.";
            }
            dialog = 0;
            if (op == 4 && !rr) {
                strset(msg, "Snapshot creato: ora visibile nella colonna laterale.", sizeof msg);
            } else if (op == 5 && !rr) {
                strset(msg, "Volume ripristinato nello stato dello snapshot.", sizeof msg);
            } else if (op == 7 && !rr) {
                strset(msg, "Snapshot eliminato.", sizeof msg);
            } else
                strset(msg, rr ? "Operazione non riuscita. Controlla il volume in Recupero." : okmsg, sizeof msg);
            refresh();
            paint();
        }
        slp(4);
    }
}
