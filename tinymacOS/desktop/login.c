#include "text_input.h"
#include "desk.h"
#include "date.h"
#include "activity.h"
// The login surface is drawn directly by the compositor, before asking for any
// client window. The login loop never forwards raw keyboard events to clients.
static SessionInfo account;
static AuthRequest form;
static char confirm[65], notice[112], setup_code[33];
static int field, confirmPower, blink = -1, minute = -1, lastPhase = -1, shake, reset_arm;
static u64 spinStart, caretTick;
static int carets[3];
static u32 glow[1920 * 1080];
int gate;
static void wipe(void *p, int n)
{
    volatile u8 *q = p;
    while (n--)*q++ = 0;
}
static void notice_set(const char *s)
{
    strset(notice, s, sizeof notice);
    dirty = 1;
}
void login_enter(void)
{
    gate = 1;
    field = 0;
    memset(carets,0,sizeof carets);caretTick=sys.ticks;
    confirmPower = 0;
    shake = 0;
    reset_arm = 0;
    blink = minute = lastPhase = -1;
    wipe(&form, sizeof form);
    wipe(confirm, sizeof confirm);
    wipe(setup_code, sizeof setup_code);
    notice[0] = 0;
    sc(S_SESSION, &account, 0, 0, 0);
    for (int y = 0; y < H; y++) {
        int d = y * 256 / H;
        int a = d < 96 ? 86 - d / 2 : d > 136 ? 37 + (d - 136) / 2 : 37;
        for (int x = 0; x < W; x++)glow[y * W + x] = blend(back[y * W + x], 0x0c172d, a);
    }
    dirty = 1;
    if (account.needs_seed)trace("host code ready");
    trace(account.state == SESSION_SETUP ? "setup ready" : account.state == SESSION_LOCKED ? "login ready" :
          "account unavailable");
}
static void clocktext(char t[24], char day[88])
{
    char n[20];
    t[0] = 0;
    if (sys.hour < 10)append(t, "0", 24);
    number(n, sys.hour);
    append(t, n, 24);
    append(t, ":", 24);
    if (sys.minute < 10)append(t, "0", 24);
    number(n, sys.minute);
    append(t, n, 24);
    int m = clampi(sys.month, 1, 12);
    strset(day, dayname(weekday(sys.year, m, MAX(1u, sys.day))), 88);
    append(day, " ", 88);
    number(n, sys.day);
    append(day, n, 88);
    append(day, " ", 88);
    char mo[24];
    strset(mo, monthname(m), 24);
    mo[0] += 'a' - 'A';
    append(day, mo, 88);
}
static int setup(void)
{
    return account.state == SESSION_SETUP;
}
static int avatar_y(void)
{
    return setup() ? H / 2 - 94 : H * 61 / 100 - (H < 680 ? 43 : 12);
}
static Rect inputbox(int i)
{
    int cx = W / 2, y = avatar_y() + (setup() ? 126 : 137);
    int w = setup() ? 254 : 244;
    return (Rect) {
        cx - w / 2 + (shake ? ((shake % 4 < 2) ? 4 : -4) : 0), y + i * 41, w, 34
    };
}
static Rect submitbox(void)
{
    Rect r = inputbox(setup() ? 2 : 0);
    if (setup())return (Rect) {
        W / 2 - 92, r.y + 44, 184, 34
    };
    return (Rect) {
        r.x + r.w - 34, r.y, 34, 34
    };
}
// Recovery action on the fail-closed screen: zero the unreadable account so
// enrollment can start again. Documents and snapshots are not touched.
// Two-step confirm: the first click arms the button, the second executes.
static Rect resetbox(void)
{
    return (Rect) {
        W / 2 - 138, avatar_y() + 88 + 74, 276, 36
    };
}
// Only shown on a genuinely blank volume if all automatic sources decline.
static Rect seed_box(int button)
{
    int w = button ? 150 : 352;
    return (Rect) {
        W / 2 - w / 2, avatar_y() + 158 + (button ? 44 : 0), w, 34
    };
}
static void seed_submit(void)
{
    if (strlen(setup_code) != 32 || sc(S_SETUP_CODE, setup_code, 0, 0, 0) < 0) {
        notice_set("Usa le 32 cifre di un nuovo codice generato sul computer host.");
        return;
    }
    wipe(setup_code, sizeof setup_code);
    field = 0;
    sc(S_SESSION, &account, 0, 0, 0);
    notice_set("");
}
static void seed_draw(void)
{
    int cx = W / 2, y = avatar_y() + 68;
    center(cx, y + 8, "Codice di configurazione", F_CAPTION, 0xffffff);
    center(cx, y + 34, "Per proteggere il tuo account serve un codice casuale.", F_SMALL, 0xf4f5fa);
    center(cx, y + 55, "Segui le istruzioni nella guida di avvio del supporto.", F_SMALL, 0xffffff);
    Rect r = seed_box(0), b = seed_box(1);
    roundr(r.x, r.y, r.w, r.h, 17, 0x10233d, 177);
    stroke(r, 17, 0xffffff, 180);
    if (!setup_code[0])text(r.x + 16, r.y + 8, "32 cifre esadecimali · non è la password", F_SMALL, 0xe9ecf6);
    else {
        text(r.x + 16, r.y + 7, setup_code, F_MONO, 0xffffff);
        if (blink)rect(r.x + 18 + textw(setup_code, F_MONO), r.y + 9, 1, 16, 0xffffff);
    }
    roundr(b.x, b.y, b.w, b.h, 17, 0xf4f7ff, 233);
    center(cx, b.y + 8, "Continua", F_SMALL, 0x1c2b45);
    center(cx, b.y + b.h + 9, notice[0] ? notice :
           "Usa un codice nuovo, generato sul computer di origine.", F_SMALL, 0xf9f3ed);
}
static Rect powerbox(int j)
{
    return (Rect) {
        W / 2 - 70 + j * 82, H - 71, 57, 58
    };
}
static unsigned hovermask(void)
{
    unsigned m=0;
    for(int j=0;j<2;j++)m|=(unsigned)inside(mx,my,powerbox(j))<<(j+1);
    m|=(unsigned)inside(mx,my,submitbox())<<3;
    m|=(unsigned)inside(mx,my,resetbox())<<4;
    return m;
}
static void field_draw(int i)
{
    Rect r = inputbox(i);
    const char *s = setup() && i == 0 ? form.name : setup() && i == 2 ? confirm : form.password;
    int secret = !setup() || i != 0, len=strlen(s);
    carets[i]=MAX(0,MIN(carets[i],len));
    int col=text_columns(s,carets[i]),startcol=MAX(0,col-(secret?18:21));
    roundr(r.x,r.y,r.w,r.h,17,i==field?0x284969:0x172c45,238);
    const char *hint = !setup() ? "Inserisci la password" : i == 0 ? "Nome utente" : i == 1 ?
                       "Password · almeno 8 caratteri" : "Conferma password";
    int cx=r.x+16;
    if (!s[0])text(r.x+(i==field?24:17),r.y+8,hint,F_SMALL,0xf3f5fc);
    else if (secret) {
        int n = MIN(19, text_columns(s,len)-startcol);
        for (int j = 0; j < n; j++)roundr(r.x + 16 + j * 9, r.y + 14, 5, 5, 2, 0xc4d8ec, 245);
        cx=r.x+16+(col-startcol)*9;
    } else {
        int start=text_offset(s,0,len,startcol),end=len;
        char visible[128],prefix[128];
        for(;;){memcpy(prefix,s+start,carets[i]-start);prefix[carets[i]-start]=0;
            if(textw(prefix,F_BODY)<=r.w-38||start>=carets[i])break;start=text_next(s,start);}
        for(;;){memcpy(visible,s+start,end-start);visible[end-start]=0;
            if(textw(visible,F_BODY)<=r.w-32||end<=carets[i])break;end=text_prev(s,end);}
        text(r.x+16,r.y+7,visible,F_BODY,0xffffff);cx=r.x+16+textw(prefix,F_BODY);
    }
    if(i==field&&blink&&!account.working)rect(MIN(r.x+r.w-12,cx),r.y+8,1,17,0xffffff);
}
void login_draw(void)
{
    memcpy(canvas, glow, (size_t)W * H * 4);
    int cx = W / 2;
    char t[24], day[88];
    clocktext(t, day);
    center(cx, H / 11, day, F_CAPTION, 0xfbf6f0);
    center(cx, H / 11 + 18, t, F_CLOCK, 0xfffaf4);
    int y = avatar_y(), sz = setup() ? 68 : 88;
    avatar(cx - sz / 2, y, sz);
    stroke((Rect) {
        cx - sz / 2 + 1, y + 1, sz - 2, sz - 2
    }, sz / 2 - 1, 0xffffff, 130);
    if (account.needs_seed) {
        seed_draw();
    } else {
        center(cx, y + sz + 8, account.state == SESSION_BROKEN ? "Account non disponibile" : setup() ?
               (account.working ? form.name : "Crea un account") :
               account.name, setup() ? F_CAPTION : F_TITLE, 0xffffff);
        if (setup() &&
                !account.working)center(cx, y + sz + 36, sys.persistent ? "L’account resta su questo disco dopo il riavvio." :
                                            "Potrai conservare questo account installando il sistema.", F_SMALL, 0xe8edf8);
        if (account.working) {
            // Replace the form, not the boot bar. Input stays disabled until the real
            // authentication operation finishes; there is no minimum display delay.
            Rect r = inputbox(0);
            int cy = r.y + r.h / 2;
            login_activity(cx, cy, lastPhase < 0 ? 0 : lastPhase);
            center(cx, cy + 22, setup() ? "Creazione dell’account…" : "Verifica della password…", F_SMALL, 0xf4f5fa);
        } else if (account.state != SESSION_BROKEN) {
            for (int i = 0; i < (setup() ? 3 : 1); i++)field_draw(i);
            Rect r = submitbox();
            if (setup()) {
                roundr(r.x, r.y, r.w, r.h, 17, 0xf4f7ff, 233);
                center(cx, r.y + 8, "Crea account", F_SMALL, 0x1c2b45);
            } else {
                roundr(r.x + 4, r.y + 4, 26, 26, 13, 0xffffff, 65);
                text(r.x + 11, r.y + 5, "→", F_CAPTION, 0xffffff);
            }
            int yy = r.y + r.h + 9;
            center(cx, yy, notice[0] ? notice : (!setup() &&
                                                 !sys.persistent ? "Sessione temporanea" : ""), F_SMALL, 0xfff2e6);
        } else {
            center(cx, y + sz + 50, "Impossibile leggere l’account. Il disco non è stato reimpostato.", F_SMALL,
                   0xffe6d6);
            Rect b = resetbox();
            roundr(b.x, b.y, b.w, b.h, 17, reset_arm ? 0xffd9c4 : 0xf4f7ff, inside(mx, my, b) ? 255 : 233);
            center(cx, b.y + 9, reset_arm ? "Conferma: azzera l’account" : "Reimposta l’account", F_SMALL, 0x1c2b45);
            center(cx, b.y + b.h + 8, "Azzera solo l’account inutilizzabile: i documenti restano sul disco.", F_SMALL,
                   0xe8edf8);
        }
    }
    for (int j = 0; j < 2; j++) {
        Rect r = powerbox(j);
        int x = r.x + r.w / 2;
        roundr(x - 15, r.y, 31, 31, 15, 0xffffff, inside(mx, my, r) ? 75 : 42);
        icon(x - 10, r.y + 5, 21, j ? 35 : 36);
        center(x, r.y + 36, j ? "Spegni" : "Riavvia", F_SMALL, 0xf4f5fa);
    }
    if (confirmPower) {
        Rect r = {cx - 169, H / 2 - 67, 338, 134};
        shadow(r, 17, 80);
        glass(r, 17, MAT_SHEET);
        center(cx, r.y + 14, confirmPower == 2 ? "Spegnere il sistema?" : "Riavviare il sistema?", F_CAPTION, TX);
        center(cx, r.y + 47, "I documenti non salvati andranno persi.", F_SMALL, SUB);
        for (int j = 0; j < 2; j++) {
            Rect b = {r.x + 20 + j * 151, r.y + 81, 145, 30};
            button_face(b, j);
            center(b.x + 72, b.y + 7, j ? "Conferma" : "Annulla", F_SMALL, j ? (night ? 0x102040 : 0xffffff) : TX);
        }
    }
}
static void submit(void)
{
    if (account.working || account.wait_seconds || account.state == SESSION_BROKEN)return;
    if (setup()) {
        if (!form.name[0]) {
            field = 0;
            notice_set("Inserisci il tuo nome.");
            return;
        }
        if (text_columns(form.password,strlen(form.password)) < 8) {
            field = 1;
            notice_set("Scegli una password di almeno 8 caratteri.");
            return;
        }
        if (!eq(form.password, confirm)) {
            field = 2;
            wipe(confirm, sizeof confirm);
            notice_set("Le due password non coincidono. Riprova.");
            return;
        }
    }
    if (!form.password[0]) {
        notice_set("Inserisci la password per accedere.");
        return;
    }
    int r = sc(S_AUTH_BEGIN, &form, 0, 0, 0);
    wipe(form.password, sizeof form.password);
    wipe(confirm, sizeof confirm);
    if (r < 0) {
        sc(S_SESSION, &account, 0, 0, 0);
        notice_set(account.needs_seed ? "Inserisci il codice di configurazione per continuare." :
                   "Impossibile accedere. Riprova.");
        return;
    }
    sc(S_SESSION, &account, 0, 0, 0);
    spinStart = sys.ticks;
    lastPhase = -1;
    notice_set("");
}
static void do_reset(void)
{
    wipe(&form, sizeof form);
    form.op = 1;
    sc(S_AUTH_BEGIN, &form, 0, 0, 0);
    wipe(&form, sizeof form);
    sc(S_SESSION, &account, 0, 0, 0);
    notice_set("");
    dirty = 1;
}
void login_input(Event *e)
{
    if (e->type == EV_MOUSE || e->type == EV_POINTER_ABS) {
        unsigned hover=hovermask();
        pointer_position(e);
        cursorDirty=1;
        int down = (e->c & 1) && !(buttons & 1);
        buttons = e->c;
        if(hover!=hovermask() || down)dirty=1;
        if (!down || account.working)return;
        if (confirmPower) {
            int x = W / 2, y = H / 2;
            if (my >= y + 14 && my < y + 45 && mx >= x - 149 && mx < x + 149) {
                int v = confirmPower;
                confirmPower = 0;
                if (mx > x)sc(S_POWER, v == 1, 0, 0, 0);
            }
            return;
        }
        for (int j = 0; j < 2; j++)if (inside(mx, my, powerbox(j))) {
                confirmPower = j + 1;
                return;
            }
        if (account.state == SESSION_BROKEN) {
            if (inside(mx, my, resetbox())) {
                if (reset_arm) {
                    reset_arm = 0;
                    do_reset();
                } else {
                    reset_arm = 1;
                    dirty = 1;
                }
            }
            return;
        }
        if (account.needs_seed) {
            if (inside(mx, my, seed_box(1)))seed_submit();
            return;
        }
        if (inside(mx, my, submitbox())) {
            submit();
            return;
        }
        for (int i = 0; i < (setup() ? 3 : 1); i++)if (inside(mx, my, inputbox(i))){
            field=i;char *value=setup()&&i==0?form.name:setup()&&i==2?confirm:form.password;
            carets[i]=strlen(value);caretTick=sys.ticks;blink=1;
        }
        return;
    }
    if (e->type != EV_KEY)return;
#ifdef TMC_DEBUG
    {   /* key forensics: fires even when a volume job swallows the key */
        char _b[40], _n[12];
        strset(_b, "login k=", 40);
        number(_n, e->a); append(_b, _n, 40);
        append(_b, " b=", 40);
        number(_n, e->b); append(_b, _n, 40);
        if (account.working)append(_b, " drop=work", 40);
        trace(_b);
    }
#endif
    if (account.working)return;
    int k = e->a;
    caretTick=sys.ticks;blink=1;
    dirty = 1;
    if (confirmPower) {
        int v = confirmPower;
        if (k == 27 || k == '\n')confirmPower = 0;
        if (k == '\n')sc(S_POWER, v == 1, 0, 0, 0);
        return;
    }
    if (account.state == SESSION_BROKEN) {
        // Keyboard never resets the account: only the labeled button, twice.
        return;
    }
    if (account.needs_seed) {
        int n = strlen(setup_code);
        if (k == '\n') {
            seed_submit();
            return;
        }
        if (k == 27 || (e->b && (k == 'u' || k == 'a')))wipe(setup_code, sizeof setup_code);
        else if (k == 8 && n)setup_code[n - 1] = 0;
        else if (!e->b && n < 32 && ((k >= '0' && k <= '9') || (k >= 'a' && k <= 'f') || (k >= 'A' && k <= 'F'))) {
            setup_code[n] = (char)k;
            setup_code[n + 1] = 0;
        }
        notice[0] = 0;
        return;
    }
    if (k == '\t') {
        if (setup())field = (field + (e->c ? 2 : 1)) % 3;
        return;
    }
    if (k == '\n') {
        if (setup() && field < 2)field++;
        else submit();
        return;
    }
    if (k == 27) {
        wipe(form.password, sizeof form.password);
        wipe(confirm, sizeof confirm);
        notice[0] = 0;
        return;
    }
    char *p = setup() && field == 0 ? form.name : setup() && field == 2 ? confirm : form.password;
    int max = p == form.name ? 31 : 64;
    int *at=&carets[field];*at=MAX(0,MIN(*at,strlen(p)));
    if (e->b) {
        if (k == 'u' || k == 'a'){wipe(p, max + 1);*at=0;}
        return;
    }
    if(k==K_LEFT)*at=text_prev(p,*at);
    else if(k==K_RIGHT)*at=text_next(p,*at);
    else if(k==K_HOME)*at=0;
    else if(k==K_END)*at=strlen(p);
    else if(k==8)text_backspace(p,at);
    else if(k==K_DEL)text_delete(p,*at);
    else if(text_printable(k))text_insert(p,max+1,at,k);
    notice[0] = 0;
}
int login_step(void)
{
    u32 waited = account.wait_seconds;
    int old = account.working;
    if (account.working && lastPhase >= 0) {
        int res = 0;
        for (int i = 0; i < 5 && !res; i++)res = sc(S_AUTH_STEP, 0, 0, 0, 0);
        if (res == 1) {
            gate = 0;
            wipe(&form, sizeof form);
            wipe(confirm, sizeof confirm);
            dirty = 1;
            trace("desktop unlocked");
            return 1;
        }
        if (res < 0) {
            shake = sys.reduce_motion ? 0 : 8;
            notice_set(res == -2 ? "Password non corretta. Riprova." : "Impossibile salvare l’account.");
        }
    }
    sc(S_SESSION, &account, 0, 0, 0);
    int phase = !account.working ? -1 : sys.reduce_motion ? 0 : (int)((sys.ticks - spinStart) / 8 % ACTIVITY_RAYS);
    if (!!old != !!account.working || lastPhase != phase || waited != account.wait_seconds)dirty = 1;
    lastPhase = phase;
    if (shake) {
        shake--;
        dirty = 1;
    }
    if (minute != (int)sys.minute || blink != (int)(((sys.ticks-caretTick)/64%2)==0)) {
        minute = sys.minute;
        blink = ((sys.ticks-caretTick)/64%2)==0;
        dirty = 1;
    }
    return 0;
}
