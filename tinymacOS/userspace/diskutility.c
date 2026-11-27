#include "volume.h"
#include "display_size.h"
static DiskInfo d;
static SnapInfo sn[8];
static FsInfo fi;
static EncInfo ei;
static int sel, view, dialog, op, nsn,side_scroll;
static char msg[160], snapname[32];
static DiskEdit edit;
static char editsize[16],editname[48];
static int editfield;
static int decimal(const char*s,u32*n)
{
    u32 v=0;if(!*s)return -1;
    for(;*s;s++){if(*s<'0'||*s>'9'||v>TMC_DISK_MAX_MIB)return -1;v=v*10+*s-'0';}
    if(!v||v>TMC_DISK_MAX_MIB)return -1;*n=v;return 0;
}
static void edit_begin(int operation)
{
    if(sel<0||sel>=d.count||view<200)return;
    DiskMap*m=&d.disk[sel].map;memset(&edit,0,sizeof edit);
    edit.disk=sel;edit.op=operation;editfield=0;msg[0]=0;
    if(operation==DISK_EDIT_CREATE){
        int j=view-400;if(j<0||j>=m->gaps)return;
        DiskGap*g=m->gap+j;edit.start=((u64)g->start+2047)&~2047ull;
        if(edit.start>=g->start+g->sectors)return;
        edit.size=(g->sectors-(edit.start-g->start))&~2047u;
        strset(editname,"Senza titolo",sizeof editname);
    }else{
        int j=view-200;if(j<0||j>=m->count)return;
        DiskPartition*p=m->part+j;edit.number=p->number;edit.start=p->start;edit.sectors=p->sectors;edit.size=p->sectors;
        editname[0]=0;
    }
    editsize[0]=0;addnum(editsize,edit.size/2048,sizeof editsize);dialog=3;
}
static void edit_preview(void)
{
    if(edit.op==DISK_EDIT_DELETE){
        if(!eq(editname,"ELIMINA")){strset(msg,"Digita ELIMINA per confermare la selezione.",sizeof msg);return;}
    }else{
        u32 mb;if(decimal(editsize,&mb)){strset(msg,"Inserisci una dimensione intera in MiB.",sizeof msg);return;}
        edit.size=mb*2048;strset(edit.name,editname,sizeof edit.name);
    }
    if(!sc(S_DISK,10,&edit,0,0)){dialog=4;msg[0]=0;}
    else strset(msg,edit.message,sizeof msg);
}
static void lines(int x,int y,int w,const char*text)
{
    char b[160];int n=0;const char*p=text;
    while(*p){
        int last=0;n=0;
        while(p[n]&&n<158){b[n]=p[n];b[n+1]=0;if(b[n]==' ')last=n;
            if(TextWidth(b,F_SMALL)>w)break;n++;}
        if(!n)n=1;else if(p[n]&&last)n=last;
        memcpy(b,p,n);b[n]=0;Text(x,y,b,F_SMALL,SUB);y+=20;p+=n;while(*p==' ')p++;
    }
}
static void edit_paint(void)
{
    int tw=MIN(440,aw-48),xx=(aw-tw)/2,yy=(ah-282)/2;
    Sheet(xx,yy,tw,282,14);Icon(xx+20,yy+19,38,33);
    Text(xx+72,yy+23,edit.op==DISK_EDIT_CREATE?"Nuova partizione":edit.op==DISK_EDIT_DELETE?"Elimina partizione":"Ridimensiona partizione",F_CAPTION,TX);
    char target[64]="Disco ";addnum(target,edit.disk,sizeof target);
    if(edit.number){append(target," · partizione ",sizeof target);addnum(target,edit.number,sizeof target);}
    else{append(target," · LBA ",sizeof target);addnum(target,edit.start,sizeof target);}
    Text(xx+20,yy+66,target,F_SMALL,SUB);
    if(dialog==3){
        Text(xx+20,yy+101,edit.op==DISK_EDIT_DELETE?"Conferma":"Dimensione (MiB)",F_SMALL,TX);
        Box(xx+157,yy+94,tw-178,30,6,editfield==0?ACC:EDGE);Box(xx+158,yy+95,tw-180,28,5,CARD);
        const char *value=edit.op==DISK_EDIT_DELETE?editname:editsize;InputText(xx+168,yy+101,tw-200,value,strlen(value),editfield==0,0,TX);
        if(edit.op==DISK_EDIT_CREATE){
            Text(xx+20,yy+143,"Nome",F_SMALL,TX);Box(xx+157,yy+136,tw-178,30,6,editfield==1?ACC:EDGE);Box(xx+158,yy+137,tw-180,28,5,CARD);InputText(xx+168,yy+143,tw-200,editname,strlen(editname),editfield==1,0,TX);
        }
        lines(xx+20,yy+181,tw-40,msg[0]?msg:edit.op==DISK_EDIT_DELETE?"Digita ELIMINA. Il contenitore esteso include tutte le partizioni logiche.":edit.op==DISK_EDIT_CREATE?"BTRFS nativo: subset proprietario, non Btrfs Linux.":"Nativo: 8–32 MiB, slot variabili. FAT32: coda libera. NTFS: rilocazione e controllo Windows.");
    }else{
        lines(xx+20,yy+105,tw-40,edit.message);
        Text(xx+20,yy+188,"Esegui prima un backup. Non scollegare il disco.",F_SMALL,TX);
    }
    button(xx+tw-218,yy+237,88,"Annulla",0);
    button(xx+tw-119,yy+237,97,dialog==3?"Continua":dialog==5?"Attendi…":edit.op==DISK_EDIT_DELETE?"Elimina":"Applica",1);
}
static void edit_event(Event*e)
{
    int tw=MIN(440,aw-48),xx=(aw-tw)/2,yy=(ah-282)/2;
    if(e->type==EV_KEY){
        if(e->a==27){dialog=0;msg[0]=0;return;}
        if(e->a=='\n'){if(dialog==3)edit_preview();else if(dialog==4)dialog=5;return;}
        if(dialog!=3)return;
        if(e->a=='\t'&&edit.op==DISK_EDIT_CREATE){editfield=!editfield;return;}
        char*s=edit.op==DISK_EDIT_DELETE||editfield?editname:editsize;int cap=s==editname?sizeof editname:sizeof editsize,n=strlen(s);
        if((e->a==8||e->a==127)&&n)s[n-1]=0;
        else if(e->a>=32&&e->a<=126&&n<cap-1&&(s==editname||(e->a>='0'&&e->a<='9'))){s[n]=e->a;s[n+1]=0;}
        msg[0]=0;
    }
    if(e->type==EV_CLICK){
        if(dialog==3&&e->a>=xx+157&&e->a<xx+tw-21){if(e->b>=yy+94&&e->b<yy+124)editfield=0;if(e->b>=yy+136&&e->b<yy+166&&edit.op==DISK_EDIT_CREATE)editfield=1;}
        if(e->b>=yy+233&&e->b<yy+271){
            if(e->a>=xx+tw-218&&e->a<xx+tw-130){dialog=0;msg[0]=0;}
            if(e->a>=xx+tw-119&&e->a<xx+tw-22){if(dialog==3)edit_preview();else if(dialog==4)dialog=5;}
        }
    }
}

