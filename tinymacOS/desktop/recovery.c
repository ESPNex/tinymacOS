#include "desk.h"
#include "../recovery/background.h"
/* Native maintenance UI. Interface.tfn and its text renderer are exactly the
 * same as the desktop. The home window contains only real recovery choices. */
static DiskInfo disks;
static int selected, choice, armed=-1;
static char result[100];
static const char *titles[]={"Ripristina da una copia locale","Reinstalla tinymacOS","Utility Disco","Terminale","Disco di avvio","Crea una copia locale","Inizializza i dati utente"};
static const char *details[]={"Recupera documenti, account e preferenze salvati.","Ripristina il sistema senza modificare i documenti.","Controlla e ripara i volumi di questo computer."};
static Rect card(void){return (Rect){(W-660)/2,MAX(50,(H-390)/2),660,390};}
static Rect row(int i){Rect q=card();return (Rect){q.x+24,q.y+28+86*i,q.w-48,76};}
static Rect confirm_box(void){Rect q=card();return (Rect){q.x+35,q.y+95,q.w-70,205};}
static Rect disk_button(Rect a){return (Rect){a.x+24,a.y+65,a.w-48,30};}
static void refresh(void){sc(S_DISK,0,&disks,0,0);if(selected>=disks.count)selected=0;}
static void begin_operation(int op)
{
    refresh();
    if(op==2)launch(DISKUTIL);
    else if(op==3)launch(TERMINAL);
    else if(op==4||disks.count)armed=op;
    else strset(result,"Nessun disco disponibile.",sizeof result);
    dirty=1;
}
void recovery_background(void)
{
    recovery_mode=1;night=1;lite=1;
    for(int y=0;y<H;y++)for(int x=0;x<W;x++)back[y*W+x]=rec_background_pixel(x,y,W,H);
    refresh();trace("Recovery native desktop ready; RAM home; native USB input");
}
void recovery_draw(void)
{
    Rect q=card();shadow(q,16,52);roundr(q.x,q.y,q.w,q.h,16,INK(I_PANEL),255);
    for(int i=0;i<3;i++){
        Rect r=row(i);if(i==choice)roundr(r.x,r.y,r.w,r.h,8,0x3c3c40,255);
        const int icons[]={42,38,39};icon(r.x+12,r.y+14,48,icons[i]);
        text(r.x+80,r.y+15,titles[i],F_BODY,TX);
        text(r.x+80,r.y+41,details[i],F_SMALL,SUB);
    }
    button_face((Rect){q.x+q.w-150,q.y+q.h-54,122,32},0);
    center(q.x+q.w-89,q.y+q.h-48,"Continua",F_BODY,TX);
    if(armed>=0){
        Rect a=confirm_box();shadow(a,14,50);roundr(a.x,a.y,a.w,a.h,14,0x333336,255);
        center(W/2,a.y+18,titles[armed],F_BOLD,TX);
        if(armed!=4){
            Rect d=disk_button(a);button_face(d,0);
            const char *model=disks.count?disks.disk[selected].model:"Nessun disco disponibile";
            center(W/2,d.y+6,model,F_BODY,TX);
            line(d.x+d.w-20,d.y+12,d.x+d.w-15,d.y+17,0xb8b8bc);line(d.x+d.w-15,d.y+17,d.x+d.w-10,d.y+12,0xb8b8bc);
        }
        center(W/2,a.y+112,armed==6?"I dati verranno eliminati.":armed==4?"Riavvia per scegliere il disco dal firmware.":"Confermi l’operazione sul disco selezionato?",F_BODY,SUB);
        button_face((Rect){a.x+24,a.y+153,170,34},0);center(a.x+109,a.y+159,"Annulla",F_BODY,TX);
        button_face((Rect){a.x+a.w-194,a.y+153,170,34},0);center(a.x+a.w-109,a.y+159,"Continua",F_BODY,TX);
    }
}
int recovery_click(int x,int y)
{
    Rect q=card();
    if(armed>=0){
        Rect a=confirm_box();
        if(armed!=4&&inside(x,y,disk_button(a))){if(disks.count)selected=(selected+1)%disks.count;dirty=1;return 1;}
        if(inside(x,y,(Rect){a.x+24,a.y+153,170,34})){armed=-1;dirty=1;return 1;}
        if(inside(x,y,(Rect){a.x+a.w-194,a.y+153,170,34})){
            int op=armed;armed=-1;
            if(op==4)sc(S_POWER,1,0,0,0);
            else{static const int cmds[]={5,6,0,0,0,8,7};int r=sc(S_DISK,cmds[op],selected,0,0);
                strset(result,r?"Operazione non riuscita.":"Operazione completata.",sizeof result);refresh();}
            dirty=1;
        }
        return 1;
    }
    for(int i=0;i<3;i++)if(inside(x,y,row(i))){choice=i;dirty=1;return 1;}
    if(inside(x,y,(Rect){q.x+q.w-150,q.y+q.h-54,122,32})){begin_operation(choice);return 1;}
    return 0;
}

