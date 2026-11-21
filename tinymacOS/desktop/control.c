#include "desk.h"
#include "control.h"
/* Real system preferences only: no synthetic brightness, radio or audio state. */
static int focus=-1,dragging=-1,last=-1;
static u32 ink=0xf9fcff,muted=0xcde8f4;
static int sz(Rect p,int n){return MAX(1,n*p.h/550);}
static void setpref(int key,int value){if(sc(S_PREF,key,value,0,0)==0)dirty=1;}
void control_cancel(void){focus=dragging=last=-1;}
static int value(int id){return id==CC_THEME_SLIDER?sys.theme:sys.wall;}
static int count(int id){return id==CC_THEME_SLIDER?NTHEME:NWALL;}
static void select_at(Rect p,int id,int x){
    int v=control_slider_value(control_rect(p,id),x,count(id));
    if(v!=last){if(v!=value(id))setpref(id==CC_THEME_SLIDER?0:1,v);last=v;}
}
static void activate(int id){
    switch(id){
    case CC_APPEAR:setpref(5,!sys.dark);break;
    case CC_MATERIAL:togglepref(4);break;
    case CC_MOTION:togglepref(3);break;
    case CC_DOCK:togglepref(2);break;
    case CC_WALL:togglepref(1);break;
    case CC_MISSION:overlay_show(0);mc_enter();break;
    case CC_LOCK:overlay_show(0);action(152);break;
    case CC_CALC:overlay_show(0);launch(CALC);break;
    case CC_CALENDAR:overlay_show(0);launch(CALENDAR);break;
    case CC_TERMINAL:overlay_show(0);launch(TERMINAL);break;
    case CC_ALL_SETTINGS:overlay_show(0);launch(SETTINGS);break;
    }
    dirty=1;
}
void control_click(Rect p,int x,int y){
    for(int id=0;id<CC_COUNT;id++)if(control_hit(p,x,y,id)){
        focus=id;
        if(id==CC_THEME_SLIDER){dragging=id;last=-1;select_at(p,id,x);}
        else activate(id);
        return;
    }
}
void control_pointer(Rect p,int down){
    if(!down){dragging=last=-1;return;}
    if(dragging>=0)select_at(p,dragging,mx);
}
int control_key(Rect p,Event *e){
    int k=e->a;
    if(k=='\t'||k==K_UP||k==K_DOWN){
        int back=(k==K_UP)||(k=='\t'&&e->c);
        focus=(focus+(back?CC_COUNT-1:1)+CC_COUNT)%CC_COUNT;dirty=1;return 1;
    }
    if(focus<0)focus=0;
    if((focus==CC_THEME_SLIDER||focus==CC_WALL)&&
        (k==K_LEFT||k==K_RIGHT||k==K_HOME||k==K_END)){
        int n=count(focus),v=k==K_HOME?0:k==K_END?n-1:clampi(value(focus)+(k==K_LEFT?-1:1),0,n-1);
        setpref(focus==CC_THEME_SLIDER?0:1,v);return 1;
    }
    if(k==' '||k=='\n'){activate(focus);return 1;}
    (void)p;return 1;
}
static void tile(Rect p,int id,int active){
    Rect b=control_rect(p,id);int r=control_radius(p,id);
    shadow(b,r,34);glass(b,r,MAT_DOCK);
    if(active)roundr(b.x,b.y,b.w,b.h,r,INK(I_ACC),lite?215:62);
    if(control_hit(p,mx,my,id))roundr(b.x,b.y,b.w,b.h,r,0xffffff,24);
    stroke(b,r,0xd3f4ff,lite?82:128);
    if(id==focus)stroke((Rect){b.x-2,b.y-2,b.w+4,b.h+4},r+2,0xffffff,235);
}
/* Crisp monochrome symbols drawn here, never borrowed application artwork. */
enum {SYM_APPEAR,SYM_GLASS,SYM_MOTION,SYM_DOCK,SYM_MISSION,SYM_LOCK,SYM_CALC,SYM_CALENDAR,SYM_TERMINAL,SYM_SETTINGS,SYM_THEME,SYM_WALL};
static Rect symbolbox;static u32 symbolink;
static int sx(int x){return symbolbox.x+x*symbolbox.w/32;}
static int sy(int y){return symbolbox.y+y*symbolbox.h/32;}
static void seg(int x,int y,int X,int Y){
    line(sx(x),sy(y),sx(X),sy(Y),symbolink);
    if(symbolbox.w>=25)line(sx(x)+1,sy(y),sx(X)+1,sy(Y),symbolink);
}
static void frame(int x,int y,int w,int h,int r){stroke((Rect){sx(x),sy(y),MAX(1,w*symbolbox.w/32),MAX(1,h*symbolbox.h/32)},MAX(1,r*symbolbox.w/32),symbolink,256);}
static void dot(int x,int y,int d){int z=MAX(2,d*symbolbox.w/32);roundr(sx(x),sy(y),z,z,z/2,symbolink,256);}
static void symbol(Rect b,int id,u32 c){
    symbolbox=b;symbolink=c;
    switch(id){
    case SYM_APPEAR:frame(4,4,24,24,12);seg(16,4,16,28);seg(6,11,14,11);seg(5,16,14,16);seg(6,21,14,21);break;
    case SYM_GLASS:seg(3,11,16,4);seg(16,4,29,11);seg(29,11,16,18);seg(16,18,3,11);seg(3,17,16,24);seg(16,24,29,17);seg(3,23,16,30);seg(16,30,29,23);break;
    case SYM_MOTION:frame(5,5,23,23,11);seg(16,9,16,17);seg(16,17,22,20);seg(2,9,2,17);seg(2,17,9,17);break;
    case SYM_DOCK:frame(2,5,28,22,4);frame(7,19,18,5,2);seg(8,10,24,10);break;
    case SYM_MISSION:frame(3,5,26,22,4);seg(11,6,11,26);seg(3,13,10,13);seg(3,20,10,20);break;
    case SYM_LOCK:frame(7,13,19,16,3);frame(11,3,11,16,6);dot(15,19,3);seg(16,21,16,25);break;
    case SYM_CALC:frame(7,2,19,28,3);frame(10,5,13,6,1);for(int y=15;y<=25;y+=5)for(int x=10;x<=20;x+=5)dot(x,y,2);break;
    case SYM_CALENDAR:frame(3,6,26,23,3);seg(3,13,29,13);seg(10,2,10,9);seg(23,2,23,9);for(int y=17;y<=23;y+=6)for(int x=8;x<=22;x+=7)dot(x,y,2);break;
    case SYM_TERMINAL:frame(2,4,28,24,4);seg(7,10,13,16);seg(13,16,7,22);seg(17,22,24,22);break;
    case SYM_SETTINGS:frame(7,7,18,18,9);frame(12,12,8,8,4);seg(16,1,16,7);seg(16,25,16,31);seg(1,16,7,16);seg(25,16,31,16);seg(5,5,9,9);seg(23,23,27,27);seg(5,27,9,23);seg(23,9,27,5);break;
    case SYM_THEME:frame(8,8,16,16,8);seg(16,1,16,5);seg(16,27,16,31);seg(1,16,5,16);seg(27,16,31,16);seg(5,5,8,8);seg(24,24,27,27);seg(5,27,8,24);seg(24,8,27,5);break;
    case SYM_WALL:frame(2,4,28,24,4);seg(4,25,12,15);seg(12,15,18,22);seg(18,22,23,16);seg(23,16,28,24);dot(21,9,4);break;
    }
}
static void glyph(Rect p,Rect b,int ico,int active){
    int s=sz(p,30);if(active){int d=sz(p,48);roundr(b.x+(b.w-d)/2,b.y+(b.h-d)/2,d,d,d/2,0xf7fcff,232);}
    symbol((Rect){b.x+(b.w-s)/2,b.y+(b.h-s)/2,s,s},ico,active?0x1678ef:ink);
}
static void pill(Rect p,int id,const char *title,const char *state,int ico,int on){
    Rect b=control_rect(p,id);tile(p,id,on);
    int d=sz(p,44),x=b.x+sz(p,13),y=b.y+(b.h-d)/2;
    roundr(x,y,d,d,d/2,on?0xffffff:0xc7eaff,on?233:42);
    symbol((Rect){x+sz(p,9),y+sz(p,9),sz(p,26),sz(p,26)},ico,on?0x1678ef:ink);
    text(x+d+sz(p,9),b.y+sz(p,18),title,F_BOLD,ink);
    text(x+d+sz(p,9),b.y+sz(p,43),state,F_SMALL,muted);
}
static void slider(Rect p,int id,const char *label,const char *state){
    Rect b=control_rect(p,id);tile(p,id,0);
    text(b.x+sz(p,20),b.y+sz(p,10),label,F_BOLD,ink);
    int sw=textw(state,F_SMALL);text(b.x+b.w-sz(p,20)-sw,b.y+sz(p,12),state,F_SMALL,muted);
    int pad=MAX(25,b.w*44/376),x=b.x+pad,y=b.y+sz(p,55),len=b.w-2*pad;
    int n=count(id),v=value(id),fill=v*len/(n-1),h=MAX(4,sz(p,5));
    roundr(x,y,len,h,h/2,0x143f69,99);roundr(x,y,MAX(h,fill),h,h/2,lite&&!sys.dark?0x2586ef:0xf4fbff,242);
    for(int j=0;j<n;j++)roundr(x+j*len/(n-1)-1,y+sz(p,10),2,2,1,ink,112);
    int d=sz(p,19);shadow((Rect){x+fill-d/2,y+h/2-d/2,d,d},d/2,28);
    roundr(x+fill-d/2,y+h/2-d/2,d,d,d/2,0xffffff,256);
    symbol((Rect){b.x+sz(p,12),y-sz(p,7),sz(p,19),sz(p,19)},id==CC_THEME_SLIDER?SYM_THEME:SYM_WALL,ink);
    symbol((Rect){b.x+b.w-sz(p,31),y-sz(p,7),sz(p,19),sz(p,19)},id==CC_THEME_SLIDER?SYM_THEME:SYM_WALL,ink);
}
void control_draw(Rect p){
    /* Identical backdrop convolution/refraction and reduced-transparency fallback as Dock. */
    ink=lite&&!sys.dark?0x23324b:0xf9fcff;muted=lite&&!sys.dark?0x4b6074:0xd9edf5;
    text(p.x+sz(p,7),p.y+sz(p,3),"Centro di Controllo",F_SMALL,ink);
    pill(p,CC_APPEAR,"Aspetto",sys.dark?"Scuro":"Chiaro",SYM_APPEAR,sys.dark);
    tile(p,CC_MATERIAL,!sys.opaque);glyph(p,control_rect(p,CC_MATERIAL),SYM_GLASS,!sys.opaque);
    tile(p,CC_MOTION,!sys.reduce_motion);glyph(p,control_rect(p,CC_MOTION),SYM_MOTION,!sys.reduce_motion);
    pill(p,CC_DOCK,"Dock",sys.dock?"Laterale":"In basso",SYM_DOCK,sys.dock);
    Rect b=control_rect(p,CC_WALL);tile(p,CC_WALL,0);
    Rect thumb={b.x+sz(p,15),b.y+sz(p,14),b.w-sz(p,30),sz(p,78)};
    photo(thumb,sys.wall,sz(p,15));stroke(thumb,sz(p,15),0xffffff,112);
    text(b.x+sz(p,15),b.y+sz(p,105),"Sfondo",F_BOLD,ink);
    text(b.x+sz(p,15),b.y+sz(p,130),wallname(sys.wall),F_SMALL,muted);
    text(b.x+b.w-sz(p,30),b.y+sz(p,105),">",F_BODY,ink);
    tile(p,CC_MISSION,0);glyph(p,control_rect(p,CC_MISSION),SYM_MISSION,0);
    tile(p,CC_LOCK,0);glyph(p,control_rect(p,CC_LOCK),SYM_LOCK,0);
    slider(p,CC_THEME_SLIDER,"Tema",themename(sys.theme));
    const int ids[]={CC_CALC,CC_CALENDAR,CC_TERMINAL},icons[]={SYM_CALC,SYM_CALENDAR,SYM_TERMINAL};
    for(int j=0;j<3;j++){tile(p,ids[j],0);glyph(p,control_rect(p,ids[j]),icons[j],0);}
    b=control_rect(p,CC_ALL_SETTINGS);tile(p,CC_ALL_SETTINGS,0);
    center(b.x+b.w/2,b.y+(b.h-14)/2,"Tutte le impostazioni",F_SMALL,ink);
    /* Space below circles is reserved for a real, live hover/focus description. */
    int hit=-1;for(int id=0;id<CC_COUNT;id++)if(control_hit(p,mx,my,id)){hit=id;break;}
    if(hit<0)hit=focus;
    const char *tips[]={"Aspetto chiaro / scuro","Trasparenza del vetro","Riduci movimento","Posizione del Dock","Sfondo · clic o frecce","Mission Control","Blocca sessione","Tema · trascina o usa le frecce","Calcolatrice","Calendario","Terminale","Impostazioni di Sistema"};
    const char *s=hit>=0?tips[hit]:"Vetro · movimento · scrivania";
    center(p.x+p.w/2,p.y+sz(p,490),s,F_MICRO,muted);
}
