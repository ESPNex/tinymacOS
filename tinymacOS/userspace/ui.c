#include "ui.h"
#include "mono_wire.h"
#include "caret.h"
int aw, ah;
SysInfo si;
char wintitle[48];
static Draw list[NCMDS];
static int count, fx;
void ui_motion(int kind)
{
    fx = kind;
}
void win(int w, int h, const char *s)
{
    aw = w - 2;
    ah = h;
    strset(wintitle, s, 48);
    if (sc(S_WOPEN, w, h, s, 0) < 0)
        quit();
    sc(S_INFO, &si, 0, 0, 0);
}
void begin(void)
{
    count = 0;
}
void finish(void)
{
    sc(S_WDRAW, list, count, wintitle, fx);
    fx = 0;
}
static Draw *op(int kind, int x, int y, int w, int h, int style, u32 col, int r)
{
    static Draw sink;
    Draw *q = count < NCMDS ? list + count++ : &sink;
    *q = (Draw) {
        x, y, w, h, kind, style, r, col, {0}
    };
    return q;
}
void Box(int x, int y, int w, int h, int r, u32 c)
{
    op(C_RECT, x, y, w, h, 0, c, r);
}
void Text(int x, int y, const char *s, int f, u32 c)
{
    Draw *q = op(C_TEXT, x, y, 0, 0, f, c, 0);
    strset(q->text, s, 100);
}
/* Fixed-cell text uses the existing font repertoire's compact wire format.
 * 99 glyphs fit in one command regardless of their UTF-8 byte lengths. */
void MonoText(int x,int y,const char *s,int bytes,u32 color)
{
    while(bytes>0&&*s){
        Draw *q=op(C_MONO,x,y,0,0,0,color,0);int n=0;
        while(bytes>0&&*s&&n<99){
            int used;unsigned code=mono_decode(s,bytes,&used);
            q->text[n++]=(char)mono_reference(code);s+=used;bytes-=used;
        }
        q->w=n;x+=n*8;
    }
}
void Caret(int x,int y,int w,int h,u32 ink)
{
    if(caret_dimensions(w,h))op(C_CARET,x,y,w,h,0,ink,0);
}
/* Shared single-line edit rendering. secret never serializes source bytes;
 * both masked and normal fields reserve a cell for the insertion marker. */
