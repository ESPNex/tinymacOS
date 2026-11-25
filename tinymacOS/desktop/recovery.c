#include "desk.h"
#include "../recovery/background.h"
/* Native Recovery is a maintenance desktop in a separate kernel. All input,
 * storage and window rendering uses syscalls; no firmware protocol is retained. */
static DiskInfo disks;
static int selected, armed=-1;
static char result[100];
static const char *titles[]={"Ripristina da una copia locale","Reinstalla tinymacOS","Utility Disco","Terminale","Disco di avvio","Crea una copia locale","Inizializza i dati utente"};
static const char *details[]={"Recupera documenti, account e preferenze salvati.","Ripristina il sistema senza modificare i documenti.","Controlla e ripara i volumi di questo computer.","Comandi di recupero nella sessione RAM isolata.","Riavvia e torna alla selezione del firmware.","Salva una copia dei dati del disco selezionato.","Elimina account, documenti e preferenze del disco."};
static Rect card(void){return (Rect){(W-660)/2,MAX(45,(H-550)/2),660,550};}
static Rect row(int i){Rect q=card();return (Rect){q.x+24,q.y+82+50*i,q.w-48,46};}
static void refresh(void){sc(S_DISK,0,&disks,0,0);if(selected>=disks.count)selected=0;}
void recovery_background(void)
{
    night=0;lite=0;
    for(int y=0;y<H;y++)for(int x=0;x<W;x++)back[y*W+x]=rec_background_pixel(x,y,W,H);
    refresh();
    trace("Recovery native desktop ready; RAM home; native USB input");
}
void recovery_draw(void)
{
    /* One material boundary only: no owner window, outline or nested frame. */
    Rect q=card();shadow(q,20,52);glass(q,20,MAT_DIALOG);
    center(W/2,q.y+20,"Recupero tinymacOS",F_TITLE,TX);
    center(W/2,q.y+53,"10.2.1 · kernel dedicato · USB nativa",F_CAPTION,SUB);
    for(int i=0;i<7;i++){
        Rect r=row(i);if(inside(mx,my,r))roundr(r.x,r.y,r.w,r.h,9,INK(I_SEL),100);
        int icons[]={42,38,39,2,14,42,39};icon(r.x+6,r.y+6,32,icons[i]);
        text(r.x+52,r.y+3,titles[i],F_BODY,TX);
        text(r.x+52,r.y+25,details[i],F_CAPTION,SUB);
    }
    char disk[100]="Disco: ";
    if(disks.count){append(disk,disks.disk[selected].model,sizeof disk);append(disk,"  [clic per cambiare]",sizeof disk);}
    else append(disk,"nessun disco SATA disponibile",sizeof disk);
    text(q.x+28,q.y+444,disk,F_CAPTION,TX);
    text(q.x+28,q.y+470,result,F_CAPTION,SUB);
    text(q.x+28,q.y+511,"Riavvia",F_BODY,TX);text(q.x+q.w-94,q.y+511,"Spegni",F_BODY,TX);
    if(armed>=0){
        Rect a={q.x+35,q.y+180,q.w-70,180};shadow(a,16,50);glass(a,16,MAT_DIALOG);
        center(W/2,a.y+22,titles[armed],F_BOLD,TX);
        center(W/2,a.y+58,"Confermi l’operazione sul disco selezionato?",F_BODY,TX);
        if(armed==6)center(W/2,a.y+84,"I dati verranno eliminati.",F_BODY,INK(I_ACC));
        button_face((Rect){a.x+24,a.y+126,170,34},0);center(a.x+109,a.y+132,"Annulla",F_BODY,TX);
        button_face((Rect){a.x+a.w-194,a.y+126,170,34},1);center(a.x+a.w-109,a.y+132,"Continua",F_BODY,INK(I_ACC_TXT));
    }
}
int recovery_click(int x,int y)
{
    Rect q=card();
    if(armed>=0){
        Rect a={q.x+35,q.y+180,q.w-70,180};
        if(inside(x,y,(Rect){a.x+24,a.y+126,170,34})){armed=-1;dirty=1;return 1;}
        if(inside(x,y,(Rect){a.x+a.w-194,a.y+126,170,34})){
            int op=armed;armed=-1;strset(result,"Operazione in corso…",sizeof result);dirty=1;
            if(op==4)sc(S_POWER,1,0,0,0);
            else{static const int cmds[]={5,6,0,0,0,8,7};
                int r=sc(S_DISK,cmds[op],selected,0,0);
                strset(result,r?"Operazione non riuscita: controlla il disco e la copia.":"Operazione completata. Puoi riavviare.",sizeof result);refresh();}
        }
        return 1;
    }
    if(inside(x,y,(Rect){q.x+24,q.y+438,q.w-48,27})){
        refresh();if(disks.count)selected=(selected+1)%disks.count;result[0]=0;dirty=1;return 1;}
    if(inside(x,y,(Rect){q.x+24,q.y+504,120,36})){sc(S_POWER,1,0,0,0);return 1;}
    if(inside(x,y,(Rect){q.x+q.w-120,q.y+504,100,36})){sc(S_POWER,0,0,0,0);return 1;}
    for(int i=0;i<7;i++)if(inside(x,y,row(i))){
        if(i==2)launch(DISKUTIL);
        else if(i==3)launch(TERMINAL);
        else if(i==4 || disks.count){armed=i;dirty=1;}
        else{strset(result,"Collega un disco SATA compatibile.",sizeof result);dirty=1;}
        return 1;
    }
    return 0;
}
