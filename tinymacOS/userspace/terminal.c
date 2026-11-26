#include "path.h"
#define NH 40
#define NR 240
#define STAGES 6
#define INPUT 320
static char cwd[PATHLEN] = HOME, oldcwd[PATHLEN] = HOME, input[INPUT], saved[INPUT], history[NH][INPUT],
                           screen[NR][400], redir[PATHLEN], readfrom[PATHLEN];
static u8 kinds[NR];
static int nh, hpos, nt, scroll, cur, job, stage, count, mode, last, mark;
static ExecArgs cmd[STAGES];
static ExecResult result;
static Dirent matches[NSLOTS];
static Clipboard clipboard;
static int selection,selecting,anchor_row,anchor_byte,end_row,end_byte;

static int cols(void)
{
    return MIN(96, MAX(25, (aw - 31) / 8));
}
static void line(const char *s, int n, int kind)
{
    int i = nt++ % NR;
    n = MIN(398, n);
    memcpy(screen[i], s, n);
    screen[i][n] = 0;
    kinds[i] = kind;
#ifdef TMC_DEBUG
    /* Mirror every terminal row to COM1 as [user:Terminale] <row> so the
       black-box harness can assert command output without OCR. */
    {
        char b[400];
        memcpy(b, s, n);
        b[n] = 0;
        trace(b);
    }
#endif
}
static void output(const char *s, int n, int kind)
{
    char row[400];int bytes=0,column=0,c=cols();
    for(int i=0;i<n;){
        unsigned char a=s[i];int next=MIN(n,text_next(s,i)),z=next-i;
        if(a=='\n'){line(row,bytes,kind);bytes=column=0;i++;continue;}
        if(a=='\r'){i++;continue;}
        if(column==c){line(row,bytes,kind);bytes=column=0;}
        if(a<32){row[bytes++]=a=='\t'?' ':'.';}else{memcpy(row+bytes,s+i,z);bytes+=z;}
        column++;i=next;
    }
    if(bytes)line(row,bytes,kind);
}

