#include "ui.h"
#include "filepanel.h"
static FilePanel picker;
static int hasfile;
static char incoming[TEXTEDIT_LIMIT+1];
static char doc[TEXTEDIT_LIMIT + 1], path[PATHLEN], status[96];
static int length, cursor, dirty, dialog, firstline;
static Clipboard clipboard;
static int starts[TEXTEDIT_LIMIT + 2], nlines;
static void layout(void)
{
    int width = MAX(1, (aw - 56) / 8);
    int n = 0, col = 0;
    starts[n++] = 0;
    for (int i = 0; i < length;) {
        int next=text_next(doc,i);
        if (doc[i] == '\n') {
            starts[n++] = next;
            col = 0;
        } else if (++col >= width && next < length && doc[next] != '\n') {
            starts[n++] = next;
            col = 0;
        }
        i=next;
    }
    starts[n] = length;
    nlines = n;
}
static int cursorline(void)
{
    int j = 0;
    while (j + 1 < nlines && starts[j + 1] <= cursor)
        j++;
    return j;
}
static int endline(int j)
{
    int e = starts[j + 1];
    if (e > starts[j] && doc[e - 1] == '\n')
        e--;
    return e;
}
/* Viewport scrolling does not move the insertion point. Only input/navigation
 * explicitly follows the caret, never paint() or a periodic redraw. */
static int visible_rows(void){return MAX(1,(ah-115)/20);}
static void viewport(int follow)
{
    layout();
    int rows=visible_rows(),j=cursorline();
    if(follow){if(j<firstline)firstline=j;if(j>=firstline+rows)firstline=j-rows+1;}
    firstline=MAX(0,MIN(firstline,MAX(0,nlines-rows)));
}
static void scroll_document(int delta)
{
    /* Clamp input before multiplication, including synthetic INT_MIN. */
    delta=MAX(-nlines,MIN(nlines,delta));
    firstline-=delta*3;viewport(0);
}
static int line_cursor_end(int j)
{
    int e=endline(j);
    /* A soft-wrap boundary belongs to the following row. End/click must
     * stay on the requested visual row, rather than jump to the next one. */
    if(j+1<nlines&&e==starts[j+1]&&e>starts[j])e=text_prev(doc,e);
    return e;
}
static void document_click(int x,int y)
{
    viewport(0);
    int j=MIN(nlines-1,MAX(0,firstline+(y-70)/20));
    cursor=text_offset(doc,starts[j],line_cursor_end(j),MAX(0,(x-28+4)/8));
}
static int saved(void)
{
    int r = writefile(path, doc, length);
    if (r == length) {
        dirty = 0;hasfile=1;
        strset(status, si.persistent ? "Salvato su disco" : "Salvato nella sessione live", 96);
        trace(status);return 1;
    } else
        strset(status, "Salvataggio non riuscito. Controlla cartella e permessi.", 96);
    return 0;
}
static int load(const char *p)
{
    Dirent d;
    if (sc(S_STAT, p, &d, 0, 0) < 0 || d.dir || d.size > TEXTEDIT_LIMIT)
        return -1;
    int r = readfile(p, incoming, TEXTEDIT_LIMIT);
    if (r < 0 || (u64)r != d.size)
        return -1;
    memcpy(doc,incoming,r);
    hasfile=1;
    length = r;
    doc[r] = 0;
    cursor = 0;
    dirty = 0;
    firstline = 0;
    strset(path, p, sizeof path);
    strset(status, "Documento aperto", 96);
    trace(p);
    return 0;
}
static void blank(void)
{
    hasfile=0;
    length = cursor = dirty = firstline = 0;
    doc[0] = 0;
    strset(path, DOCS "/Untitled.txt", PATHLEN);
    Dirent di;
    for (int j = 2; j < NSLOTS + 3 && sc(S_STAT, path, &di, 0, 0) == 0; j++) {
        strset(path, DOCS "/Untitled-", PATHLEN);
        addnum(path, j, PATHLEN);
        append(path, ".txt", PATHLEN);
    }
    strset(status, "Nuovo documento", 96);
}
static void prompt(int m)
{
    if(m==1||m==2){dialog=0;filepanel_open(&picker,m==1,path);return;}
    dialog = m;
    ui_motion(FX_SHEET);

}
static void paste(const char *s, int n)
{
    if (n > TEXTEDIT_LIMIT - length) {
        strset(status, "Limite documento: 262.1 kB", 96);
        return;
    }
    memmove(doc + cursor + n, doc + cursor, length - cursor + 1);
    memcpy(doc + cursor, s, n);
    length += n;
    cursor += n;
    dirty = 1;
    status[0] = 0;
}
static void act(int a)
{
    if (a == ACT_SAVE){if(hasfile)saved();else prompt(1);}
    else if (a == ACT_OPEN)
        prompt(dirty ? 6 : 2);
    else if (a == ACT_NEW) {
        if (dirty) {
            dialog = 5;
            ui_motion(FX_SHEET);
        } else
            blank();
    } else if (a == ACT_COPY) {
        if(length>=CLIP_BYTES){strset(status,"Testo troppo lungo per gli appunti condivisi (262.1 kB).",96);return;}
        memset(&clipboard,0,sizeof clipboard);clipboard.kind=CLIP_TEXT;memcpy(clipboard.data,doc,length+1);
        strset(status,sc(S_CLIPBOARD,1,&clipboard,0,0)<0?"Appunti non disponibili.":"Testo copiato negli appunti condivisi.",96);
    } else if (a == ACT_PASTE){
        if(!sc(S_CLIPBOARD,0,&clipboard,0,0)&&clipboard.kind==CLIP_TEXT)paste(clipboard.data,strlen(clipboard.data));
    }
}
/* Only discard confirmations use this state. Open/Save never fall back to
 * the obsolete pathname-entry modal, including after an unsaved document. */
