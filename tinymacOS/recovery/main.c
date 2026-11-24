#include "screen.h"
#include "window_chrome.h"
#include "../boot/recoveryboot.h"
#include "startup.h"
#include "hidtablet.h"
#include "text_input.h"
#include "firmware_pointer_scale.h"
int rec_pointer_start(EFI_BOOT_SERVICES *,int,int);
int rec_pointer_relative(void);
int rec_pointer_next(EFI_BOOT_SERVICES *,TabletState *);
int rec_keyboard_start(EFI_BOOT_SERVICES *);
int rec_keyboard_next(EFI_BOOT_SERVICES *,int *);
// This EFI application is the entire recovery runtime: no user session, compositor, Dock or desktop app archive.
static struct RecoveryOrigin origin;
static EFI_SYSTEM_TABLE *st;
static EFI_BOOT_SERVICES *bs;
static EFI_BLOCK_IO *ios[8];
static EFI_HANDLE devs[8];
static Medium media[8];
static int types[8], num, sel, mx, my, menu, row, op, page, dirty = 1;
static char info[128], field[98], rows[16][96];
static int nr;
// SIP variable read with legacy-global fallback (2.3.0 and earlier wrote the global namespace).
static UINT32 sip_get(EFI_SYSTEM_TABLE *t)
{
    EFI_GUID gv = TMC_SIP_GUID;
    UINTN csz = 4;
    UINT32 csr = 1;
    if (t->RuntimeServices->GetVariable(L"tinymacOS-csr", &gv, NULL, &csz, &csr) == EFI_SUCCESS)
        return csr;
    EFI_GUID gg = EFI_GLOBAL_VARIABLE;
    if (t->RuntimeServices->GetVariable(L"tinymacOS-csr", &gg, NULL, &csz, &csr) == EFI_SUCCESS)
        return csr;
    return 1;                    /* absent => enabled, like a new Mac */
}
static Layout layout;
enum {HOME_MENU, DEST, TOOLS, CONFIRM, RESULT, TERM, POWER};
enum {BACKUP = 1, REINSTALL, AID, CONSOLE, START, ERASE, SNAPSHOT};
static const char *items[] = {"Ripristina da una copia locale", "Reinstalla tinymacOS", "Utility Disco", "Terminale", "Disco di avvio"};
static const char *desc[] = {"Recupera documenti, account e preferenze salvati.", "Ripristina il sistema senza modificare i tuoi documenti.", "Controlla e ripara i volumi di questo computer.", "Comandi di recupero.", "Seleziona il sistema da avviare."};
static int ico[] = {1, 2, 3, 4, 5};
static u8 alignedio[CHUNK * 512] ALIGNED(4096);
static int bio(int d, u32 l, u32 n, void *p, int wr)
{
    if (d < 0 || d >= num || !n || l > media[d].z || n > media[d].z - l)return -1;
    EFI_BLOCK_IO *i = ios[d];
    if (n > CHUNK)return -1;
    if (wr)memcpy(alignedio, p, n * 512);
    EFI_STATUS r = wr ? i->WriteBlocks(i, i->Media->MediaId, l, n * 512, alignedio) : i->ReadBlocks(i,
                   i->Media->MediaId, l, n * 512, alignedio);
    if (EFI_ERROR(r))return -1;
    if (!wr)memcpy(p, alignedio, n * 512);
    return 0;
}
static int flush(int d)
{
    return EFI_ERROR(ios[d]->FlushBlocks(ios[d])) ? -1 : 0;
}
static void disks(void)
{
    EFI_GUID g = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_HANDLE *hs;
    UINTN n;
    if (EFI_ERROR(bs->LocateHandleBuffer(ByProtocol, &g, 0, &n, &hs)))return;
    for (UINTN j = 0; j < n && num < 8; j++) {
        EFI_BLOCK_IO *i;
        if (EFI_ERROR(bs->HandleProtocol(hs[j], &g, (void **)&i)) || !i->Media->MediaPresent ||
            i->Media->LogicalPartition || i->Media->ReadOnly || i->Media->BlockSize != 512 ||
            i->Media->LastBlock >= 0xffffffffu || i->Media->IoAlign > 4096)continue;
        int d = num++;
        ios[d] = i;
        devs[d] = hs[j];
        media[d] = (Medium) {
            bio, flush, d, i->Media->LastBlock + 1
        };
        types[d] = layout_read(media + d, &layout);
    }
    bs->FreePool(hs);
    for (int j = 0; j < num; j++)if (types[j] == 2) {
            sel = j;
            break;
        }
}
static int x0, y0, ww, hh, positioned, held, dragging, dragx, dragy;
static void button(int x, int y, int w, const char *s, int blue)
{
    fill(x, y, w, 29, 8, blue ? 0x147bf3 : 0xe9e9ef);
    label(x + (w - tw(0, s)) / 2, y + 6, 0, s, blue ? 0xffffff : 0x303137);
}
static void dn(int d, char *s)
{
    strset(s, types[d] ? "Macintosh HD" : "Disco non configurato", 92);
    append(s, "  ·  ", 92);
    char z[24];
    number(z, media[d].z / 2048);
    append(s, z, 92);
    append(s, " MiB  (disk", 92);
    number(z, d);
    append(s, z, 92);
    append(s, ")", 92);
}
static void result(const char *s)
{
    strset(info, s, sizeof info);
    page = RESULT;
    dirty = 1;
}
static void lineout(const char *s)
{
    if (nr == 16) {
        memmove(rows, rows + 1, 15 * 96);
        nr--;
    }
    strset(rows[nr++], s, 96);
}
static const char *tools[] = {"SOS: controlla e ripara", "Salva una copia dei dati", "Inizializza i dati utente…"};
static void dots(void)
{
    /* Red opens the power confirmation; minimize/zoom are visibly disabled
     * in the standalone UEFI environment, not fake colored controls. */
    for(int i=0;i<3;i++)
        fill(x0+TMC_CHROME_LIGHT_X+TMC_CHROME_LIGHT_STEP*i,y0+TMC_CHROME_LIGHT_Y,
             TMC_CHROME_LIGHT_SIZE,TMC_CHROME_LIGHT_SIZE,6,i?0xc7c9ce:tmc_chrome_light(0));
}
static void paint(void)
{
    const int TH = TMC_CHROME_TITLE;      /* window title bar, like other applications */
    ww = MIN(1012, rw - 120);
    hh = MIN(560, rh - 96);
    if(!positioned){x0=(rw-ww)/2;y0=(rh-hh)/2;positioned=1;}
    x0=MAX(8,MIN(rw-ww-8,x0));y0=MAX(42,MIN(rh-hh-8,y0));
    fill(0, 0, rw, rh, 0, 0xe9eaf0);
    fill(0, 0, rw, 34, 0, 0x34353a);
    menu_apple(12,3);
    label(50, 6, 1, "tinymacOS Recovery", 0xffffff);
    label(rw - 109, 9, 0, "Spegni…", 0xf2f2f5);
    const char *head = page == HOME_MENU ? "tinymacOS Recovery" : page == DEST ? "Seleziona un disco" : page ==
                       TOOLS ? "Utility Disco" : page == TERM ? "Terminale" : page == CONFIRM ? "Conferma l’operazione" : page ==
                       POWER ? "Spegnere il computer?" : "Recupero";
    window_frame(x0,y0,ww,hh);
    dots();
    label(x0 + (ww - tw(2, head)) / 2, y0 + TMC_CHROME_TEXT_Y, 2, head, 0x2e2f36);
    int x = x0 + 30, y = y0 + TH + 18;
    const int BY = y0 + hh - 46;         /* bottom button row */
    fill(x0 + 1, BY - 8, ww - 2, 1, 0, 0xe3e3e8);
    if (page == HOME_MENU) {
        for (int j = 0; j < 5; j++) {
            int yy = y + 36 + 62 * j;
            fill(x0 + 18, yy, ww - 36, 57, 10, j == menu ? 0xe4efff : 0xfdfdfe);
            glyph(x0 + 32, yy + 9, 39, ico[j]);
            /* Card titles use the real 14px Regular face, not Bold/Semibold. */
            label(x0 + 85, yy + 6, 1, items[j], 0x242532);
            label(x0 + 85, yy + 30, 0, desc[j], 0x616473);
        }
    }
    if (page == DEST) {
        label(x, y, 0, "Scegli il volume su cui intervenire.", 0x696a73);
        for (int j = 0; j < num; j++) {
            int yy = y + 42 + j * 40;
            char s[92];
            dn(j, s);
            fill(x0 + 24, yy, ww - 48, 36, 9, j == sel ? 0xe4efff : 0xf5f5f8);
            glyph(x0 + 34, yy + 5, 26, 1);
            label(x0 + 74, yy + 8, 1, s, 0x252634);
        }
        if (!num)label(x, y + 62, 1, "Nessun disco disponibile.", 0x565961);
    }
    if (page == TOOLS) {
        char s[92];
        dn(sel, s);
        label(x, y, 0, "Gestisci il volume selezionato.", 0x696a73);
        glyph(x, y + 48, 52, 3);
        label(x + 70, y + 61, 2, s, 0x282933);
        for (int j = 0; j < 3; j++) {
            int yy = y + 132 + 52 * j;
            fill(x0 + 25, yy, ww - 50, 44, 9, j == row ? 0xdfeeff : 0xf2f2f6);
            label(x0 + 46, yy + 10, 1, tools[j], 0x23252c);
        }
    }
    if (page == CONFIRM) {
        char s[92];
        dn(sel, s);
        label(x, y, 0, "Nessuna modifica è stata ancora effettuata.", 0x696a73);
        label(x, y + 48, 2, s, 0x22242b);
        const char *t = op == ERASE ? "Account, documenti e copia locale verranno cancellati." : op == BACKUP ?
                        "La copia salvata sostituirà i documenti e l’account attuali." : op == REINSTALL ?
                        "Il sistema verrà reinstallato. I dati personali resteranno intatti." :
                        "La copia locale precedente verrà sostituita.";
        label(x, y + 100, 1, t, 0x393a44);
        label(x, y + 152, 1, op == ERASE ? "Scrivi INIZIALIZZA per continuare." : "Seleziona Conferma per procedere.",
              0x494a52);
        if (op == ERASE) {
            fill(x, y + 196, 245, 36, 8, 0xe7edf8);
            label(x + 8, y + 204, 1, field, 0x252631);
        }
    }
    if (page == RESULT) {
        glyph(x, y + 46, 56, 3);
        label(x, y + 140, 1, info, 0x33353d);
    }
    if (page == TERM) {
        // Dark shell body filling the whole window, like a normal application window.
        int by = y, bx = x0 + 18, bw = ww - 36, bh = BY - y - 14;
        fill(bx, by, bw, bh, 12, 0x1e1e24);
        int yy = by + 12;
        for (int j = 0; j < nr; j++) {
            label(bx + 17, yy, 1, rows[j], 0xe8e8ec);
            yy += 23;
        }
        char q[120] = "recovery % ";
        append(q, field, 120);
        label(bx + 17, yy + 1, 1, q, 0xffffff);
    }
    if (page == POWER) label(x, y + 42, 1, "Potrai riaccenderlo quando vuoi.", 0x494a53);
    if (page != HOME_MENU)button(x0 + 23, BY, 92, "Indietro", 0);
    if (page != TERM)button(x0 + ww - 151, BY, 128,
                                page == RESULT ? "Fine" : page == CONFIRM ? "Conferma" : page == POWER ? "Spegni" : "Continua", 1);
    show(mx, my);
}
static void startdisk(void)
{
    EFI_GUID fs = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_HANDLE *handles, target = 0;
    UINTN count, length = dpsize(devpath(devs[sel]));
    if (length > 4 && !EFI_ERROR(bs->LocateHandleBuffer(ByProtocol, &fs, 0, &count, &handles))) {
        EFI_DEVICE_PATH *base = devpath(devs[sel]);
        for (UINTN j = 0; j < count; ++j) {
            int part = 0;
            EFI_DEVICE_PATH *dp = devpath(handles[j]);
            if (hd_prefix(dp, &part) == length - 4 && part == 1 && !memcmp(dp, base, length - 4)) {
                target = handles[j];
                break;
            }
        }
        bs->FreePool(handles);
    }
    if (target) boot_once(st, target);
    result("Rimuovi i supporti esterni e riavvia dal disco nelle opzioni UEFI.");
}
static u8 snapshot[NDATA * 512] ALIGNED(4096);
static void perform(void)
{
    Medium *m = media + sel;
    if (op == ERASE && !eq(field, "INIZIALIZZA"))return;
    result("Operazione in corso. Non spegnere il computer.");
    paint();
    int r = -1;
    if (op == REINSTALL)r = restore_os(m);
    if (op == BACKUP)r = restore_copy(m);
    if (op == ERASE)r = reset_data(m);
    if (op == SNAPSHOT) {
        int ok = 1;
        for (u32 i = 0; i < NDATA;
             i += CHUNK)if (bio(sel, LDATA + i, MIN(CHUNK, NDATA - i), snapshot + 512 * i, 0))ok = 0;
        if (ok)r = save_copy(m, snapshot);
        erase(snapshot, sizeof snapshot);
    }
    result(r ? "Operazione non riuscita. Controlla il volume con SOS." : op == REINSTALL ?
           "tinymacOS è stato reinstallato. I tuoi documenti sono rimasti sul disco." : op == BACKUP ?
           "Copia ripristinata. Riavvia dal volume Macintosh HD." : op == ERASE ?
           "Volume dati inizializzato. Configura l’account al prossimo avvio." : "Copia dei dati salvata e verificata.");
}
static void enter(void)
{
    dirty = 1;
    if (page == HOME_MENU) {
        op = menu + 1;
        if (op == CONSOLE) {
            page = TERM;
            nr = 0;
            field[0] = 0;
            lineout("Usa help per i comandi disponibili.");
        } else page = DEST;
        return;
    }
    if (page == DEST) {
        if (!num || types[sel] != 2) {
            result("Seleziona un disco Tramontana con Recupero locale.");
            return;
        }
        if (op == START) {
            startdisk();
            return;
        }
        if (op == AID) {
            row = 0;
            page = TOOLS;
        } else if (op == BACKUP && !has_copy(media + sel))result("Su questo volume non è presente una copia locale.");
        else {
            page = CONFIRM;
            field[0] = 0;
        }
        return;
    }
    if (page == TOOLS) {
        if (row == 0) {
            int r = firstaid(media + sel, 1);
            result(r ? "SOS non ha potuto completare la riparazione." :
                   "SOS completato. La mappa dei volumi e il volume dati sono validi.");
        } else {
            op = row == 1 ? SNAPSHOT : ERASE;
            page = CONFIRM;
            field[0] = 0;
        }
        return;
    }
    if (page == CONFIRM) {
        perform();
        return;
    }
    if (page == POWER) {
        st->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, 0);
        return;
    }
    page = HOME_MENU;
}
static void term(void)
{
    char s[110] = "% ";
    append(s, field, 110);
    lineout(s);
    if (eq(field, "help")) {
        lineout("diskutil list  |  diskutil info /  |  diskutil verifyVolume");
        lineout("csrutil status  |  csrutil enable  |  csrutil disable");
        lineout("sw_vers  |  date  |  exit  |  reboot");
    } else if (eq(field, "csrutil")) {
        lineout("Uso: csrutil status | csrutil enable | csrutil disable");
    } else if (eq(field, "csrutil status")) {
        UINT32 csr = sip_get(st);
        lineout((csr & 1) ? "System Integrity Protection: attiva."
                          : "System Integrity Protection: disattiva.");
    } else if (eq(field, "csrutil enable") || eq(field, "csrutil disable")) {
        EFI_GUID gv = TMC_SIP_GUID;
        UINT32 v = eq(field, "csrutil enable") ? 1u : 0u;
        EFI_STATUS r = st->RuntimeServices->SetVariable(
            L"tinymacOS-csr", &gv,
            EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
                EFI_VARIABLE_RUNTIME_ACCESS,
            4, &v);
        if (EFI_ERROR(r)) lineout("Scrittura NVRAM non riuscita.");
        else lineout(v ? "SIP attiva. Riavviare per applicare."
                       : "SIP disattivata. Riavviare per applicare.");
    } else if (eq(field,"diskutil info /")) {
        if (origin.magic==ORIGIN_MAGIC) {
            lineout("Immagine: /Recovery/BaseSystem.dmg");
            lineout(origin.optical ? "Origine: ISO di installazione" : origin.partition==3 ?
                    "Origine: disco interno, partizione Recovery" : "Origine: volume UEFI");
            char z[40]; number(z,origin.image_bytes); append(z," byte verificati",sizeof z); lineout(z);
        } else lineout("Avvio diretto del programma UEFI.");
    }
    else if (eq(field, "diskutil list")) {
        for (int i = 0; i < num; i++) {
            dn(i, s);
            lineout(s);
        }
    } else if (eq(field, "diskutil verifyVolume"))lineout(num &&
                !firstaid(media + sel, 0) ? "Volume valido." : "Verifica non riuscita.");
    else if (eq(field, "sw_vers"))lineout("tinymacOS Recovery 10.1.9 (30B10)");
    else if (eq(field, "date")) {
        EFI_TIME t;
        if (!EFI_ERROR(st->RuntimeServices->GetTime(&t, 0))) {
            char z[24];
            number(s, t.Day);
            append(s, "/", 110);
            number(z, t.Month);
            append(s, z, 110);
            append(s, "/", 110);
            number(z, t.Year);
            append(s, z, 110);
            lineout(s);
        }
    } else if (eq(field, "exit"))page = HOME_MENU;
    else if (eq(field, "reboot"))st->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, 0);
    else if (*field)lineout("Comando non disponibile in Recupero.");
    field[0] = 0;
}
static void back(void)
{
    if (page == CONFIRM && (op == ERASE || op == SNAPSHOT)) {
        op = AID;
        page = TOOLS;
    } else if (page == TOOLS)page = DEST;
    else page = HOME_MENU;
    dirty = 1;
}
static void click(void)
{
    if (my < 34 && mx > rw - 125) {
        page = POWER;
        dirty = 1;
        return;
    }
    const int TH = TMC_CHROME_TITLE;
    if (mx < x0 || mx >= x0 + ww || my < y0 || my >= y0 + hh)return;
    if(my<y0+TMC_CHROME_TITLE && mx>=x0+TMC_CHROME_LIGHT_X && mx<x0+TMC_CHROME_LIGHT_X+TMC_CHROME_LIGHT_SIZE){page=POWER;dirty=1;return;}
    if (my >= y0 + hh - 54) {
        if (mx >= x0 + ww - 165 && page != TERM)enter();
        else if (mx < x0 + 135 && page != HOME_MENU)back();
        return;
    }
    int yy = y0 + TH + 18;
    int j;
    if (page == HOME_MENU && (j = (my - yy - 36) / 62) >= 0 && my >= yy + 36 && j < 5)menu = j;
    else if (page == DEST && my >= yy + 42 && (j = (my - yy - 42) / 40) < num)sel = j;
    else if (page == TOOLS && my >= yy + 132 && (j = (my - yy - 132) / 52) < 3)row = j;
    dirty = 1;
}
static void pointer_button(int down)
{
    if(down&&!held){
        if(mx>x0+83&&mx<x0+ww&&my>=y0&&my<y0+TMC_CHROME_TITLE){dragging=1;dragx=mx-x0;dragy=my-y0;}
        else click();
    }
    if(down&&dragging){x0=MAX(8,MIN(rw-ww-8,mx-dragx));y0=MAX(42,MIN(rh-hh-8,my-dragy));}
    if(!down)dragging=0;
    held=down;dirty=1;
}
/* Prefer firmware pointer protocols. Direct UsbIo is only for unclaimed HID. */
#ifdef TMC_DEBUG
static void pointer_trace(UINTN id,EFI_SIMPLE_POINTER_PROTOCOL *source,EFI_SIMPLE_POINTER_STATE *state)
{
    static unsigned count;if(count++>=24)return;
    const char *prefix="[efi-pointer] ";
    for(const char *p=prefix;*p;p++){unsigned char c=*p;asm volatile("outb %0,%1"::"a"(c),"Nd"((unsigned short)0x3f8));}
    u64 values[]={id,(u32)state->RelativeMovementX,(u32)state->RelativeMovementY,source->Mode?source->Mode->ResolutionX:0,source->Mode?source->Mode->ResolutionY:0};
    for(unsigned k=0;k<5;k++){for(int shift=60;shift>=0;shift-=4){unsigned char c="0123456789abcdef"[(values[k]>>shift)&15];asm volatile("outb %0,%1"::"a"(c),"Nd"((unsigned short)0x3f8));}unsigned char c=k==4?'\n':' ';asm volatile("outb %0,%1"::"a"(c),"Nd"((unsigned short)0x3f8));}
}
#endif
static void poll_pointer_sources(EFI_ABSOLUTE_POINTER_PROTOCOL *absolute,
    EFI_SIMPLE_POINTER_PROTOCOL **pointers,int *pointer_down,UINTN npointers)
{
        static EFI_ABSOLUTE_POINTER_PROTOCOL *last_absolute;
        static int abs_valid,abs_x,abs_y,abs_down;
        static EFI_SIMPLE_POINTER_PROTOCOL *simple_source[16];
        static RecPointerAxis simple_axes[16][2];
        int got_abs = 0;
        if (absolute && !got_abs) {
            EFI_ABSOLUTE_POINTER_STATE p;
            if (!EFI_ERROR(absolute->GetState(absolute,&p))) {
                EFI_ABSOLUTE_POINTER_MODE *m=absolute->Mode;
                if(!m)return;
                int x=pointer_pixel(p.CurrentX,m->AbsoluteMinX,m->AbsoluteMaxX,rw);
                int y=pointer_pixel(p.CurrentY,m->AbsoluteMinY,m->AbsoluteMaxY,rh);
                int down=!!(p.ActiveButtons & EFI_ABSP_TouchActive);
                /* A firmware absolute device can return EFI_SUCCESS forever
                 * without fresh activity. It must not hide SimplePointer USB.
                 * Compare against that source, not the shared on-screen cursor. */
                if(last_absolute!=absolute || !abs_valid || x!=abs_x || y!=abs_y || down!=abs_down) {
                    mx=x;my=y;pointer_button(down);got_abs=1;dirty=1;
                }
                last_absolute=absolute;abs_valid=1;abs_x=x;abs_y=y;abs_down=down;
            }
        }
        if (!got_abs) for(UINTN i=0;i<npointers;i++) {
            EFI_SIMPLE_POINTER_STATE p;EFI_SIMPLE_POINTER_PROTOCOL *pointer=pointers[i];
            if (!EFI_ERROR(pointer->GetState(pointer, &p))) {
#ifdef TMC_DEBUG
                pointer_trace(i,pointer,&p);
#endif
                int down=!!p.LeftButton;
                if(!p.RelativeMovementX && !p.RelativeMovementY && down==pointer_down[i])continue;
                pointer_down[i]=down;
                if(i>=COUNT(simple_source))continue;
                if(simple_source[i]!=pointer){
                    simple_source[i]=pointer;memset(simple_axes[i],0,sizeof simple_axes[i]);
                }
                int dx=rec_pointer_pixels(&simple_axes[i][0],p.RelativeMovementX,pointer->Mode&&pointer->Mode->ResolutionX?pointer->Mode->ResolutionX:8);
                int dy=rec_pointer_pixels(&simple_axes[i][1],p.RelativeMovementY,pointer->Mode&&pointer->Mode->ResolutionY?pointer->Mode->ResolutionY:8);
                mx = MAX(0, MIN(rw - 1, (i64)mx + dx));
                my = MAX(0, MIN(rh - 1, (i64)my + dy));
                pointer_button(down);dirty=1;
            }
        }
}
/* Firmware may install mouse children late, or after hotplug. Snapshot the
 * protocols periodically, without resetting devices or stealing ownership. */