void InputText(int x,int y,int width,const char *s,int at,int active,int secret,u32 ink)
{
    int len=strlen(s);at=MAX(0,MIN(at,len));
    while(at>0&&((u8)s[at]&0xc0)==0x80)at--;
    int columns=MAX(1,width/8-1),col=text_columns(s,at);
    int start=text_offset(s,0,at,MAX(0,col-columns));
    int end=text_offset(s,start,len,columns);
    if(secret){
        int visible=text_columns(s+start,end-start),done=0;
        while(done<visible){Draw *q=op(C_MONO,x+done*8,y,MIN(99,visible-done),0,0,ink,0);
            for(int j=0;j<q->w;j++)q->text[j]=(char)mono_reference(0x2022);done+=q->w;}
    }else MonoText(x,y,s+start,end-start,ink);
    if(active)Caret(x+8*text_columns(s+start,at-start),y,1,17,ink);
}
void Center(int x, int y, const char *s, int f, u32 c)
{
    Text(x, y, s, f | 128, c);
}
void Icon(int x, int y, int size, int id)
{
    op(C_ICON, x, y, size, size, id, 0, 0);
}
void Line(int x, int y, int X, int Y, u32 c)
{
    op(C_LINE, x, y, X, Y, 0, c, 0);
}
void button(int x, int y, int w, const char *s, int strong)
{
    op(C_BUTTON, x, y, w, 26, strong != 0, 0, 0);
    Center(x + w / 2, y + 4, s, F_SMALL, strong ? (si.dark ? 0x12284a : 0xffffff) : TX);
}
void modal(const char *name, const char *value, const char *hint)
{
    int w = MIN(aw - 46, 430), x = (aw - w) / 2, y = (ah - 143) / 2;

    Sheet(x, y, w, 143, 15);
    Text(x + 16, y + 12, name, F_BOLD, TX);
    Box(x + 16, y + 43, w - 32, 35, 5, PANEL);
    int l = strlen(value);
    Text(x + 25, y + 52, value + MAX(0, l - (w - 59) / 9), F_MONO, TX);
    Text(x + 16, y + 91, hint, F_SMALL, SUB);
    Text(x + 16, y + 115, "Invio per confermare  ·  Esc per annullare", F_MICRO, EMP);
}
void inputmodal(const char *name,const char *value,const char *hint,int secret)
{
    modal(name,"",hint);
    int w=MIN(aw-46,430),x=(aw-w)/2,y=(ah-143)/2;
    InputText(x+25,y+52,w-50,value,strlen(value),1,secret,TX);
}
int baseevent(Event *e)
{
    if (e->type == EV_THEME) {
        sc(S_INFO, &si, 0, 0, 0);
        return 1;
    }
    if (e->type == EV_RESIZE) {
        aw = e->a;
        ah = e->b;
        return 1;
    }
    return 0;
}
void quit(void)
{
    sc(S_EXIT, 0, 0, 0, 0);
    for (;;)
        ;
}
void addnum(char *s, u64 a, int cap)
{
    char n[24];
    number(n, a);
    append(s, n, cap);
}
void parentpath(char *p)
{
    int len = strlen(p);
    while (len > 1 && p[len - 1] != '/')
        len--;
    if (len > 1)
        len--;
    p[len] = 0;
}
const char *leaf(const char *p)
{
    const char *s = p;
    while (*p) {
        if (*p == '/')
            s = p + 1;
        p++;
    }
    return s;
}
int absolutepath(char *out, const char *cwd, const char *name)
{
    if (!name || !*name)
        return -1;
    char s[PATHLEN * 2];
    if (*name == '/')
        strset(s, name, sizeof s);
    else {
        strset(s, cwd, sizeof s);
        append(s, "/", sizeof s);
        append(s, name, sizeof s);
    }
    int at = 0;
    out[0] = '/';
    out[1] = 0;
    while (s[at]) {
        while (s[at] == '/')
            at++;
        int j = at;
        while (s[j] && s[j] != '/')
            j++;
        if (j == at)
            break;
        int n = j - at;
        if (n > 47)
            return -1;
        char part[48];
        memcpy(part, s + at, n);
        part[n] = 0;
        at = j;
        if (eq(part, "."))
            continue;
        if (eq(part, "..")) {
            parentpath(out);
            continue;
        }
        if (strlen(out) + n + 2 >= PATHLEN)
            return -1;
        if (!eq(out, "/"))
            append(out, "/", PATHLEN);
        append(out, part, PATHLEN);
    }
    return 0;
}
int readfile(const char *p,char *b,int cap)
{
    if(cap<0||(!b&&cap))return -1;int d=sc(S_OPEN,p,0,0,0);if(d<0)return -1;int n=0;
    while(n<cap){int take=MIN(MAXFILE,cap-n),r=sc(S_READ,d,b+n,take,0);if(r<0||r>take){n=-1;break;}if(!r)break;n+=r;}
    sc(S_CLOSE,d,0,0,0);return n;
}
int writefile(const char *p,const char *b,int n)
{
    if(n<0||(!b&&n))return -1;int d=sc(S_OPEN,p,1,0,0);
    if(d<0){if(sc(S_CREATE,p,0,0,0)<0)return -1;d=sc(S_OPEN,p,1,0,0);}if(d<0)return -1;
    int at=0;do{int take=MIN(MAXFILE,n-at),r=sc(S_WRITE,d,b?b+at:0,take,0);if(r!=take){at=-1;break;}at+=r;}while(at<n);
    sc(S_CLOSE,d,0,0,0);return at;
}
int copyfile(const char *p,const char *q)
{
    char buf[MAXFILE];Dirent from;
    if(sc(S_STAT,p,&from,0,0)<0||from.dir)return -1;int src=sc(S_OPEN,p,0,0,0);if(src<0)return -1;
    /* Exclusive create: an existing destination is NEVER truncated, including
     * a destination created by another app after our initial source lookup. */
    if(sc(S_CREATE,q,0,0,0)<0){sc(S_CLOSE,src,0,0,0);return -1;}
    int dst=sc(S_OPEN,q,1,0,0),failed=dst<0;u64 at=0;
    if(!failed)do{int take=MIN((u64)MAXFILE,from.size-at),r=sc(S_READ,src,buf,take,0);if(r!=take||sc(S_WRITE,dst,buf,take,0)!=take){failed=1;break;}at+=take;}while(at<from.size);
    sc(S_CLOSE,src,0,0,0);if(dst>=0)sc(S_CLOSE,dst,0,0,0);return failed?-1:0;
}
void title(const char *s, const char *t)
{
    strset(wintitle, s, 48);
    append(wintitle, t, 48);
}

void Photo(int x, int y, int w, int h, int id, int r)
{
    op(C_IMAGE, x, y, w, h, id, 0, r);
}
void Sheet(int x, int y, int w, int h, int r)
{
    op(C_GLASS, x, y, w, h, 0, 0, r);
}
void DialogSheet(int x, int y, int w, int h, int r)
{
    op(C_GLASS, x, y, w, h, GLASS_DIALOG, 0, r);
}
void Keyface(int x, int y, int w, int h, int on)
{
    op(C_BUTTON, x, y, w, h, on != 0, 0, 0);
}
