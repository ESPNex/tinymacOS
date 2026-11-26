#include "ui.h"
// Time Machine: backup front-end over the snapshot subsystem (S_SNAP).
// Snapshots share immutable blocks; writes preserve prior versions by CoW.
// A local checkpoint is not protection against failure of the physical disk.
static SnapInfo sn[8];
static FsInfo fi;
static EncInfo ei;
static int nsn, sel = -1, dialog, op, pagebase, more;
static char msg[64], snapname[36];

static void refresh(void)
{
    nsn = sc(S_SNAP, 7, pagebase, sn, 8);
    while(nsn==0&&pagebase){pagebase=MAX(0,pagebase-8);nsn=sc(S_SNAP,7,pagebase,sn,8);}
    SnapInfo next;more=nsn==8&&sc(S_SNAP,7,pagebase+nsn,&next,1)>0;
    if (nsn < 0)
        nsn = 0;
    if (sc(S_SNAP, 5, 0, &fi, 0))
        memset(&fi, 0, sizeof fi);
    sc(S_SNAP, 6, 0, 0, &ei);
    if (sel >= nsn)
        sel = -1;
}
static void timeline_page(int direction)
{
 if((direction>0&&!more)||(direction<0&&!pagebase))return;
 if(direction>0&&pagebase>0x7fffffff-16)return;
 pagebase=MAX(0,pagebase+direction*8);sel=-1;refresh();
}
static void dt(char *s, int cap, unsigned v)
{
    char z[3];
    addnum(s, ((v >> 25) & 127) + 1980, cap);
    append(s, "-", cap);
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
// Automatic name: Backup-YYYY-MM-DD-HHMM from the session clock.
static void mkname(void)
{
    char z[3];
    strset(snapname, "Backup-", sizeof snapname);
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
    append(snapname,"-",sizeof snapname);z[0]='0'+si.second/10;z[1]='0'+si.second%10;append(snapname,z,sizeof snapname);
}
static void yes(void)
{
    if (dialog != 1)
        return;
    dialog = 2;
    strset(msg, "Operazione in corso…", sizeof msg);
}
static void paint(void)
{
    begin();
    Text(15, 9, "Copia di sicurezza del disco", F_SMALL, SUB);
    Text(14, 29, "Time Machine", F_TITLE, TX);
    button(aw - 178, 20, 160, "Esegui backup ora", 1);
    // Left card: volume, usage, snapshot count, encryption.
    Box(14, 64, 196, 170, 13, CARD);
    Icon(32, 78, 44, 39);
    Text(88, 84, "Macintosh HD", F_BOLD, TX);
    Text(88, 104, "- Dati · disco interno", F_SMALL, SUB);
    Box(30, 134, 164, 9, 5, SOFT);
    int fill = fi.total ? (int)((u64)fi.used * 164 / fi.total) : 0;
    if (fill < 3 && fi.used)
        fill = 3;
    if (fill > 164)
        fill = 164;
    Box(30, 134, fill, 9, 5, ACC);
    char z[56] = "";
    addnum(z, fi.used / 1024, 56);
    append(z, " KB su ", 56);
    addnum(z, fi.total / 1024, 56);
    append(z, " KB", 56);
    Text(30, 152, z, F_SMALL, SUB);
    Text(30, 178, "Backup", F_SMALL, SUB);
    z[0] = 0;
    addnum(z, fi.snaps, 56);
    Text(162, 177, z, F_BODY, TX);
    Text(30, 202, "Crittografia", F_SMALL, SUB);
    Text(118, 201, ei.state ? (ei.locked ? "Attiva (bloccata)" : "Attiva (sbloccata)") : "Disattiva",
         F_BODY, TX);
    Text(30, 224, "I backup ereditano la crittografia", F_MICRO, SUB);
    // Right: backup timeline.
    Text(226, 70, "Linea temporale dei backup", F_SMALL, SUB);
    if(pagebase||more){button(aw-78,64,28,"<",0);button(aw-44,64,28,">",0);}
    if (!nsn) {
        Text(236, 116, "Nessun backup presente.", F_BODY, SUB);
        Text(236, 146, "«Esegui backup ora» crea il primo backup.", F_SMALL, SUB);
    }
    for (int i = 0; i < nsn; i++) {
        int Y = 92 + i * 36;
        Box(226, Y, aw - 240, 30, 7, sel == i ? SEL : (i & 1 ? SOFT : CARD));
        char t[24] = "";
        dt(t, 24, sn[i].date);
        Text(238, Y + 4, sn[i].date?t:"Snapshot CoW", F_SMALL, TX);
        Text(238, Y + 17, sn[i].name, F_MICRO, SUB);
        char idz[12] = "id ";
        addnum(idz, sn[i].id, 12);
        Text(aw - 74, Y + 9, idz, F_MICRO, SUB);
    }
    button(aw - 306, 386, 108, "Ripristina", 1);
    button(aw - 190, 386, 84, "Verifica", 0);
    button(aw - 98, 386, 84, "Elimina", 0);
    if (msg[0])
        Text(16, ah - 25, msg, F_MICRO, EMP);
    if (dialog) {
        int tw = MIN(430, aw - 48), xx = (aw - tw) / 2, yy = (ah - 190) / 2;
        Sheet(xx, yy, tw, 190, 14);
        Icon(xx + 20, yy + 16, 42, 42);
        Text(xx + 76, yy + 24, op == 2 ? "Ripristina backup" : "Elimina backup", F_CAPTION, TX);
        Text(xx + 20, yy + 76, op == 2 ? "Il volume torna allo stato del backup." :
             "L'immagine del backup verrà eliminata.", F_SMALL, TX);
        Text(xx + 20, yy + 100, op == 2 ? "I file creati dopo il backup verranno persi." :
             "Questa azione non è reversibile.", F_SMALL, SUB);
        button(xx + tw - 216, yy + 145, 88, "Annulla", 0);
        button(xx + tw - 118, yy + 145, 96, dialog == 2 ? "Attendi…" : "Continua", 1);
        finish();
        return;
    }
    finish();
}
void app_main(void)
{
    win(600, 430, "Time Machine");
    refresh();
    paint();
    for (;;) {
        Event e;
        int changed = 0;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            changed = 1;
            if (baseevent(&e))
                continue;
            if (e.type == EV_CLOSE)
                quit();
            if (dialog) {
                if (e.type == EV_KEY) {
                    if (e.a == 27)dialog = 0;
                    if (e.a == '\n')yes();
                }
                if (e.type == EV_CLICK) {
                    int tw = MIN(430, aw - 48), xx = (aw - tw) / 2, yy = (ah - 190) / 2;
                    if (e.b >= yy + 140 && e.b < yy + 184) {
                        if (e.a >= xx + tw - 216 && e.a < xx + tw - 124)dialog = 0;
                        else if (e.a >= xx + tw - 118 && e.a < xx + tw - 18)yes();
                    }
                }
                continue;
            }
            if(e.type==EV_SCROLL){timeline_page(e.a<0?1:-1);continue;}
            if(e.type==EV_KEY&&(e.a==K_LEFT||e.a==K_RIGHT)){timeline_page(e.a==K_RIGHT?1:-1);continue;}
            if(e.type==EV_CLICK&&(pagebase||more)&&e.b>=64&&e.b<90&&e.a>=aw-78){timeline_page(e.a<aw-48?-1:1);continue;}
            if (e.type != EV_CLICK)
                continue;
            if (e.b >= 16 && e.b < 56 && e.a >= aw - 178 && e.a < aw - 12) {
                mkname();
                int r = sc(S_SNAP, 1, snapname, 0, 0);
                strset(msg, r ? "Backup non riuscito." : "Backup creato.", sizeof msg);
                refresh();
            } else if (e.a >= 226 && e.b >= 92 && e.b < 92 + 8 * 36) {
                int i = (e.b - 92) / 36;
                if (i >= 0 && i < nsn && (e.b - 92) % 36 < 30) {
                    sel = i;
                    msg[0] = 0;
                }
            } else if (e.b >= 380 && e.b < 430) {
                if (e.a >= aw - 306 && e.a < aw - 194) {
                    if (sel < 0)strset(msg, "Seleziona un backup.", sizeof msg);
                    else {
                        op = 2;
                        dialog = 1;
                    }
                } else if (e.a >= aw - 190 && e.a < aw - 102) {
                    if (sel < 0)strset(msg, "Seleziona un backup.", sizeof msg);
                    else {
                        int r = sc(S_SNAP, 3, sn[sel].name, 0, 0);
                        strset(msg, r ? "Backup danneggiato." : "Backup valido (crc32c).", sizeof msg);
                    }
                } else if (e.a >= aw - 98 && e.a < aw - 10) {
                    if (sel < 0)strset(msg, "Seleziona un backup.", sizeof msg);
                    else {
                        op = 4;
                        dialog = 1;
                    }
                }
            }
        }
        if (changed)
            paint();
        if (dialog == 2) {
            int rr = sc(S_SNAP, op, sn[sel].name, 0, 0);
            dialog = 0;
            if (!rr)
                strset(msg, op == 2 ? "Backup ripristinato." : "Backup eliminato.", sizeof msg);
            else
                strset(msg, "Operazione non riuscita.", sizeof msg);
            refresh();
            paint();
        }
        slp(4);
    }
}
