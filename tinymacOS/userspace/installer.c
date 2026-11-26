#include "ui.h"
// The assistant is an ordinary Ring-3 window. Only the kernel owns disk addresses.
static InstallInfo info;
static int page, choice = -1, confirm, dirty = 1;
static int reinstall, reinstall_pending;
static int encrypted,focus;
static VaultRequest encryption;
static char password[2][65],key_check[9],enc_error[96];
static void encryption_clear(void){
 memset(&encryption,0,sizeof encryption);memset(password,0,sizeof password);memset(key_check,0,sizeof key_check);encrypted=0;focus=0;enc_error[0]=0;
}
static void encryption_discard(void){
 VaultRequest q={.disk=-1};sc(S_INSTALL,5,&q,0,0);memset(&q,0,sizeof q);encryption_clear();
}
static DiskInfo recovery_disks;
static int can_reset[SETUP_MAX_DISKS];
static void paint(void);
static void refresh(void)
{
    sc(S_INSTALL, 0, &info, 0, 0);
    if(reinstall){
        sc(S_DISK,0,&recovery_disks,0,0);
        info.count=recovery_disks.count;
        for(int i=0;i<info.count;i++){
            InstallDisk*q=info.disk+i;
            VaultRequest vq={.disk=i};VaultInfo vi={0};sc(S_FILEVAULT,0,&vq,&vi,0);can_reset[i]=vi.format==3&&!vi.locked&&!vi.error;
            q->sectors=recovery_disks.disk[i].sectors;
            q->scheme=recovery_disks.disk[i].map.scheme;
            strset(q->model,recovery_disks.disk[i].model,sizeof q->model);
            q->usable=recovery_disks.disk[i].layout==2&&recovery_disks.disk[i].healthy;
            strset(q->reason,q->usable?"Sistema nativo rilevato; documenti conservati.":recovery_disks.disk[i].layout!=2?"Nessun tinymacOS installato: usa il desktop live.":"Sistema rilevato ma non reinstallabile: apri Utility Disco.",sizeof q->reason);
        }
        return;
    }
    if (info.state == INSTALL_DONE)
        page = 4;
    else if (info.state == INSTALL_ERROR)
        page = 5;
    else if (info.state >= INSTALL_SCAN && info.state <= INSTALL_VERIFY)
        page = 3;
    if (!info.live)
        page = 6;
}
static void go(int n)
{
    page = n;
    dirty = 1;
    ui_motion(FX_SLIDE);
}
static int can(void)
{
    return choice >= 0 && choice < info.count && info.disk[choice].usable;
}
static void next(void)
{
    if(page==7){
        encryption.disk=choice;strset(encryption.password,password[0],65);strset(encryption.replacement,password[1],65);
        int r=sc(S_INSTALL,5,&encryption,0,0);memset(password,0,sizeof password);memset(encryption.password,0,65);memset(encryption.replacement,0,65);
        if(r){strset(enc_error,reinstall?"Sblocca prima FileVault; password uguali di almeno 8 caratteri.":"Password diverse, meno di 8 caratteri o entropia non pronta.",sizeof enc_error);return;}
        enc_error[0]=0;go(8);return;
    }
    if(page==8){
        if(!eq(key_check,encryption.recovery+56)){strset(enc_error,"Gli 8 caratteri non coincidono.",sizeof enc_error);return;}
        strset(encryption.password,key_check,65);
        if(sc(S_INSTALL,6,&encryption,0,0)){memset(encryption.password,0,65);strset(enc_error,"Preparazione scaduta: torna indietro e riprova.",sizeof enc_error);return;}
        memset(encryption.password,0,65);memset(encryption.recovery,0,65);memset(key_check,0,sizeof key_check);encrypted=1;confirm=0;go(2);return;
    }
    if (page == 0) {
        refresh();
        if (info.live || reinstall)
            go(1);
    } else if (page == 1 && can()) {
        confirm = 0;
        go(2);
    } else if (page == 2 && confirm && can()) {
        if(reinstall){reinstall_pending=1;go(3);}
        else{sc(S_INSTALL, 1, choice, encrypted, 0);refresh();dirty=1;}
    } else if (page == 4) {
        if(reinstall)quit();else sc(S_POWER, 0, 0, 0, 0);
    } else if (page == 5) {
        if(!reinstall)sc(S_INSTALL, 3, 0, 0, 0);
        encryption_discard();
        refresh();
        choice = -1;
        go(1);
    } else if (page == 6)
        quit();
}
static void back(void)
{
    if(page==7||page==8){encryption_discard();go(2);return;}
    if (page == 1)
        go(0);
    else if (page == 2) {
        confirm = 0;
        encryption_discard();
        go(1);
    } else if (page == 3 && !reinstall && info.state == INSTALL_SCAN && sc(S_INSTALL, 3, 0, 0, 0) == 0) {
        encryption_discard();
        refresh();
        go(1);
    } else if (page == 0 || page >= 4){
        encryption_discard();quit();
    }
}
static void diskname(char*s, int d)
{
    strset(s, "Disco ", 80);
    addnum(s, d, 80);
    append(s, "  ·  ", 80);
    append(s, info.disk[d].model, 80);
}
static int rail(void){return 0;}
static void paint(void)
{
    begin();
    int r = rail(), x = r + 24, w = aw - x - 23, foot = ah - 52;
    Box(0, 0, aw, ah, 0, CARD);
    if(page==7){
        Text(x,20,reinstall?"Nuove credenziali del volume":"Proteggi la nuova installazione",F_TITLE,TX);
        Text(x,70,reinstall?"OPERAZIONE IRREVERSIBILE: elimina dati, account e snapshot.":"FileVault cifra dati, account e snapshot fin dalla creazione.",F_SMALL,SUB);
        Text(x,94,"Scegli la password del volume (8–64 caratteri).",F_SMALL,SUB);
        Text(x,115,reinstall?"Nuove chiavi; dopo il riavvio creerai un nuovo account.":"La password dell’account importato resta distinta e invariata.",F_SMALL,SUB);
        for(int i=0;i<2;i++){
            Text(x,150+i*65,i?"Conferma password":"Password del volume",F_SMALL,SUB);
            Box(x,173+i*65,w,32,6,i==focus?SEL:PANEL);char dots[65];int n=strlen(password[i]);memset(dots,'*',n);dots[n]=0;Text(x+12,181+i*65,dots,F_MONO,TX);memset(dots,0,sizeof dots);
        }
        if(enc_error[0])Text(x,293,enc_error,F_SMALL,SUB);
        Text(x,318,reinstall?"Non è una cancellazione fisica sicura del supporto.":"Capacità utile: circa l’80% dello spazio dati grezzo.",F_SMALL,SUB);
    }else if(page==8){
        Text(x,20,"Salva la chiave di recupero",F_TITLE,TX);
        Text(x,67,"Conservala fuori dal disco. Non viene salvata nei documenti.",F_SMALL,SUB);
        char half[33];memcpy(half,encryption.recovery,32);half[32]=0;Text(x,104,half,F_MONO,TX);memcpy(half,encryption.recovery+32,32);Text(x,131,half,F_MONO,TX);memset(half,0,sizeof half);
        Text(x,180,"Conferma gli ultimi 8 caratteri della chiave:",F_SMALL,SUB);
        Box(x,207,w,32,6,SEL);Text(x+12,215,key_check,F_MONO,TX);
        Text(x,255,"Non perdere password e chiave: i dati non sono recuperabili.",F_SMALL,SUB);
        if(enc_error[0])Text(x,293,enc_error,F_SMALL,SUB);
    }else if (!page) {
        Icon(aw/2-64, 30, 128, 38);
        Center(aw/2, 178, "tinymacOS Tramontana", F_TITLE, TX);
        Center(aw/2, 229, reinstall?"Per reinstallare il sistema, fai clic su Continua.":"Per configurare l’installazione di tinymacOS, fai clic su Continua.", F_SMALL, SUB);
        Center(aw/2, 266, reinstall?"Richiede tinymacOS gia installato; documenti e account conservati.":"Le partizioni esistenti non verranno inizializzate.", F_SMALL, SUB);
        if(reinstall)Center(aw/2, 300, "Per una nuova installazione avvia il desktop live della ISO.", F_SMALL, SUB);
    } else if (page == 1) {
        Center(aw/2, 19, "Seleziona la destinazione", F_TITLE, TX);
        Center(aw/2, 61, reinstall?"Scegli il sistema nativo da reinstallare senza inizializzare i dati.":"tinymacOS verrà installato soltanto nello spazio non allocato.", F_SMALL, SUB);
        if (!info.count) {
            Text(x, 122, "Nessun disco rilevato.", F_BOLD, TX);
            Text(x, 154, reinstall?"Collega il disco con l’installazione da ripristinare.":"Collega un disco con almeno 96 MiB non allocati.", F_SMALL, SUB);
        }
        int slots = MAX(1, (foot - 104) / 64), first = MAX(0, choice - slots + 1);
        for (int i = first; i < info.count && i < first + slots; i++) {
            int y = 98 + (i - first) * 64;
            Box(x, y, w, 59, 9, i == choice ? SEL : SOFT);
            Icon(x + 7, y + 4, 35, 33);
            char s[80];
            diskname(s, i);
            Text(x + 53, y + 3, s, F_SMALL, TX);
            s[0] = 0;
            if(!reinstall&&info.disk[i].available&&info.disk[i].available<2048)append(s,"<1",80);else addnum(s, (reinstall?info.disk[i].sectors:info.disk[i].available) / 2048, 80);
            append(s, reinstall?" MiB · ":" MiB non allocati · ",80);append(s,partition_scheme(info.disk[i].scheme),80);
            append(s,info.disk[i].usable?"":" · Non disponibile",80);
            Text(x + 53, y + 23, s, F_MICRO, SUB);
            Text(x + 53, y + 41, !reinstall&&info.disk[i].usable&&!info.disk[i].repair_mbr_signature?info.disk[i].formats:info.disk[i].reason,F_MICRO,SUB);
        }
    } else if (page == 2) {
        Text(x, 20, reinstall?(encrypted?"Conferma inizializzazione cifrata":"Conferma la reinstallazione"):"Conferma l’installazione", F_TITLE, TX);
        char s[80];
        diskname(s, choice);
        Icon(x, 77, 46, 33);
        Text(x + 58, 82, s, F_BOLD, TX);
        s[0] = 0;
        addnum(s, (reinstall?info.disk[choice].sectors:info.disk[choice].available) / 2048, 80);
        append(s, reinstall?" MiB · ":" MiB non allocati · ",80);append(s,partition_scheme(info.disk[choice].scheme),80);
        Text(x + 58, 106, s, F_SMALL, SUB);
        Box(x, 146, w, 1, 0, EDGE);
        Text(x, 164, reinstall?(encrypted?"Tutti i dati utente e le vecchie chiavi verranno eliminati.":"Verrà riscritto soltanto il volume di sistema selezionato."):"Sistema, dati e Recupero useranno solo la nuova destinazione.", F_SMALL, TX);
        Text(x, 189, reinstall?(encrypted?"Sistema e Recovery restano invariati. Serve il nuovo enrollment.":"Documenti, account, FileVault e snapshot restano invariati."):"Le altre partizioni e il loro contenuto restano invariati.", F_SMALL, SUB);
        Text(x, 209, reinstall?(encrypted?"Un’interruzione può rendere i dati inaccessibili. Nessun rollback.":"Si usa la copia locale verificata, non un aggiornamento online."):info.disk[choice].repair_mbr_signature?"Firma MBR 55 AA assente: verra ripristinata dopo conferma.":"L’avvio BIOS avrà un menu; il codice MBR originale è conservato.", F_SMALL, SUB);
        if(info.encryption_available&&(!reinstall||(can()&&can_reset[choice]))){
            button(x,240,310,reinstall?"Inizializza dati cifrati…":encrypted?"FileVault attivo · Modifica…":"Proteggi con FileVault…",0);
            Text(x+326,247,encrypted?"Chiave confermata":reinstall?"Volume già sbloccato":"Facoltativo · tasto E",F_SMALL,SUB);
        }
        Box(x, foot - 66, w, 48, 9, SOFT);
        Box(x + 12, foot - 52, 20, 20, 5, confirm ? ACC : EDGE);
        if (confirm)
            Center(x + 22, foot - 53, "✓", F_SMALL, si.dark ? 0x142525 : 0xffffff);
        Text(x + 42, foot - 51, !reinstall&&info.disk[choice].repair_mbr_signature?"Confermo destinazione e ripristino firma MBR":"Confermo la destinazione selezionata", F_SMALL, TX);
    } else if (page == 3) {
        Icon(x, 23, 52, 38);
        Text(x, 95, reinstall?(encrypted?"Inizializzazione cifrata":"Reinstallazione in corso"):"Installazione in corso", F_TITLE, TX);
        Text(x, 150, reinstall?(encrypted?"Nuove chiavi e nuova struttura dati…":"Verifica della copia locale e ripristino del sistema…"):info.message, F_BODY, SUB);
        int pct = !reinstall && info.total ? (u64)info.done * 100 / info.total : 0;
        Box(x, 198, w, 8, 4, EDGE);
        if (pct)
            Box(x, 198, MAX(5, w * pct / 100), 8, 4, ACC);
        char s[24] = "";
        addnum(s, pct, 24);
        append(s, "%", 24);
        Text(x, 223, reinstall?"Non spegnere il computer.":s, F_SMALL, EMP);
        Text(x, foot - 30, "Lascia acceso il computer fino alla verifica finale.", F_SMALL, SUB);
    } else if (page == 4) {
        Photo(x, 15, w, MAX(57, foot - 212), 12, 11);
        int y = MAX(57, foot - 212) + 32;
        Text(x, y, reinstall?(encrypted?"Dati inizializzati":"Reinstallazione completata"):"Il disco è pronto", F_TITLE, TX);
        Text(x, y + 47, reinstall?(encrypted?"Volume cifrato nuovo e verificato, vecchie chiavi ritirate.":"Il sistema è stato verificato dopo la riscrittura."):"Puoi avviare tinymacOS senza il supporto di installazione.", F_SMALL, SUB);
        Text(x, y + 81, reinstall?(encrypted?"Sblocca con la nuova password e crea un nuovo account.":"Documenti e account sono stati conservati."):"1.  Spegni il computer.", F_BODY, TX);
        Text(x, y + 112, reinstall?"Chiudi l’assistente e riavvia dal menu Recupero.":"2.  Rimuovi il supporto di installazione.", F_BODY, TX);
        Text(x, y + 143, reinstall?"":"3.  BIOS: scegli 2. UEFI: avvia EFI/BOOT/BOOTX64.EFI dal nuovo volume.", F_BODY, TX);
    } else {
        Icon(x, 18, 58, page == 5 ? 33 : 38);
        Text(x, 106, page == 5 ? (reinstall?"Reinstallazione non riuscita":"Installazione interrotta") : "Sistema già installato", F_TITLE, TX);
        if (page == 5) {
            Text(x, 168, reinstall?"Il ripristino non è completo: non forzare l’avvio del sistema.":"Il disco non è stato attivato dall’assistente.", F_BODY, TX);
            Text(x, 212, info.message, F_SMALL, SUB);
        } else {
            Text(x, 167, "Stai usando il sistema avviato dal disco.", F_BODY, TX);
            Text(x, 209, "Per una nuova installazione, avvia dalla ISO.", F_SMALL, SUB);
        }
    }
    Box(r, foot, aw - r, 1, 0, EDGE);
    if (page == 3) {
        if (!reinstall && info.state == INSTALL_SCAN)
            button(aw - 117, foot + 13, 95, "Annulla", 0);
    } else {
        if (page != 6)
            button(x, foot + 13, 85, page == 0 || page >= 4 ? "Chiudi" : "Indietro", 0);
        const char *label = page == 2 ? (reinstall?(encrypted?"Inizializza":"Reinstalla"):"Installa") : page == 4 ? (reinstall?"Fine":"Spegni") : page == 5 ? "Torna ai dischi" : page == 6 ? "Fine" : page==8?"Ho salvato la chiave":
                            "Continua";
        int enabled = (page != 1 || can()) && (page != 2 || confirm);
        if (enabled)
            button(aw - (page==8?210:151), foot + 13, page==8?186:127, label, 1);
        else {
            Box(aw - 151, foot + 13, 127, 26, 7, SOFT);
            Center(aw - 88, foot + 17, label, F_SMALL, SUB);
        }
    }
    finish();
}
void app_main(void)
{
    sc(S_INFO,&si,0,0,0);
    win(760, 440, installer_title(si.recovery));
    reinstall=!!si.recovery;
    if(!reinstall)encryption_discard();
    refresh();
    paint();
    for (;;) {
        Event e;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            dirty = 1;
            if (baseevent(&e))
                continue;
            if (e.type == EV_CLOSE) {
                if (page != 3){
                    encryption_discard();quit();
                }
                else if (info.state == INSTALL_SCAN && sc(S_INSTALL, 3, 0, 0, 0) == 0)
                    quit();
            }
            if (e.type == EV_KEY) {
                if(page==7&&e.a!=27&&e.a!='\n'){
                    if(e.a=='\t')focus^=1;else text_append_key(password[focus],65,e.a);
                }else if(page==8&&e.a!=27&&e.a!='\n')text_append_key(key_check,9,e.a);
                else if(page==2&&info.encryption_available&&(!reinstall||(can()&&can_reset[choice]))&&(e.a=='e'||e.a=='E')){encryption_discard();go(7);}
                else if (e.a == 27)
                    back();
                else if (e.a == '\n')
                    next();
                else if (page == 2 && e.a == ' ')
                    confirm = !confirm;
                else if (page == 1 && (e.a == K_UP || e.a == K_DOWN)) {
                    if (info.count)
                        choice = MAX(0, MIN(info.count - 1, choice + (e.a == K_UP ? -1 : 1)));
                }
            }
            if (e.type == EV_CLICK) {
                int foot = ah - 52, x = rail() + 24;
                if(page==7&&e.b>=173&&e.b<270){focus=e.b>=238;}
                else if(page==2&&info.encryption_available&&(!reinstall||(can()&&can_reset[choice]))&&e.b>=240&&e.b<266){encryption_discard();go(7);}
                else if (e.b >= foot + 9 && e.b <= foot + 44) {
                    if (e.a >= aw - (page==8?210:151)) {
                        if (page == 3)
                            back();
                        else
                            next();
                    } else if (e.a >= x && e.a <= x + 85)
                        back();
                } else if (e.a >= x && e.a < aw - 21 && page == 1 && e.b >= 98 && e.b < foot) {
                    int i = MAX(0, choice - MAX(1, (foot - 104) / 64) + 1) + (e.b - 98) / 64;
                    if (i < info.count)
                        choice = i;
                } else if (page == 2 && e.b >= foot - 66 && e.b < foot - 18 && e.a >= x)
                    confirm = !confirm;
            }
        }
        if(page==3&&reinstall_pending){
            /* Present the same wizard's busy page before the synchronous,
             * kernel-serialized restore; no invented percentage or success. */
            paint();reinstall_pending=0;
            int r=sc(S_INSTALL,encrypted?7:4,choice,0,0);
            if(r)strset(info.message,encrypted?"Inizializzazione incompleta: dati forse inaccessibili. Non forzare l’avvio.":"Copie locali, tabelle o I/O non validi. Nessun esito positivo dichiarato.",sizeof info.message);
            go(r?5:4);
        }
        if (page == 3 && !reinstall) {
            if (sc(S_INSTALL, 2, 0, 0, 0) >= 0 || info.live) {
                refresh();
                dirty = 1;
            }
        }
        if (dirty) {
            paint();
            dirty = 0;
        }
        slp(page == 3 ? 1 : 4);
    }
}