/* Classic, opaque Recovery menu bar. It is independent of the desktop shell:
 * no Dock, widgets, Control Center or Liquid Glass are enabled here. */
static int bar_menu=-1, bar_pick, info_open, power_confirm;
static Rect bar_hits[7], bar_popup;
static struct {const char *label;int action,enabled;} bar_items[8];
static int bar_count;
enum {RM_INFO=1001,RM_RESTART,RM_OFF,RM_STARTUP,RM_TERMINAL,RM_DISK,RM_CLOSE,RM_MIN,RM_COPY,RM_PASTE,RM_BACKUP,RM_ERASE};
static void recovery_power(int restart){power_confirm=restart?1:2;bar_menu=-1;dirty=1;}
static void menu_item(const char *s,int a,int enabled){if(bar_count<8){bar_items[bar_count].label=s;bar_items[bar_count].action=a;bar_items[bar_count++].enabled=enabled;}}
static void menu_contents(void)
{
    bar_count=0;int active=topwin()>=0;
    switch(bar_menu){
    case 0:menu_item("Disco di avvio…",RM_STARTUP,1);menu_item("Riavvia…",RM_RESTART,1);menu_item("Spegni…",RM_OFF,1);break;
    case 1:menu_item("Info sul Recupero",RM_INFO,1);if(active)menu_item("Esci dall’app",RM_CLOSE,1);break;
    case 2:menu_item("Chiudi finestra",RM_CLOSE,active);break;
    case 3:/* The maintenance clients expose no clipboard action. */menu_item("Copia",RM_COPY,0);menu_item("Incolla",RM_PASTE,0);break;
    case 4:menu_item("Terminale",RM_TERMINAL,1);menu_item("Utility Disco",RM_DISK,1);menu_item("Disco di avvio…",RM_STARTUP,1);menu_item("Crea una copia locale…",RM_BACKUP,1);menu_item("Inizializza i dati utente…",RM_ERASE,1);break;
    case 5:menu_item("Riduci a icona",RM_MIN,active);menu_item("Terminale",RM_TERMINAL,1);menu_item("Utility Disco",RM_DISK,1);break;
    case 6:menu_item("Info sul Recupero",RM_INFO,1);break;
    }
}
static void menu_action(int a)
{
    bar_menu=-1;dirty=1;
    if(a==RM_INFO)info_open=1;
    else if(a==RM_RESTART||a==RM_OFF)recovery_power(a==RM_RESTART);
    else if(a==RM_STARTUP)begin_operation(4);
    else if(a==RM_BACKUP)begin_operation(5);
    else if(a==RM_ERASE)begin_operation(6);
    else if(a==RM_TERMINAL)launch(TERMINAL);
    else if(a==RM_DISK)launch(DISKUTIL);
    else if(a==RM_CLOSE)action(142);
    else if(a==RM_MIN)action(141);
    else if(a==RM_COPY||a==RM_PASTE)eventto(topwin(),(Event){.type=EV_ACTION,.a=a==RM_COPY?ACT_COPY:ACT_PASTE});
}
static Rect message_box(void){return (Rect){(W-460)/2,(H-170)/2,460,170};}
void recovery_bar_draw(void)
{
    rect(0,0,W,BAR,0x242424);
    icon(11,3,24,31);
    int active=topwin(),x=43;
    const char *labels[]={"",active<0?"Recupero tinymacOS":appname(views[active].s.app),"File","Modifica","Utility","Finestra","Aiuto"};
    bar_hits[0]=(Rect){5,0,35,BAR};
    for(int k=0;k<7;k++){
        if(k){int w=textw(labels[k],k==1?F_BOLD:F_BODY)+22;bar_hits[k]=(Rect){x,0,w,BAR};x+=w;}
        Rect h=bar_hits[k];if(bar_menu==k)roundr(h.x,2,h.w,BAR-4,5,0x48484c,255);
        if(k)text(h.x+11,7,labels[k],k==1?F_BOLD:F_BODY,0xf2f2f2);
    }
    /* Draw Apple after its selected background, so the icon never disappears. */
    icon(11,3,24,31);
    char clock[16],n[8];number(clock,sys.hour);append(clock,":",sizeof clock);if(sys.minute<10)append(clock,"0",sizeof clock);number(n,sys.minute);append(clock,n,sizeof clock);
    text(W-textw(clock,F_BODY)-18,7,clock,F_BODY,0xe8e8ea);
    if(bar_menu>=0){
        menu_contents();int width=244;
        bar_popup=(Rect){MIN(bar_hits[bar_menu].x,W-width-8),BAR+2,width,bar_count*31+12};
        shadow(bar_popup,7,48);roundr(bar_popup.x,bar_popup.y,bar_popup.w,bar_popup.h,7,0x303033,255);
        for(int j=0;j<bar_count;j++){
            Rect row={bar_popup.x+5,bar_popup.y+6+31*j,bar_popup.w-10,29};
            if(bar_items[j].enabled&&(j==bar_pick||inside(mx,my,row)))roundr(row.x,row.y,row.w,row.h,4,0x4a4a4e,255);
            text(row.x+11,row.y+5,bar_items[j].label,F_BODY,bar_items[j].enabled?0xf2f2f2:0x838387);
        }
    }
    if(info_open||power_confirm||result[0]){
        Rect a=message_box();shadow(a,14,52);roundr(a.x,a.y,a.w,a.h,14,0x303033,255);
        const char *title=result[0]?"Recupero":info_open?"Recupero tinymacOS 10.2.3":power_confirm==1?"Riavviare il computer?":"Spegnere il computer?";
        center(W/2,a.y+20,title,F_BOLD,0xf2f2f2);
        center(W/2,a.y+59,result[0]?result:info_open?"Versione 10.2.3 · 30B14":"Le operazioni non salvate andranno perse.",F_BODY,0xc0c0c4);
        if(info_open||result[0]){button_face((Rect){a.x+a.w-126,a.y+118,102,34},0);center(a.x+a.w-75,a.y+124,"Fine",F_BODY,TX);}
        else{
            button_face((Rect){a.x+24,a.y+118,134,34},0);center(a.x+91,a.y+124,"Annulla",F_BODY,TX);
            button_face((Rect){a.x+a.w-180,a.y+118,156,34},0);center(a.x+a.w-102,a.y+124,power_confirm==1?"Riavvia":"Spegni",F_BODY,TX);
        }
    }
}
int recovery_bar_click(int x,int y)
{
    if(info_open||power_confirm||result[0]){
        Rect a=message_box();
        if(info_open||result[0]){if(inside(x,y,(Rect){a.x+a.w-126,a.y+118,102,34})){info_open=0;result[0]=0;dirty=1;}}
        else if(inside(x,y,(Rect){a.x+24,a.y+118,134,34})){power_confirm=0;dirty=1;}
        else if(inside(x,y,(Rect){a.x+a.w-180,a.y+118,156,34}))sc(S_POWER,power_confirm==1,0,0,0);
        return 1;
    }
    if(y<BAR){
        if(armed>=0)return 1;
        for(int k=0;k<7;k++)if(inside(x,y,bar_hits[k])){bar_menu=bar_menu==k?-1:k;bar_pick=-1;dirty=1;return 1;}
        bar_menu=-1;dirty=1;return 1;
    }
    if(bar_menu>=0){
        menu_contents();
        if(inside(x,y,bar_popup)){
            int j=(y-bar_popup.y-6)/31;
            if(y>=bar_popup.y+6&&j>=0&&j<bar_count&&bar_items[j].enabled)menu_action(bar_items[j].action);
        }else{bar_menu=-1;dirty=1;}
        return 1;
    }
    return 0;
}
int recovery_key(Event *e)
{
    if(info_open||power_confirm||result[0]){
        if(e->a==27){info_open=power_confirm=0;result[0]=0;dirty=1;}
        else if(e->a=='\n'){if(power_confirm)sc(S_POWER,power_confirm==1,0,0,0);else{info_open=0;result[0]=0;dirty=1;}}
        return 1;
    }
    if(armed>=0){
        if(e->a==27){armed=-1;dirty=1;}
        else if(e->a=='\n'){Rect a=confirm_box();recovery_click(a.x+a.w-100,a.y+170);}
        return 1;
    }
    if(e->a==K_F10){bar_menu=bar_menu<0?4:-1;bar_pick=-1;dirty=1;return 1;}
    if(bar_menu<0){
        if(topwin()<0&&armed<0){
            if(e->a==K_UP||e->a==K_DOWN){choice=(choice+(e->a==K_UP?2:1))%3;dirty=1;return 1;}
            if(e->a=='\n'){begin_operation(choice);return 1;}
        }
        return 0;
    }
    if(e->a==27){bar_menu=-1;dirty=1;return 1;}
    if(e->a==K_LEFT||e->a==K_RIGHT){bar_menu=(bar_menu+(e->a==K_LEFT?6:1))%7;bar_pick=-1;}
    menu_contents();
    if(e->a==K_UP||e->a==K_DOWN){
        for(int n=0;n<bar_count;n++){bar_pick=bar_pick<0?(e->a==K_UP?bar_count-1:0):(bar_pick+bar_count+(e->a==K_UP?-1:1))%bar_count;if(bar_items[bar_pick].enabled)break;}
    }else if(e->a=='\n'&&bar_pick>=0&&bar_pick<bar_count&&bar_items[bar_pick].enabled)menu_action(bar_items[bar_pick].action);
    dirty=1;return 1;
}