static void confirm_accept(void)
{
    int m=dialog;dialog=0;ui_motion(FX_DISSOLVE);
    if(m==3){quit();return;}
    if(m==5){blank();return;}
    if(m==6)prompt(2);
}
static void dk(int k)
{
    if(k==27){dialog=0;ui_motion(FX_DISSOLVE);return;}
    if(k=='\n')confirm_accept();
}
static void confirm_click(const Event *e)
{
    int w=MIN(460,aw-40),x=(aw-w)/2,y=(ah-190)/2;
    if(e->b<y+144||e->b>=y+178)return;
    if(e->a>=x+w-238&&e->a<x+w-134){dialog=0;ui_motion(FX_DISSOLVE);}
    else if(e->a>=x+w-124&&e->a<x+w-20)confirm_accept();
}
static void key(Event *e)
{
    int k = e->a;
    if (dialog) {
        dk(k);
        return;
    }
    if (e->b) {
        if ((k == 's' || k == 'S') && e->c)
            prompt(1);
        else if (k == 's')
            act(ACT_SAVE);
        else if (k == 'o')
            act(ACT_OPEN);
        else if (k == 'n')
            act(ACT_NEW);
        else if (k == 'c')
            act(ACT_COPY);
        else if (k == 'v')
            act(ACT_PASTE);
        return;
    }
    if(k==8&&cursor){text_backspace(doc,&cursor);length=strlen(doc);dirty=1;
    } else if(k==K_DEL&&cursor<length){text_delete(doc,cursor);length=strlen(doc);dirty=1;
    } else if (k == K_LEFT)
        cursor = text_prev(doc,cursor);
    else if (k == K_RIGHT)
        cursor = text_next(doc,cursor);
    else if (k == K_HOME || k == K_END || k == K_UP || k == K_DOWN) {
        layout();
        int j = cursorline(), col = text_columns(doc+starts[j],cursor-starts[j]);
        if (k == K_HOME)
            cursor = starts[j];
        else if (k == K_END)
            cursor = line_cursor_end(j);
        else {
            j = MAX(0, MIN(nlines - 1, j + (k == K_UP ? -1 : 1)));
            cursor = text_offset(doc,starts[j],line_cursor_end(j),col);
        }
    } else if(k=='\n'||text_printable(k)){
        if(text_insert(doc,sizeof doc,&cursor,k)){length=strlen(doc);dirty=1;}
    } else if (k == '\t')
        paste("    ", 4);
}
static void paint(void)
{
    viewport(0);
    begin();
    title(dirty ? "* " : "", leaf(path));
    /* The chooser is the only surface while modal; keep the document in memory. */
    if(picker.active){filepanel_draw(&picker);finish();return;}
    int nrows = visible_rows(), j = cursorline();
    Box(0, 0, aw, 47, 0, PANEL);
    button(13, 10, 55, "Nuovo", 0);
    button(75, 10, 57, "Apri", 0);
    button(139, 10, 60, "Salva", 1);
    button(206, 10, 91, "Salva come", 0);
    Text(aw - 117, 16, "Testo semplice", F_SMALL, SUB);
    Box(0, 47, aw, 1, 0, EDGE);
    Box(0, 48, aw, ah - 82, 0, CARD);
    Box(0, ah - 34, aw, 1, 0, EDGE);
    for (int i = firstline; i < nlines && i < firstline + nrows; i++) {
        MonoText(28,70+(i-firstline)*20,doc+starts[i],endline(i)-starts[i],TX);
    }
    if(!dialog&&j>=firstline&&j<firstline+nrows)
        Caret(28 + 8 * text_columns(doc+starts[j],cursor-starts[j]), 71 + 20 * (j - firstline), 2, 16, ACC);
    char z[96] = "Riga ";
    addnum(z, j + 1, 96);
    append(z, "   /   ", 96);
    addnum(z, length, 96);
    append(z, " byte", 96);
    Text(15, ah - 25, status[0] ? status : dirty ? "Modifiche non salvate" : "Pronto", F_SMALL, dirty ? EMP : SUB);
    Text(aw - 165, ah - 25, z, F_SMALL, SUB);
    if (dialog) {
        int w=MIN(460,aw-40),x=(aw-w)/2,y=(ah-190)/2;
        Sheet(x,y,w,190,14);
        Text(x+20,y+23,dialog==3?"Chiudere senza salvare?":dialog==5?"Creare un nuovo documento?":"Aprire un altro documento?",F_CAPTION,TX);
        Text(x+20,y+66,leaf(path),F_BODY,TX);
        Text(x+20,y+98,"Le modifiche non salvate andranno perse.",F_SMALL,SUB);
        button(x+w-238,y+144,104,"Annulla",0);
        button(x+w-124,y+144,104,"Non salvare",1);
    }
    finish();
}
static void picker_event(const Event *e)
{
    int result=filepanel_event(&picker,e);
    if(result!=1)return;
    if(picker.save){
        char old[PATHLEN];strset(old,path,PATHLEN);strset(path,picker.result,PATHLEN);
        int before=hasfile;
        if(!saved()){strset(path,old,PATHLEN);hasfile=before;picker.active=1;strset(picker.message,status,sizeof picker.message);}
    }else if(load(picker.result)<0){picker.active=1;strset(picker.message,"Impossibile aprire il documento.",sizeof picker.message);}
}
void app_main(void)
{
    win(760, 480, "TextEdit");
    blank();
    char s[PATHLEN];
    sc(S_STARTARG, s, 0, 0, 0);
    if (s[0] && load(s) < 0) {
        Dirent di;
        if (sc(S_STAT, s, &di, 0, 0) == 0)strset(status, "Apri i file di sistema con Vista rapida nel Finder.", 96);
        else {
            strset(path, s, PATHLEN);
            strset(status, "Nuovo file nel percorso scelto", 96);
        }
    }
    paint();
    for (;;) {
        Event e;
        int redraw = 0;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            redraw = 1;
            if (baseevent(&e))
                continue;
            if(picker.active){picker_event(&e);continue;}
            if (e.type == EV_CLOSE) {
                if (dirty) {
                    dialog = 3;
                    ui_motion(FX_SHEET);
                } else {
                    trace("window closed");
                    quit();
                }
            }
            if(dialog&&e.type==EV_CLICK){confirm_click(&e);continue;}
            if (e.type == EV_KEY){key(&e);viewport(1);}
            if (!dialog && e.type == EV_ACTION){act(e.a);viewport(1);}
            if (!dialog && e.type == EV_SCROLL){scroll_document(e.a);continue;}
            if (!dialog && e.type == EV_CLICK) {
                if (e.b < 46) {
                    if (e.a < 70)
                        act(ACT_NEW);
                    else if (e.a < 135)
                        act(ACT_OPEN);
                    else if (e.a < 202)
                        act(ACT_SAVE);
                    else if (e.a < 298)
                        prompt(1);
                } else if (e.b >= 67 && e.b < ah - 33) {
                    document_click(e.a,e.b);
                }
            }
        }
        SysInfo next;sc(S_INFO,&next,0,0,0);
        if(picker.active&&(si.fsrev!=next.fsrev||si.ticks/64!=next.ticks/64))redraw=1;
        si=next;
        if (redraw)
            paint();
        slp(2);
    }
}