static void refresh(void)
{
    sc(S_DISK, 0, &d, 0, 0);
    if (d.count && sel >= d.count)sel = d.count - 1;
    if (sc(S_SNAP, 6, 0, 0, &ei))
        memset(&ei, 0, sizeof ei);
    nsn = sc(S_SNAP, 0, 0, sn, 8);
    if (nsn < 0 || sc(S_SNAP, 5, 0, &fi, 0)) {
        nsn = 0;
        memset(&fi, 0, sizeof fi);
    }
    if (view >= 100 && view < 200 && view - 100 >= nsn)view = -1;
}
static int installed(void)
{
    return sel >= 0 && sel < d.count && d.disk[sel].layout == 2 && sel == d.current && !si.live_cd;
}
static int nvol(void)
{
    if (sel < 0 || sel >= d.count || !d.disk[sel].layout)return 0;
    return d.disk[sel].layout == 2 ? 4 : 2;
}
static void d2(char *s, int cap, unsigned v)
{
    addnum(s, ((v >> 25) & 127) + 1980, cap);
    append(s, "-", cap);
    char z[4];
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
static void start(int a)
{
    if (view>=200 || sel < 0 || sel >= d.count || !d.disk[sel].layout) {
        strset(msg, "Seleziona un disco con un volume tinymacOS.", sizeof msg);
        return;
    }
    if (a != 1 && !installed()) {
        strset(msg, "Questa funzione richiede l’avvio dal disco installato.", sizeof msg);
        return;
    }
    if (a == 4) {
        // Automatic APFS-style name: snapshot-YYYY-MM-DD-HHMM
        char z[4];
        strset(snapname, "snapshot-", sizeof snapname);
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
    }
    if ((a == 5 || a == 7) && (view < 100 || view - 100 >= nsn)) {
        strset(msg, "Seleziona uno snapshot nella colonna laterale.", sizeof msg);
        return;
    }
    dialog = 1;
    op = a;
}
static void verify_now(void)
{
    if (view < 100 || view - 100 >= nsn) {
        strset(msg, "Seleziona uno snapshot nella colonna laterale.", sizeof msg);
        return;
    }
    int r = sc(S_SNAP, 3, sn[view - 100].name, 0, 0);
    strset(msg, r ? "Verifica fallita: immagine danneggiata." : "Snapshot valido: integrità crc32c verificata.",
           sizeof msg);
}
static void yes(void)
{
    if (dialog != 1)return;
    dialog = 2;
    strset(msg, "Operazione in corso…", sizeof msg);
}
static const char *names[] = {"Macintosh HD", "Macintosh HD - Dati", "Recovery", "Sistema locale"};
static u32 colors[] = {0x65a9f3, 0x85c4bb, 0xd5aff0, 0xa6b5c6};
static int rail(void)
{
    return aw < 600 ? 160 : 205;
}
static int side_row(int wanted,int *disk,int *v,char *label)
{
    int row=0;
    for(int i=0;i<d.count;i++){
        if(row++==wanted){*disk=i;*v=-1;strset(label,"Disco ",64);addnum(label,i,64);return 1;}
        if(i!=sel)continue;
        int native=nvol();
        for(int j=0;j<native;j++)if(row++==wanted){*disk=i;*v=j;strset(label,j==1?"Dati tinymacOS":names[j],64);return 1;}
        if(native==4)for(int j=0;j<nsn;j++)if(row++==wanted){*disk=i;*v=100+j;strset(label,sn[j].name,64);return 1;}
        DiskMap*m=&d.disk[i].map;
        for(int j=0;j<m->count;j++)if(row++==wanted){*disk=i;*v=200+j;strset(label,"s",64);addnum(label,m->part[j].number,64);append(label,"  ",64);append(label,filesystem_name(m->part[j].fs),64);return 1;}
        for(int j=0;j<m->gaps;j++)if(row++==wanted){char size[32];display_size(size,sizeof size,(u64)m->gap[j].sectors*512);*disk=i;*v=400+j;strset(label,"Non allocato · ",64);append(label,size,64);return 1;}
    }
    return 0;
}
static void side_move(int delta)
{
    char label[64];int disk,v,current=0,total=0;
    while(side_row(total,&disk,&v,label)){if(disk==sel&&v==view)current=total;total++;}
    int next=MAX(0,MIN(total-1,current+delta));if(side_row(next,&disk,&v,label)){sel=disk;view=v;}
    int slots=MAX(1,(ah-59)/27);if(next<side_scroll)side_scroll=next;if(next>=side_scroll+slots)side_scroll=next-slots+1;
}
static void pair(int x, int y, const char *k, const char *v)
{
    Text(x, y, k, F_SMALL, SUB);
    Text(x + 116, y - 1, v, F_BODY, TX);
}
static void paint(void)
{
    begin();
    int r = rail(), x = r + 22, w = aw - x - 19;
    int sh = view >= 100 && view - 100 < nsn;
    Box(0, 0, r, ah, 0, PANEL);
    Box(r, 0, 1, ah, 0, EDGE);
    Box(r + 1, 0, aw - r - 1, ah, 0, CARD);
    Text(15, 18, "Interni", F_BOLD, SUB);
    int y=54;
    for(int row=side_scroll;y+23<ah;row++,y+=27){int disk,v;char label[64];if(!side_row(row,&disk,&v,label))break;
        int selected=disk==sel&&v==view;if(selected)Box(7,y-3,r-14,26,7,SEL);
        Box(v<0?16:23,y+7,7,7,3,selected?ACC:SUB);
        if(strlen(label)>24){label[23]='.';label[24]=0;} /* narrow labels; full info at right */
        Text(v<0?32:38,y+2,label,F_SMALL,TX);
    }
    button(x, 13, 57, "SOS", 0);
    button(x + 65, 13, 89, "Copia dati", 0);
    button(x + 162, 13, 92, "Recupero…", 0);
    button(x + 262, 13, 104, "Nuovo snapshot", 0);
    Box(r + 1, 52, aw - r - 1, 1, 0, EDGE);
    if (!d.count) {
        Icon(x, 88, 50, 39);
        Text(x, 160, "Nessun disco collegato", F_CAPTION, TX);
        Text(x, 202, "Collega un disco e riapri Utility Disco.", F_SMALL, SUB);
        finish();
        return;
    }
    if (sh) {
        int k = view - 100;
        Icon(x, 75, 57, 39);
        Text(x + 69, 77, "Snapshot", F_CAPTION, TX);
        Text(x + 70, 109, sn[k].name, F_SMALL, SUB);
        char s[64] = "";
        addnum(s, sn[k].id, 64);
        pair(x, 155, "ID", s);
        s[0] = 0;
        d2(s, 64, sn[k].date);
        pair(x, 188, "Data", s);
        s[0] = 0;
        sizebadge(s, 1680 * 512, 64);
        pair(x, 221, "Dimensione", s);
        pair(x, 254, "Stato", "Immutabile · crc32c");
        button(x, 282, 110, "Ripristina", 1);
        button(x + 118, 282, 96, "Verifica", 0);
        button(x + 222, 282, 96, "Elimina", 0);
        Box(r + 1, ah - 45, aw - r - 1, 1, 0, EDGE);
        Text(x, ah - 31, msg[0] ? msg : "Snapshot CoW: blocchi condivisi, spazio occupato solo dalle modifiche.", F_SMALL, SUB);
        if (dialog) {
            int tw = MIN(440, aw - 48), xx = (aw - tw) / 2, yy = (ah - 196) / 2;
            Sheet(xx, yy, tw, 196, 14);
            Icon(xx + 20, yy + 17, 39, 39);
            Text(xx + 73, yy + 23, op == 5 ? "Ripristina snapshot" : op == 7 ? "Elimina snapshot" : "Nuovo snapshot",
                 F_CAPTION, TX);
            Text(xx + 20, yy + 75, op == 5 ? "Il volume torna allo stato puntuale dello snapshot." :
                 op == 7 ? "L’immagine verrà eliminata definitivamente." : "Viene creata una copia puntuale del volume.",
                 F_SMALL, TX);
            Text(xx + 20, yy + 99, op == 5 ? "I file creati dopo lo snapshot verranno persi." : op == 7 ?
                 "I dati dei file non vengono modificati." : "Il contenuto non cambia più al variare dei file.",
                 F_SMALL, SUB);
            button(xx + tw - 218, yy + 149, 88, "Annulla", 0);
            button(xx + tw - 119, yy + 149, 97, dialog == 2 ? "Attendi…" : "Continua", 1);
        }
        finish();
        return;
    }
    if(view>=200){
        DiskMap*m=&d.disk[sel].map;int free=view>=400,j=view-(free?400:200);
        u32 start=0,sectors=0;const char*title="Partizione",*format="Spazio non allocato";
        if(free){if(j>=m->gaps){view=-1;finish();return;}start=m->gap[j].start;sectors=m->gap[j].sectors;title="Spazio non allocato";}
        else{if(j>=m->count){view=-1;finish();return;}DiskPartition*p=m->part+j;start=p->start;sectors=p->sectors;format=filesystem_name(p->fs);title=p->name[0]?p->name:format;}
        Icon(x,75,57,33);Text(x+69,77,title,F_CAPTION,TX);Text(x+70,109,free?"Nessuna partizione in questa estensione":"Partizione del disco",F_SMALL,SUB);
        char b[64];display_size(b,sizeof b,(u64)sectors*512);pair(x,155,"Capacità",b);pair(x,188,"Formato",format);
        b[0]=0;addnum(b,start,64);pair(x,221,"Inizio LBA",b);b[0]=0;addnum(b,sectors,64);pair(x,254,"Settori",b);
        if(free)button(x,288,150,"Crea partizione…",1);
        else{button(x,288,110,"Elimina…",0);button(x+120,288,160,"Ridimensiona…",0);}
        Text(x,336,free?"Crea un volume dati nativo nello spazio selezionato.":"RAW non significa vuoto. Ogni modifica richiede conferma.",F_SMALL,SUB);
        Box(r+1,ah-45,aw-r-1,1,0,EDGE);Text(x,ah-31,msg[0]?msg:"Per modificare il disco di avvio, usa Recupero o il supporto live.",F_SMALL,SUB);
        if(dialog>=3)edit_paint();
        finish();return;
    }
    Icon(x, 75, 57, view == -1 ? 33 : 39);
    Text(x + 69, 77, view < 0 ? "Disco interno" : names[view], F_CAPTION, TX);
    Text(x + 70, 109, view < 0 ? d.disk[sel].model : "Volume del disco interno", F_SMALL, SUB);
    int mb = d.disk[sel].sectors / 2048, z[] = {20, MAX(0, mb - 45), 4, 20};
    if (d.disk[sel].layout != 2)z[1] = MAX(0, mb - 21);
    char s[64] = "";
    if (view == 1 && installed()) {
        display_size(s,sizeof s,fi.used);
    } else
        display_size(s,sizeof s,view>=0?(u64)z[view]*1048576:(u64)d.disk[sel].sectors*512);
    pair(x, 155, view == 1 && installed() ? "Utilizzo" : "Capacità", s);
    pair(x, 188, "Formato", view < 0 ? partition_scheme(d.disk[sel].map.scheme) : view == 1 ?
         "BTRFS subset (struttura APFS)" : view == 3 ? "Immagine di ripristino" : "Volume EFI (FAT16)");
    if (view == 1 && installed()) {
        char k[24] = "";
        addnum(k, fi.snaps, 24);
        pair(x, 221, "Snapshot", k);
        pair(x, 254, "Contenitore", "cont0");
        pair(x + w / 2, 254, "Crittografia",
             ei.state ? (ei.locked ? "Bloccata" : "Attiva") : "Disattiva");
    }
    int sw = w - 2;
    if (d.disk[sel].layout) {
        int n = d.disk[sel].layout == 2 ? 4 : 2, off = 0;
        for (int j = 0; j < n; j++) {
            int len = j == n - 1 ? sw - off : MAX(4, sw * z[j] / MAX(1, mb));
            Box(x + off, 275, len, 14, 2, colors[j]);
            off += len;
        }
        for (int j = 0; j < n; j++) {
            int xx = x + (j % 2) * (w / 2), yy = 300 + j / 2 * 24;
            Box(xx, yy + 5, 8, 8, 4, colors[j]);
            Text(xx + 14, yy, names[j], F_SMALL, SUB);
        }
    } else {
        Box(x, 275, sw, 14, 6, SOFT);
        Text(x, 305, "Seleziona una partizione o lo spazio non allocato nella barra laterale.", F_SMALL, SUB);
    }
    Box(r + 1, ah - 45, aw - r - 1, 1, 0, EDGE);
    Text(x, ah - 31, msg[0] ? msg : d.disk[sel].backup ? "Copia dei dati disponibile in Recupero." :
         view == 1 ? "Crea uno snapshot con «Nuovo snapshot»: resta incolonnato a sinistra." :
         "Seleziona un volume per vederne i dettagli.", F_SMALL, SUB);
    if (dialog) {
        int tw = MIN(440, aw - 48), xx = (aw - tw) / 2, yy = (ah - 196) / 2;
        Sheet(xx, yy, tw, 196, 14);
        Icon(xx + 20, yy + 17, 39, 39);
        Text(xx + 73, yy + 23, op == 1 ? "Esegui SOS" : op == 2 ? "Crea una copia locale" : op == 3 ? "Apri Recupero" :
             "Nuovo snapshot", F_CAPTION, TX);
        Text(xx + 20, yy + 75, op == 1 ? "Controlla i dati e ripara la mappa delle partizioni." : op == 2 ?
             "Salva documenti, account e preferenze sul disco." : op == 3 ?
             "Il computer si riavvierà in Recupero tinymacOS." :
             "Crea una copia puntuale del volume con nome automatico.", F_SMALL, TX);
        Text(xx + 20, yy + 99, op == 2 ? "L’eventuale copia precedente verrà sostituita." : op == 3 ?
             "Prima di continuare, salva i documenti aperti." : op == 4 ? snapname : "Non scollegare il disco.",
             F_SMALL, SUB);
        button(xx + tw - 218, yy + 149, 88, "Annulla", 0);
        button(xx + tw - 119, yy + 149, 97, dialog == 2 ? "Attendi…" : "Continua", 1);
    }
    finish();
}
void app_main(void)
{
    win(810, 460, "Utility Disco");
    refresh();
    sel = d.current >= 0 ? d.current : 0;
    view = -1;
    paint();
    for (;;) {
        Event e;
        int changed = 0;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            changed = 1;
            if (baseevent(&e))continue;
            if (e.type == EV_CLOSE)quit();
            if(dialog>=3){edit_event(&e);continue;}
            if (dialog) {
                if (e.type == EV_KEY) {
                    if (e.a == 27)dialog = 0;
                    if (e.a == '\n')yes();
                }
                if (e.type == EV_CLICK) {
                    int tw = MIN(440, aw - 48), xx = (aw - tw) / 2, yy = (ah - 196) / 2;
                    if (e.b >= yy + 145 && e.b < yy + 181) {
                        if (e.a >= xx + tw - 119 && e.a < xx + tw - 22)yes();
                        else if (e.a >= xx + tw - 220 && e.a < xx + tw - 130)dialog = 0;
                    }
                }
                continue;
            }
            if (e.type == EV_KEY) {
                if (e.a == 's')start(1);
                if (e.a == 'b')start(2);
                if (e.a == 'r')start(3);
                if (e.a == 'n')start(4);
                if (e.a == K_UP || e.a == K_DOWN) {
                    side_move(e.a==K_DOWN?1:-1);
                    msg[0] = 0;
                }
            }
            if (e.type == EV_CLICK) {
                int r = rail(), x = r + 22;
                if (e.a < r) {
                    int row=side_scroll+(e.b-54)/27,dd,vv;char label[64];
                    if(e.b>=51&&side_row(row,&dd,&vv,label)){sel=dd;view=vv;msg[0]=0;}
                } else if (e.b >= 10 && e.b < 43) {
                    if (e.a >= x && e.a < x + 58)start(1);
                    else if (e.a >= x + 65 && e.a < x + 155)start(2);
                    else if (e.a >= x + 162 && e.a < x + 255)start(3);
                    else if (e.a >= x + 262 && e.a < x + 366)start(4);
                } else if(view>=200&&e.b>=284&&e.b<325){
                    if(view>=400&&e.a>=x&&e.a<x+150)edit_begin(DISK_EDIT_CREATE);
                    else if(view<400){if(e.a>=x&&e.a<x+110)edit_begin(DISK_EDIT_DELETE);else if(e.a>=x+120&&e.a<x+280)edit_begin(DISK_EDIT_RESIZE);}
                } else if (view >= 100 && view<200 && e.b >= 278 && e.b < 316) {
                    if (e.a >= x && e.a < x + 110)start(5);
                    else if (e.a >= x + 118 && e.a < x + 214)verify_now();
                    else if (e.a >= x + 222 && e.a < x + 318)start(7);
                }
            }
        }
        if (changed)paint();
        if(dialog==5){
            edit.confirm=DISK_EDIT_CONFIRM;
            int result=sc(S_DISK,11,&edit,0,0);
            strset(msg,edit.message,sizeof msg);dialog=0;
            refresh();if(!result){view=-1;side_scroll=0;}paint();
        }
        if (dialog == 2) {
            int rr;
            const char *okmsg;
            if (op >= 4) {
                const char *nm = op == 4 ? snapname : sn[view - 100].name;
                rr = sc(S_SNAP, op - 3, nm, 0, 0);
                okmsg = op == 4 ? "Snapshot creato." : op == 5 ? "Volume ripristinato." : "Snapshot eliminato.";
            } else {
                rr = sc(S_DISK, op, sel, 0, 0);
                okmsg = op == 2 ? "Copia locale salvata e verificata." : "SOS completato. Il volume è valido.";
            }
            dialog = 0;
            if (op == 4 && !rr) {
                strset(msg, "Snapshot creato: ora visibile nella colonna laterale.", sizeof msg);
            } else if (op == 5 && !rr) {
                strset(msg, "Volume ripristinato nello stato dello snapshot.", sizeof msg);
            } else if (op == 7 && !rr) {
                strset(msg, "Snapshot eliminato.", sizeof msg);
            } else
                strset(msg, rr ? "Operazione non riuscita. Controlla il volume in Recupero." : okmsg, sizeof msg);
            refresh();
            paint();
        }
        slp(4);
    }
}
