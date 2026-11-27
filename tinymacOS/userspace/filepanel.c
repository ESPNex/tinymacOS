#include "filepanel.h"
static int in(int x,int y,int a,int b,int w,int h){return x>=a&&x<a+w&&y>=b&&y<b+h;}
static void geometry(FilePanel *p)
{
    p->x=14;p->y=10;p->w=aw-28;p->h=ah-20;p->side=145;
    p->rows=MAX(1,MIN(12,(p->h-225)/26));
}
static void message(FilePanel *p,const char *s){strset(p->message,s,sizeof p->message);}
void filepanel_refresh(FilePanel *p)
{
    int n=browser_page(p->cwd,p->entries,BROWSER_PAGE,p->pagebase,&p->more);
    while(!n&&p->pagebase){p->pagebase=MAX(0,p->pagebase-BROWSER_PAGE);n=browser_page(p->cwd,p->entries,BROWSER_PAGE,p->pagebase,&p->more);}
    if(n<0){p->count=0;message(p,"Impossibile leggere la cartella.");return;}
    p->count=n;p->revision=si.fsrev;
    p->selected=MIN(p->selected,n-1);p->offset=MAX(0,MIN(p->offset,MAX(0,n-p->rows)));
}
static int navigate(FilePanel *p,const char *path)
{
    Dirent d;char next[PATHLEN];
    if(absolutepath(next,p->cwd,path)<0||sc(S_STAT,next,&d,0,0)<0||!d.dir){message(p,"Cartella non disponibile.");return 0;}
    strset(p->cwd,next,PATHLEN);strset(p->location,next,PATHLEN);
    p->selected=-1;p->offset=0;p->pagebase=0;p->overwrite=0;message(p,"");
    if(!p->save)p->name[0]=0;
    filepanel_refresh(p);p->focus=0;p->cursor=0;return 1;
}
void filepanel_open(FilePanel *p,int save,const char *initial)
{
    memset(p,0,sizeof *p);p->active=1;p->save=!!save;p->selected=-1;
    strset(p->cwd,DOCS,PATHLEN);geometry(p);
    if(initial&&initial[0]){
        char parent[PATHLEN];strset(parent,initial,PATHLEN);parentpath(parent);
        Dirent d;if(sc(S_STAT,parent,&d,0,0)>=0&&d.dir)strset(p->cwd,parent,PATHLEN);
        if(save)strset(p->name,leaf(initial),PATHLEN);
    }
    strset(p->location,p->cwd,PATHLEN);p->focus=save?1:0;p->cursor=strlen(p->name);p->replace=save;
    filepanel_refresh(p);ui_motion(FX_SHEET);
}
static void selected_name(FilePanel *p)
{
    if(p->selected>=0&&p->selected<p->count&&!p->entries[p->selected].dir){strset(p->name,p->entries[p->selected].name,PATHLEN);p->cursor=strlen(p->name);p->replace=1;}
}
static int panel_page(FilePanel*p,int direction)
{
 if((direction>0&&!p->more)||(direction<0&&!p->pagebase))return 0;
 if(direction>0&&p->pagebase>0x7fffffff-2*BROWSER_PAGE)return 0;
 p->pagebase=MAX(0,p->pagebase+direction*BROWSER_PAGE);p->selected=-1;p->offset=0;p->focus=0;if(!p->save)p->name[0]=0;
 filepanel_refresh(p);return 1;
}
static int accept(FilePanel *p)
{
    char path[PATHLEN];Dirent d,parent;
    if(p->focus==0&&p->selected>=0&&p->selected<p->count&&p->entries[p->selected].dir){navigate(p,p->entries[p->selected].name);return 0;}
    if(!p->name[0]){message(p,p->save?"Inserisci un nome per il documento.":"Seleziona un file da aprire.");return 0;}
    if(absolutepath(path,p->cwd,p->name)<0){message(p,"Nome o percorso troppo lungo o non valido.");return 0;}
    int exists=sc(S_STAT,path,&d,0,0)>=0;
    if(exists&&d.dir){navigate(p,path);return 0;}
    if(!p->save&&!exists){message(p,"Il file selezionato non esiste.");return 0;}
    if(!p->save&&d.size>TEXTEDIT_LIMIT){message(p,"Documento troppo grande per TextEdit (256 KiB).");return 0;}
    if(p->save){
        char dir[PATHLEN];strset(dir,path,PATHLEN);parentpath(dir);
        if(sc(S_STAT,dir,&parent,0,0)<0||!parent.dir||parent.ro||(exists&&d.ro)){message(p,"La destinazione non e scrivibile.");return 0;}
        if(exists&&!p->overwrite){strset(p->result,path,PATHLEN);p->overwrite=1;return 0;}
        if(p->overwrite&&!eq(path,p->result)){p->overwrite=0;message(p,"La destinazione e cambiata. Conferma nuovamente.");return 0;}
    }
    strset(p->result,path,PATHLEN);p->active=0;p->overwrite=0;ui_motion(FX_DISSOLVE);return 1;
}
static void field_draw(FilePanel *p,int x,int y,int w,const char *s,int focus)
{
    if(p->focus==focus)Box(x-2,y-2,w+4,29,7,ACC);
    Box(x,y,w,25,5,CARD);
    int active=p->focus==focus&&!p->overwrite&&(!p->newfolder||focus==3);
    InputText(x+8,y+4,w-16,s,p->focus==focus?p->cursor:strlen(s),active,0,TX);
}
void filepanel_draw(FilePanel *p)
{
    geometry(p);if(si.fsrev!=p->revision)filepanel_refresh(p);
    int x=p->x,y=p->y,w=p->w,h=p->h,l=x+p->side+12,cw=w-p->side-24;
    DialogSheet(x,y,w,h,14);Text(x+18,y+13,p->save?"Salva documento":"Apri documento",F_BOLD,TX);
    if(p->pagebase||p->more){char page[32]="Pagina ";addnum(page,p->pagebase/BROWSER_PAGE+1,sizeof page);Text(x+w-175,y+16,page,F_SMALL,SUB);button(x+w-78,y+9,28,"<",0);button(x+w-44,y+9,28,">",0);}
    button(x+12,y+48,39,"Su",0);
    field_draw(p,x+61,y+48,w-73,p->location,2);
    Box(x+8,y+87,p->side-8,h-195,8,PANEL);
    Text(x+18,y+91,"Preferiti",F_MICRO,SUB);
    for(int i=0;i<7;i++){
        int yy=y+115+i*27;if(yy+25>y+h-112)break;
        if(eq(p->cwd,browser_paths[i]))Box(x+12,yy-2,p->side-16,25,6,SEL);
        Icon(x+19,yy+2,17,-browser_icons[i]);Text(x+43,yy+3,browser_labels[i],F_SMALL,TX);
    }
    Text(l,y+88,"Nome",F_SMALL,SUB);Text(x+w-89,y+88,"Tipo",F_SMALL,SUB);
    Line(l,y+110,x+w-12,y+110,EDGE);
    for(int row=0;row<p->rows&&row+p->offset<p->count;row++){
        int i=row+p->offset,yy=y+113+row*26;Dirent *d=p->entries+i;
        if(i==p->selected)Box(l-4,yy,cw+8,25,5,SEL);
        Icon(l+2,yy+3,18,d->dir?6:7);
        int limit=MAX(5,MIN(47,(cw-102)/7)),end=text_offset(d->name,0,strlen(d->name),limit);
        char name[48];memcpy(name,d->name,end);name[end]=0;
        Text(l+27,yy+5,name,F_SMALL,TX);Text(x+w-89,yy+5,d->dir?"Cartella":"File",F_MICRO,SUB);
    }
    if(!p->count)Text(l+8,y+126,"Cartella vuota",F_SMALL,SUB);
    if(p->offset>0)Text(x+w-24,y+88,"^",F_SMALL,SUB);
    if(p->offset+p->rows<p->count)Text(x+w-24,y+h-110,"v",F_SMALL,SUB);
    Text(x+16,y+h-92,p->save?"Salva con nome:":"Nome file:",F_SMALL,TX);
    field_draw(p,x+133,y+h-97,w-149,p->name,1);
    Text(x+16,y+h-62,p->message[0]?p->message:si.persistent?"Volume persistente":"Sessione live: dati temporanei",F_SMALL,p->message[0]?EMP:SUB);
    button(x+12,y+h-37,119,"Nuova cartella",0);
    button(x+w-201,y+h-37,85,"Annulla",0);button(x+w-106,y+h-37,94,p->save?"Salva":"Apri",1);
    if(p->overwrite||p->newfolder){
        int mw=MIN(410,w-28),mx=x+(w-mw)/2,my=y+(h-150)/2;
        DialogSheet(mx,my,mw,150,12);Text(mx+16,my+13,p->overwrite?"Sostituire il documento?":"Nuova cartella",F_BOLD,TX);
        if(p->overwrite){Text(mx+16,my+47,leaf(p->result),F_SMALL,TX);Text(mx+16,my+75,"Il file esistente verra sostituito.",F_SMALL,SUB);}
        else field_draw(p,mx+16,my+47,mw-32,p->folder,3);
        button(mx+mw-200,my+109,87,"Annulla",0);button(mx+mw-103,my+109,87,p->overwrite?"Sostituisci":"Crea",1);
    }
}
static int folder_create(FilePanel *p)
{
    char path[PATHLEN];
    if(!p->folder[0]||eq(p->folder,".")||eq(p->folder,"..")){message(p,"Nome cartella non valido.");return 0;}
    for(const char *s=p->folder;*s;s++)if(*s=='/'){message(p,"Usa un singolo nome per la cartella.");return 0;}
    if(absolutepath(path,p->cwd,p->folder)<0||sc(S_CREATE,path,1,0,0)<0){p->newfolder=0;p->focus=1;message(p,"Impossibile creare la cartella.");return 0;}
    p->newfolder=0;navigate(p,path);return 0;
}
int filepanel_event(FilePanel *p,const Event *e)
{
    geometry(p);int x=p->x,y=p->y,w=p->w,h=p->h;
    if(e->type==EV_CLOSE){p->active=0;return -1;}
    if(p->overwrite||p->newfolder){
        int mw=MIN(410,w-28),mx=x+(w-mw)/2,my=y+(h-150)/2;
        int cancel=e->type==EV_KEY&&e->a==27;
        int confirm=e->type==EV_KEY&&(e->a=='\n'||e->a=='\r');
        if(e->type==EV_CLICK){cancel=in(e->a,e->b,mx+mw-200,my+109,87,26);confirm=in(e->a,e->b,mx+mw-103,my+109,87,26);}
        if(cancel){p->overwrite=p->newfolder=0;p->focus=1;p->cursor=strlen(p->name);return 0;}
        if(confirm)return p->overwrite?accept(p):folder_create(p);
        if(p->overwrite)return 0;
    }else if(e->type==EV_CLICK){
        int a=e->a,b=e->b;
        if((p->pagebase||p->more)&&in(a,b,x+w-78,y+9,62,26)){panel_page(p,a<x+w-48?-1:1);return 0;}
        if(in(a,b,x+12,y+48,39,26)){navigate(p,"..");return 0;}
        if(in(a,b,x+61,y+48,w-73,25)){p->focus=2;p->cursor=strlen(p->location);p->replace=1;return 0;}
        if(in(a,b,x+133,y+h-97,w-149,25)){p->focus=1;p->cursor=strlen(p->name);p->replace=1;return 0;}
        if(in(a,b,x+12,y+h-37,119,26)){p->newfolder=1;p->folder[0]=0;p->focus=3;p->cursor=0;p->replace=0;return 0;}
        if(in(a,b,x+w-201,y+h-37,85,26)){p->active=0;ui_motion(FX_DISSOLVE);return -1;}
        if(in(a,b,x+w-106,y+h-37,94,26))return accept(p);
        for(int i=0;i<7;i++){int yy=y+115+i*27;if(yy+25>y+h-112)break;if(in(a,b,x+12,yy-2,p->side-16,25)){navigate(p,browser_paths[i]);return 0;}}
        if(in(a,b,x+p->side+8,y+113,w-p->side-16,p->rows*26)){
            int i=(b-y-113)/26+p->offset;if(i<p->count){p->focus=0;p->selected=i;selected_name(p);if(e->c)return accept(p);}return 0;
        }
        return 0;
    }
    if(e->type==EV_SCROLL&&!p->newfolder){
        if(e->a<0&&p->offset>=MAX(0,p->count-p->rows)&&panel_page(p,1))return 0;
        if(e->a>0&&!p->offset&&panel_page(p,-1)){p->offset=MAX(0,p->count-p->rows);return 0;}
        p->offset=MAX(0,MIN(MAX(0,p->count-p->rows),p->offset-e->a*3));return 0;}
    if(e->type!=EV_KEY)return 0;
    int k=e->a;
    if(k==27){p->active=0;ui_motion(FX_DISSOLVE);return -1;}
    if(!p->newfolder&&e->b&&(k=='l'||k=='L')){p->focus=2;p->cursor=strlen(p->location);p->replace=1;return 0;}
    if(!p->newfolder&&k=='\t'){p->focus=(p->focus+(e->c?2:1))%3;char *s=p->focus==2?p->location:p->name;p->cursor=strlen(s);p->replace=1;return 0;}
    if(k=='\n'||k=='\r'){if(p->focus==2){navigate(p,p->location);return 0;}return accept(p);}
    if(p->focus==0){
        if(k==K_UP||k==K_DOWN){
            if(k==K_DOWN&&p->selected==p->count-1&&panel_page(p,1))p->selected=-1;
            else if(k==K_UP&&p->selected==0&&panel_page(p,-1))p->selected=p->count;
            p->selected=MAX(0,MIN(p->count-1,p->selected+(k==K_UP?-1:1)));if(p->selected<p->offset)p->offset=p->selected;if(p->selected>=p->offset+p->rows)p->offset=p->selected-p->rows+1;selected_name(p);}
        else if(k==8||k==K_LEFT)navigate(p,"..");
        return 0;
    }
    char *s=p->focus==3?p->folder:p->focus==2?p->location:p->name;int cap=p->focus==3?48:PATHLEN;
    p->cursor=MAX(0,MIN(p->cursor,strlen(s)));
    if(e->b){if(k=='a'||k=='A'){p->replace=1;p->cursor=strlen(s);}return 0;}
    if(k==K_LEFT){p->cursor=text_prev(s,p->cursor);p->replace=0;}
    else if(k==K_RIGHT){p->cursor=text_next(s,p->cursor);p->replace=0;}
    else if(k==K_HOME){p->cursor=0;p->replace=0;}
    else if(k==K_END){p->cursor=strlen(s);p->replace=0;}
    else if(k==8||k==K_DEL||text_printable(k)){
        if(p->replace){s[0]=0;p->cursor=0;p->replace=0;}
        if(k==8)text_backspace(s,&p->cursor);else if(k==K_DEL)text_delete(s,p->cursor);else text_insert(s,cap,&p->cursor,k);
        message(p,"");
    }
    return 0;
}