static void note(const char *s)
{
    output(s, strlen(s), 2);
}
static void prompt(char *q, int cap)
{
    strset(q, si.user[0] ? si.user : "user", cap);
    append(q, "@tramontana ", cap);
    append(q, eq(cwd, HOME) ? "~" : eq(cwd, "/") ? "/" : leaf(cwd), cap);
    append(q, " % ", cap);
}
static void store(void)
{
    if (!*input)return;
    if (nh && !eq(history[(nh - 1) % NH], input))strset(history[nh++ % NH], input, INPUT);
    else if (!nh)strset(history[nh++ % NH], input, INPUT);
    hpos = nh;
}
static int parse(const char *s)
{
    memset(cmd, 0, sizeof cmd);
    stage = job = mode = 0;
    count = 1;
    readfrom[0] = redir[0] = 0;
    int target = 0;
    while (*s) {
        while (*s == ' ' || *s == '\t')s++;
        if (!*s)break;
        if (*s == '|') {
            if (target || !cmd[count - 1].argc || *redir || count == STAGES)return 0;
            count++;
            s++;
            continue;
        }
        if (*s == '>' || *s == '<') {
            if (target)return 0;
            target = *s++ == '<' ? 1 : 2;
            if (target == 2 && *s == '>') {
                s++;
                target = 3;
            }
            if ((target == 1 && (count != 1 || *readfrom)) || (target >= 2 && *redir))return 0;
            continue;
        }
        char w[PATHLEN];
        int q = 0, n = 0;
        while (*s && (q || (*s != ' ' && *s != '\t' && *s != '|' && *s != '<' && *s != '>'))) {
            int c = *s++;
            if ((c == '\'' || c == '"') && (!q || q == c)) {
                q = q ? 0 : c;
                continue;
            }
            if (c == '$' && q != '\'') {
                const char *v = 0;
                int used = 0;
                char status[24];
                number(status, last);
                if (*s == '?') {
                    v = status;
                    used = 1;
                } else {
                    const char *names[] = {"PWD", "HOME", "USER"};
                    const char *vals[] = {cwd, HOME, si.user};
                    for (int i = 0; i < 3; i++) {
                        int z = strlen(names[i]);
                        if (!memcmp(s, names[i], z) && !((s[z] >= 'A' && s[z] <= 'Z') || (s[z] >= 'a' && s[z] <= 'z') || (s[z] >= '0' &&
                                                         s[z] <= '9') || s[z] == '_')) {
                            v = vals[i];
                            used = z;
                            break;
                        }
                    }
                }
                if (v) {
                    int z = strlen(v);
                    if (n + z >= PATHLEN)return 0;
                    memcpy(w + n, v, z);
                    n += z;
                    s += used;
                    continue;
                }
            }
            if (c == '\\' && q != '\'' && *s)c = *s++;
            if (n >= PATHLEN - 1)return 0;
            w[n++] = c;
        }
        if (q)return 0;
        w[n] = 0;
        ExecArgs *a = cmd + count - 1;
        if (target) {
            if (expandpath(target == 1 ? readfrom : redir, cwd, w))return 0;
            if (target > 1)mode = target;
            target = 0;
        } else {
            if (a->argc == ARGMAX)return 0;
            strset(a->argv[a->argc++], w, PATHLEN);
        }
    }
    return !target && cmd[count - 1].argc;
}
static int builtin(void)
{
    ExecArgs *a = cmd;
    char *k = a->argv[0];
    int n = a->argc;
    if (!eq(k, "cd") && !eq(k, "clear") && !eq(k, "history") && !eq(k, "help") && !eq(k, "exit") && !eq(k, "off") &&
        !eq(k, "restart") && !eq(k, "kill"))return 0;
    last = 0;
    if (count != 1 || *redir || *readfrom) {
        note("I comandi interni richiedono una riga separata.");
        last = 2;
        return 1;
    }
    if (eq(k, "cd")) {
        char p[PATHLEN];
        Dirent d;
        if (n > 2 || expandpath(p, cwd, n == 1 ? HOME : eq(a->argv[1], "-") ? oldcwd : a->argv[1]) ||
            sc(S_STAT, p, &d, 0, 0) || !d.dir) {
            note("cd: cartella non trovata");
            last = 1;
        } else {
            strset(oldcwd, cwd, PATHLEN);
            strset(cwd, p, PATHLEN);
        }
    } else if (eq(k, "clear"))nt = scroll = 0;
    else if (eq(k, "exit"))quit();
    else if (eq(k, "off") || eq(k, "restart"))sc(S_POWER, eq(k, "restart"), 0, 0, 0);
    else if (eq(k, "kill")) {
        if (n != 2 || sc(S_KILL, val(a->argv[1]), 0, 0, 0)) {
            note("kill: processo non disponibile");
            last = 1;
        }
    } else if (eq(k, "history")) {
        for (int i = MAX(0, nh - NH); i < nh; i++)output(history[i % NH], strlen(history[i % NH]), 0);
    } else {
        const char *h =
            "Comandi: ls, cd, pwd, cat, echo, mkdir, rmdir, touch, rm, cp, mv, date, ps,\nwhoami, which, wc, head, tail, grep, sort, shasum, basename, dirname, open,\nuname, sw_vers, diskutil, fsck_apfs, mount_apfs,\nnewfs_apfs, apfs_unlockfv.   Interni: clear, history, exit, kill, off, restart.\nEsempi: ls /Applications | sort   ·   echo Tramontana > Desktop/nota.txt\nUsa Tab per completare, ↑ e ↓ per la cronologia, Shift+↑ per scorrere.";
        output(h, strlen(h), 0);
    }
    return 1;
}
static void launch(void)
{
    ExecArgs *a = cmd + stage;
    char p[PATHLEN];
    strset(a->cwd, cwd, PATHLEN);
    if (cmdpath(p, cwd, a->argv[0])) {
        char msg[190] = "Comando non trovato: ";
        append(msg, a->argv[0], 190);
        note(msg);
        job = 0;
        last = 127;
        return;
    }
    job = sc(S_EXEC, p, a, 0, 0);
    if (job < 0) {
        job = 0;
        last = 1;
        note("Impossibile avviare il comando.");
    }
}
static void enter(void)
{
    scroll = 0;
    char q[INPUT + 96];
    prompt(q, sizeof q);
    append(q, input, sizeof q);
    output(q, strlen(q), 1);
    if (!*input)return;
    store();
    int ok = parse(input);
    input[0] = 0;
    cur = 0;
    if (!ok) {
        note("Controlla virgolette, argomenti e reindirizzamenti.");
        last = 2;
        return;
    }
    if (builtin())return;
    if (*readfrom) {
        Dirent d;
        if (sc(S_STAT, readfrom, &d, 0, 0) || d.dir || d.size > PIPEMAX ||
            (cmd[0].inlen = readfile(readfrom, cmd[0].in, PIPEMAX)) < 0) {
            note("Impossibile leggere il file di input.");
            last = 1;
            return;
        }
    }
    launch();
}
static void collect(void)
{
    job = 0;
    last = result.status;
    if (last) {
        if (result.bytes)output(result.out, result.bytes, 2);
        return;
    }
    if (++stage < count) {
        cmd[stage].inlen = result.bytes;
        memcpy(cmd[stage].in, result.out, result.bytes);
        launch();
        return;
    }
    if (*redir) {
        int n = 0;
        char *p = cmd[0].in;
        if (mode == 3) {
            Dirent d;
            if (!sc(S_STAT, redir, &d, 0, 0)) {
                if (d.dir || d.ro || d.size > MAXFILE || (n = readfile(redir, p, MAXFILE)) < 0)goto bad;
            }
        }
        if (result.bytes > PIPEMAX - n)goto bad;
        memcpy(p + n, result.out, result.bytes);
        if (writefile(redir, p, n + result.bytes) != n + result.bytes)goto bad;
    } else output(result.out, result.bytes, 0);
    return;
bad:
    last = 1;
    note("Impossibile scrivere: controlla i permessi e lo spazio del file.");
}
static void complete(void)
{
    if (cur != (int)strlen(input))return;
    int pos = cur;
    while (pos && input[pos - 1] != ' ' && input[pos - 1] != '|')pos--;
    char *d = input + pos, *p = d;
    for (char *q = d; *q; q++)if (*q == '/')p = q + 1;
    char folder[PATHLEN];
    const char *dirs[] = {"/bin", "/usr/bin", "/usr/sbin", "/sbin"};
    int many = !pos && p == d ? 4 : 1;
    if (p == d)strset(folder, cwd, PATHLEN);
    else {
        char q[PATHLEN];
        if (p - d >= PATHLEN)return;
        memcpy(q, d, p - d);
        q[p - d] = 0;
        if (expandpath(folder, cwd, q))return;
    }
    char found[50] = {0};
    int n = 0, len = strlen(p), isdir = 0;
    for (int j = 0; j < many; j++) {
        int z = sc(S_LIST, many > 1 ? dirs[j] : folder, matches, NSLOTS, 0);
        for (int i = 0; i < z; i++) {
            char *s = matches[i].name;
            if ((int)strlen(s) < len || memcmp(s, p, len))continue;
            if (n++ == 0) {
                strset(found, s, 50);
                isdir = matches[i].dir;
            } else {
                int k = 0;
                while (s[k] && s[k] == found[k])k++;
                found[k] = 0;
            }
        }
    }
    if (n) {
        int cap = INPUT - (p - input);
        strset(p, found, cap);
        if (n == 1)append(p, isdir ? "/" : " ", cap);
        cur = strlen(input);
    }
}
static int selection_bounds(int row,int*lo,int*hi){
 if(!selection)return 0;int a=anchor_row,b=end_row,x=anchor_byte,y=end_byte;if(a>b||(a==b&&x>y)){int t=a;a=b;b=t;t=x;x=y;y=t;}
 if(row<a||row>b||row<MAX(0,nt-NR)||row>=nt)return 0;*lo=row==a?x:0;*hi=row==b?y:strlen(screen[row%NR]);return *hi>*lo;
}
static void select_event(Event*e){
 if(e->d){selection=selecting=0;}
 if(!e->d&&!selecting)return;
 int rows=MAX(3,(ah-43)/19),c=cols();char q[INPUT+96];prompt(q,sizeof q);append(q,input,sizeof q);
 int nr=job?0:text_columns(q,strlen(q))/c+1,end=MAX(0,nt-scroll),start=MAX(MAX(0,nt-NR),end-rows+(scroll?0:nr));
 if(start>=end||e->b<12||e->b>=12+(end-start)*19){if(!e->c)selecting=0;return;}
 int row=start+(e->b-12)/19,col=MAX(0,(e->a-14+4)/8),at=text_offset(screen[row%NR],0,strlen(screen[row%NR]),col);
 if(e->d){anchor_row=row;anchor_byte=at;selecting=1;}
 end_row=row;end_byte=at;selection=anchor_row!=end_row||anchor_byte!=end_byte;if(!e->c)selecting=0;
}
static void insert_input(const char*s){
 if(job)return;int n=strlen(s),old=strlen(input);if(n>=INPUT-old){note("Testo troppo lungo: nessun carattere incollato.");return;}
 memmove(input+cur+n,input+cur,old-cur+1);memcpy(input+cur,s,n);cur+=n;selection=0;scroll=0;
}
static void clipboard_action(int action){
 if(action==ACT_COPY||action==ACT_CUT){
  if(action==ACT_CUT&&selection){note("L’output è in sola lettura: usa Copia.");return;}
  memset(&clipboard,0,sizeof clipboard);clipboard.kind=CLIP_TEXT;int at=0;
  if(selection){int first=MIN(anchor_row,end_row),last=MAX(anchor_row,end_row);for(int row=first;row<=last;row++){int lo=0,hi=0;selection_bounds(row,&lo,&hi);int z=MAX(0,hi-lo);if(at+z+(row<last)>=CLIP_BYTES){note("Selezione troppo lunga per gli appunti (256 KiB).");return;}memcpy(clipboard.data+at,screen[row%NR]+lo,z);at+=z;if(row<last)clipboard.data[at++]='\n';}}
  else strset(clipboard.data,input,sizeof clipboard.data);
  if(!sc(S_CLIPBOARD,1,&clipboard,0,0)&&action==ACT_CUT&&!job)input[cur=0]=0;
 }else if(action==ACT_PASTE){
  if(sc(S_CLIPBOARD,0,&clipboard,0,0)<0||clipboard.kind!=CLIP_TEXT)return;
  /* Pasting NEVER synthesizes Enter or executes pasted commands. */
  for(int j=0;clipboard.data[j];j++)if((u8)clipboard.data[j]<32||(u8)clipboard.data[j]==127)clipboard.data[j]=' ';
  if(text_field_length(clipboard.data,sizeof clipboard.data)<0){note("Testo degli appunti non valido.");return;}insert_input(clipboard.data);
 }
}
static void drop_path(const char*path){
 char quoted[PATHLEN*2+5];int n=0;quoted[n++]='"';for(int j=0;path[j]&&j<PATHLEN-1;j++){if(path[j]=='"'||path[j]=='\\'||path[j]=='$'||path[j]=='`')quoted[n++]='\\';quoted[n++]=path[j];}quoted[n++]='"';quoted[n++]=' ';quoted[n]=0;insert_input(quoted);
}
static void paint(void)
{
    begin();
    int dark = si.dark, c = cols();
    u32 paper = dark ? 0x1b1c1f : 0xfcfcfd, fg = dark ? 0xe6e6eb : 0x202126, dim = dark ? 0xa9adb8 : 0x61656b,
        pr = dark ? 0xb8cfeb : 0x29415c;
    Box(0, 0, aw, ah, 0, paper);
    char ttl[48];
    strset(ttl, eq(cwd, HOME) ? "~" : eq(cwd, "/") ? "/" : leaf(cwd), 48);
    append(ttl, " — shell", 48);
    title("Terminale — ", ttl);
    int rows = MAX(3, (ah - 43) / 19);
    char q[INPUT + 96];
    prompt(q, sizeof q);
    int pre = strlen(q);
    append(q, input, sizeof q);
    int nr = job ? 0 : (text_columns(q,strlen(q)) / c + 1);
    int end = MAX(0, nt - scroll), start = MAX(MAX(0, nt - NR), end - rows + (scroll ? 0 : nr)), y = 12;
    for (int i = start; i < end; i++, y += 19) {
        int k = i % NR;
        int lo,hi;if(selection_bounds(i,&lo,&hi))Box(14+text_columns(screen[k],lo)*8,y,MAX(1,text_columns(screen[k]+lo,hi-lo))*8,18,0,dark?0x315780:0xb9d7ff);
        Text(14, y, screen[k], F_MONO, kinds[k] == 1 ? pr : kinds[k] == 2 ? (dark ? 0xf1a89c : 0x953b30) : fg);
    }
    if (!scroll && !job) {
        for (int i = 0; i < nr; i++, y += 19) {
            char row[400];
            int begin=text_offset(q,0,strlen(q),i*c),end=text_offset(q,begin,strlen(q),c),z=end-begin;
            memcpy(row,q+begin,z);row[z]=0;
            int caret=text_columns(q,pre+cur);
            if(caret/c==i&&mark){int x=14+(caret%c)*8;Box(x,y+1,2,15,0,fg);}
            Text(14, y, row, F_MONO, fg);
        }
    }
    Box(0, ah - 26, aw, 1, 0, EDGE);
    Box(0, ah - 25, aw, 25, 0, SOFT);
    Icon(12, ah - 21, 16, 6);
    Text(36, ah - 22, cwd, F_SMALL, dim);
    Text(aw - 109, ah - 22, job ? "In esecuzione" : scroll ? "Cronologia" : "Sessione locale", F_SMALL, dim);
    finish();
}
static void key(Event *e)
{
    int k = e->a, n = strlen(input);
    if(e->b&&e->c&&(k=='c'||k=='C'||k=='v'||k=='V'||k=='x'||k=='X')){clipboard_action(k=='v'||k=='V'?ACT_PASTE:k=='x'||k=='X'?ACT_CUT:ACT_COPY);return;}
    if (e->b && (k == 'c' || k == 'C')) {
        if (job)sc(S_KILL, job, 0, 0, 0);
        else {
            note("^C");
            input[0] = 0;
            cur = 0;
        }
        return;
    }
    if (e->c && (k == K_UP || k == K_DOWN)) {
        scroll = MAX(0, MIN(MIN(nt, NR), scroll + (k == K_UP ? 3 : -3)));
        return;
    }
    if (job)return;
    scroll = 0;
    if (e->b) {
        if (k == 'l')nt = 0;
        if (k == 'u')input[cur = 0] = 0;
        if (k == 'a')cur = 0;
        if (k == 'e')cur = n;
        if (k == 'd' && !n)quit();
        return;
    }
    if (k == '\n') {
        enter();
        return;
    }
    if (k == '\t') {
        complete();
        return;
    }
    if (k == K_UP || k == K_DOWN) {
        if (hpos == nh)strset(saved, input, INPUT);
        hpos = MAX(MAX(0, nh - NH), MIN(nh, hpos + (k == K_UP ? -1 : 1)));
        strset(input, hpos == nh ? saved : history[hpos % NH], INPUT);
        cur = strlen(input);
        return;
    }
    if (k == K_LEFT) {
        cur = text_prev(input,cur);
        return;
    }
    if (k == K_RIGHT) {
        cur = text_next(input,cur);
        return;
    }
    if (k == K_HOME) {
        cur = 0;
        return;
    }
    if (k == K_END) {
        cur = n;
        return;
    }
    if(k==8&&cur)text_backspace(input,&cur);
    else if(k==K_DEL&&cur<n)text_delete(input,cur);
    else if(text_printable(k)){if(!text_insert(input,INPUT,&cur,k))return;}
    else return;

}
void app_main(void)
{
    win(730, 398, "Terminale");
    char s[80] = "Sessione locale del ";
    addnum(s, si.day, 80);
    append(s, "/", 80);
    addnum(s, si.month, 80);
    append(s, "/", 80);
    addnum(s, si.year, 80);
    append(s, " su console", 80);
    output(s, strlen(s), 0);
    mark = 1;
    paint();
    u64 stamp = 0;
    for (;;) {
        Event e;
        int dirty = 0;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            dirty = 1;
            if (baseevent(&e))continue;
            if (e.type == EV_CLOSE)quit();
            if (e.type == EV_KEY)key(&e);
            if(e.type==EV_SELECT)select_event(&e);
            if(e.type==EV_DROP)drop_path(e.text);
            if(e.type==EV_ACTION&&(e.a==ACT_COPY||e.a==ACT_CUT||e.a==ACT_PASTE))clipboard_action(e.a);
            if (e.type == EV_SCROLL) scroll=MAX(0,MIN(MIN(nt,NR),scroll+3*e.a));
            if (e.type == EV_ACTION && e.a == ACT_NEW) {
                sc(S_SPAWN, TERMINAL, 0, 0, 0);
            }
        }
        if (job && sc(S_RESULT, &result, 0, 0, 0) > 0) {
            collect();
            dirty = 1;
        }
        u64 t = sc(S_TICKS, 0, 0, 0, 0) / 56;
        if (t != stamp) {
            stamp = t;
            mark = si.reduce_motion || !(t & 1);
            dirty = 1;
        }
        if (dirty)paint();
        slp(2);
    }
}