static void pointer_sources(EFI_SIMPLE_POINTER_PROTOCOL **pointers,UINTN *npointers,
                            EFI_ABSOLUTE_POINTER_PROTOCOL **absolute)
{
    /* OpenDuet can publish an idle PS/2 pointer before the USB mouse.
     * Poll every SimplePointer instead of trusting LocateProtocol's first hit. */
    *npointers=0;*absolute=0;
    EFI_GUID pg = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    EFI_HANDLE *ph=0;UINTN pn=0;
    if(!EFI_ERROR(bs->LocateHandleBuffer(ByProtocol,&pg,0,&pn,&ph))){
        for(UINTN i=0;i<pn && *npointers<16;i++){
            EFI_SIMPLE_POINTER_PROTOCOL *p=0;
            if(!EFI_ERROR(bs->HandleProtocol(ph[i],&pg,(void **)&p)) && p){
                pointers[(*npointers)++]=p;
            }
        }
        bs->FreePool(ph);
    }
    EFI_GUID ag = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;
    EFI_HANDLE *handles; UINTN count;
    if (!EFI_ERROR(bs->LocateHandleBuffer(ByProtocol,&ag,0,&count,&handles))) {
        for (UINTN i=0;i<count;i++) {
            EFI_ABSOLUTE_POINTER_PROTOCOL *a=0;
            if (EFI_ERROR(bs->HandleProtocol(handles[i],&ag,(void **)&a)) || !a || !a->Mode) continue;
            if (a->Mode->AbsoluteMaxX <= a->Mode->AbsoluteMinX || a->Mode->AbsoluteMaxY <= a->Mode->AbsoluteMinY) continue;
            *absolute=a;
            if (handles[i]!=st->ConsoleInHandle) break;
        }
        bs->FreePool(handles);
    }


}
EFI_STATUS _entry(EFI_HANDLE image, EFI_SYSTEM_TABLE *sys)
{
    EFI_GUID lg=LOADED_IMAGE_PROTOCOL;EFI_LOADED_IMAGE *li=0;
    if (!EFI_ERROR(sys->BootServices->HandleProtocol(image,&lg,(void **)&li)) && li &&
        li->LoadOptions && li->LoadOptionsSize==sizeof origin && ((struct RecoveryOrigin *)li->LoadOptions)->magic==ORIGIN_MAGIC)
        memcpy(&origin,li->LoadOptions,sizeof origin);
    st = sys;
    fw = st->BootServices;
    bs = st->BootServices;
    bs->SetWatchdogTimer(0, 0, 0, 0);
    st->ConOut->EnableCursor(st->ConOut, FALSE);
    if (!screen_init(st))return EFI_UNSUPPORTED;
    disks();
    mx = rw - 48;
    my = rh - 55;
    int keyboards=rec_keyboard_start(bs);
    if(!keyboards)strset(info,"Tastiera: nessun dispositivo HID boot o PS/2 disponibile",sizeof info);
    EFI_SIMPLE_POINTER_PROTOCOL *pointers[16]={0};
    int pointer_down[16]={0};UINTN npointers=0;
    EFI_ABSOLUTE_POINTER_PROTOCOL *absolute=0;
    pointer_sources(pointers,&npointers,&absolute);
    int direct=rec_pointer_start(bs,mx,my);unsigned pointer_ticks=0;

    for (;;) {
        if (dirty) {
            paint();
            dirty = 0;
        }
        int c;
        if(rec_keyboard_next(bs,&c)){
            dirty=1;
            if(c==27)back();
            else if(c==10||c==13){if(page==TERM)term();else enter();}
            else if(c==0x101||c==0x102){
                int d=c==0x102?1:-1;
                if(page==HOME_MENU)menu=(menu+5+d)%5;
                else if(page==TOOLS)row=(row+3+d)%3;
                else if(page==DEST&&num)sel=(sel+num+d)%num;
            } else if(page==TERM||(page==CONFIRM&&op==ERASE))text_append_key(field,sizeof field,c);
        }
        if(++pointer_ticks%170==0){
            pointer_sources(pointers,&npointers,&absolute);
            if(!direct)direct=rec_pointer_start(bs,mx,my);
        }
        TabletState pointer_state;
        for(int n=0;direct&&n<32&&rec_pointer_next(bs,&pointer_state);n++){
            if(rec_pointer_relative()){
                mx=MAX(0,MIN(rw-1,(i64)mx+pointer_state.x));
                my=MAX(0,MIN(rh-1,(i64)my+pointer_state.y));
            }else{mx=pointer_state.x;my=pointer_state.y;}
            pointer_button(!!(pointer_state.buttons&1));dirty=1;
        }
        poll_pointer_sources(absolute,pointers,pointer_down,npointers);
        bs->Stall(12000);
    }
    return EFI_SUCCESS;
}
