/* UI-only modifier symbols. Command exists in the sealed font; draw the
 * remaining three geometrically rather than changing locked font assets. */
#pragma once
static void modifier_symbol(int x,int y,int kind,u32 ink)
{
    if(kind==0){text(x,y-2,"⌘",F_BODY,ink);return;}
    if(kind==1){line(x+2,y+8,x+7,y+3,ink);line(x+7,y+3,x+12,y+8,ink);return;}
    if(kind==2){line(x+1,y+3,x+5,y+3,ink);line(x+5,y+3,x+10,y+12,ink);line(x+10,y+12,x+15,y+12,ink);line(x+9,y+3,x+15,y+3,ink);return;}
    line(x+1,y+7,x+7,y+1,ink);line(x+7,y+1,x+13,y+7,ink);
    line(x+1,y+7,x+4,y+7,ink);line(x+10,y+7,x+13,y+7,ink);
    line(x+4,y+7,x+4,y+13,ink);line(x+10,y+7,x+10,y+13,ink);line(x+4,y+13,x+10,y+13,ink);
}
static void shortcuts_help(Rect q)
{
    int x=q.x,y=q.y;
    text(x+23,y+21,"Scorciatoie",F_TITLE,TX);
    static const char *names[]={"Command · Windows","Control · Ctrl","Option · Alt sinistro","Maiusc · Shift"};
    for(int i=0;i<4;i++){
        int xx=x+23+(i%2)*260,yy=y+65+(i/2)*25;
        modifier_symbol(xx,yy,i,SUB);text(xx+24,yy,names[i],F_SMALL,SUB);
    }
    rect(x+23,y+120,q.w-46,1,LN);
    static const struct {int mods;const char *key,*action;} rows[]={
        {1,"Spazio","Spotlight"},{2,"↑","Mission Control"},
        {9,"A","Applicazioni"},{1,"Tab","Finestra precedente"},
        {1,"W","Chiudi finestra"},{1,"M","Riduci a icona"},
        {3,"Q","Blocca schermo"},{9,"N","Nuova cartella"}
    };
    for(unsigned i=0;i<sizeof rows/sizeof *rows;i++){
        int xx=x+23,yy=y+137+i*28;
        for(int m=0;m<4;m++)if(rows[i].mods&(1<<m)){modifier_symbol(xx,yy,m,TX);xx+=20;}
        text(xx+4,yy,rows[i].key,F_BODY,TX);
        text(x+190,yy,rows[i].action,F_BODY,TX);
    }
    Rect button={x+q.w-100,y+q.h-44,80,29};button_face(button,1);
    center(button.x+40,button.y+6,"Fine",F_SMALL,night?0x10274a:0xffffff);
}
