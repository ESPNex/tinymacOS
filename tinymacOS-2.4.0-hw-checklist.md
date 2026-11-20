# tinymacOS 2.4.0 «Levante» — Istruzioni Rufus e checklist per il test su hw reale

**Data:** 2026-11-20 · **Immagine di riferimento:** `/var/tmp/tmc210/retail/tinymacOS-2.4.0-x86_64.iso`
**SHA-256 (retail, 20 nov 2026 03:51):** `2cd87878e18ec1eced082aa2a55362b3c67ab923fdb198fc68d2096dcd31e62d`
**Dimensione:** 122 832 896 byte

> ⚠️ **Avviso.** Il test tocca dati veri. I passaggi in sezione 7 (installazione) e 11
> (ripristino/inizializzazione) sono **distruttivi**. Esegui il flusso solo su una
> macchina di cui hai fatto il backup completo, e togli **tutta** la logica di
> «Inizializza i dati utente» finché non sei pronto a buttare account e documenti.
>
> Su **macchine di produzione** non insistere: se qualcosa cade in errore, fermati e
> manda il risultato (sezione 12) — non forzare reinstallazioni ripetute.

---

## 1. Cosa ti serve

- Una chiavetta USB **da almeno 1 GB** (la ISO è 117 MB estratti) — verrà
  **completamente sovrascritta**. Verifica di aver salvato ciò che contiene.
- Un PC **x86-64 UEFI** (niente Mac, niente BIOS legacy, niente ARM — nessun Apple Silicon,
  nessun Chromebook).
- **Secure Boot: OFF** (il nostro caricatore EFI non è firmato).
- **CSM / modalità Legacy / BIOS compatibile: OFF**. Questa immagine è UEFI pura (nessuna
  coppia magic 0x55AA nel settore 0, nessun bootloader BIOS/CSM).
- Tempo: ~10 min per i test live (sezioni 5–6), ~25 min con installazione completa (7).

## 2. Verifica l'immagine PRIMA di bruciarla

Su Windows (PowerShell):

```powershell
certutil -hashfile tinymacOS-2.4.0-x86_64.iso SHA256
```

Su macOS:

```bash
shasum -a 256 tinymacOS-2.4.0-x86_64.iso
```

Su Linux:

```bash
sha256sum tinymacOS-2.4.0-x86_64.iso
```

L'hash deve corrispondere a quello della sezione iniziale (retail) — oppure al tuo build
personale se l'hai ricompilata tu (in tal caso tieni lo SHA-256 della tua ISO, non quello
di questo documento).

Se non corrisponde, **non bruciarla**: scaricala/ricompilala di nuovo.

## 3. Creazione della chiavetta con Rufus (Windows)

1. Scarica Rufus da https://rufus.ie/ e aprilo.
2. **Device**: seleziona la chiavetta giusta (controlla capacità e lettera di unità).
3. **Boot selection** → **Disk or ISO image (Please select)** → `SELECT` → scegli il file
   `tinymacOS-2.4.0-x86_64.iso`.
4. **Modalità di scrittura — importante**:
   - Alla richiesta *«Write in DD Image mode?»* / *«in ISO Image mode?»* scegli
     **ISO Image mode** (è la modalità testata in emulazione con una replica del layout
     che ne risulta: FAT32 + `Install.esp`, `System/Library/KernelCollections/KernelCache`,
     ecc. sul root del volume).
   - **Non scegliere DD Image mode**: questa ISO **non è ibrida** (nessun Boot signature
     `0x55AA` in LBA 0, nessuna MBR/GPT ibrida). Una scrittura grezza non produrrebbe un
     volume che il firmware UEFI riconosce come avviabile da USB.
   - **Non usare Ventoy** né altri caricatori "multi-ISO": non sono supportati.
   - Backup di sicurezza gratuito: se Rufus non offre la scelta e procede in modalità ISO,
     è il percorso corretto. Leggi https://laptopjudge.com/rufus-linux-usb-boot-failure/
     e https://www.itechguides.com/how-to-create-a-bootable-usb-drive-with-rufus/ per la
     spiegazione delle due modalità.
5. **Partition scheme**: **GPT**. **Target system**: **UEFI (non CSM)**.
6. **File system**: **FAT32** (default) · Format options: Quick Format ON · default il resto.
7. **START** → conferma l'avviso di sovrascrittura.
8. Al termine: espelli e reinserisci la chiavetta; deve apparire come unità FAT32 con etichetta
   `TINYMACOS` e le cartelle `EFI`, `System`, `Recovery`, più i file `EFI.IMG` e `Install.esp`
   nel root.

> **Alternativa non Windows**: burn con `dd` **non è raccomandato** (vedi punto 4). Su Linux
> usa in alternativa `genisoimage`/`xorriso` per ricreare la stessa estrazione — ma la via
> consigliata è ricreare il volume FAT32 con gli stessi passi di Rufus ISO mode.

## 4. Firmware (prima di avviare)

Entra nel setup del firmware (F2/Del/Esc/F10/F12 — varia per produttore) e imposta:

- [ ] **UEFI mode** (mai Legacy/CSM/BIOS)
- [ ] **Secure Boot: Disabled** (o «Other OS», se esiste un'opzione del genere)
- [ ] **Fast Boot: Disabled** (almeno per i primi test)
- [ ] Salva ed esci; alla riaccensione seleziona la chiavetta dal menu di avvio una
      tantum (tasto tipico: **F12** su Dell/ASUS/Lenovo/HP, **Esc** su Acer/HP, **F9/F10** su
      altri — guarda la schermata iniziale del PC).

Se l'immagine non appare tra i dispositivi avviabili:

- ricontrolla Secure Boot (il caricatore non è firmato → rifiutato);
- ricontrolla CSM (questa ISO NON avvia in Legacy);
- reinserisci la chiavetta in una porta USB diretta (non un hub alimentato).

## 5. Test H1 — «Avvia da USB con Rufus, va dritto in Recupero»

Precondizione: ISO retail bruciata in modalità ISO (sezione 3).

**Passi**: accendi il PC con la chiavetta inserita e selezionala come boot.

**Atteso (PASS):**

- Dopo il logo "mela" con la barra di avanzamento compare la schermata
  **«Crea un account»** (sessione live, avvio normale).

**FAIL se:**

- entri direttamente in **«Recupero tinymacOS»** senza averlo chiesto;
- lo schermo resta nero o fermo sulla mela senza avanzare;
- l'immagine nemmeno appare tra i dispositivi avviabili.

Se H1 riappare: foto della schermata + modalità di scrittura Rufus usata + riferimento a
questo documento (§3, punto 4). Il payload live in radice ISO/FAT è il fix di questa
regressione; se ricompare il sintomo, la tua copia della ISO potrebbe essere una build
precedente al fix (03:51 del 20 nov 2026).

## 6. Test H2 — «Il tasto R non funziona per la Recovery»

**Passi**: dalla stessa chiavetta (o dopo l'installazione, dal disco interno), accendi e
**all'inizio del caricamento premi e tieni premuto R**, oppure ripetilo ogni secondo per la
dura della mela/barra.

**Atteso (PASS):** la barra sotto il logo mostra il suggerimento «Premi R per Recupero»;
i tasti R/F12 vengono accettati **in qualsiasi momento** del caricamento e compare una
finestra **«Recupero tinymacOS»** (barra titolo chiara, semaforo rosso/giallo/verde a
sinistra, titolo centrato). La regola da ricordare è semplice: «R = Recupero».

**FAIL se:**

- nessun R/F12 apre la Recovery (continua direttamente al desktop/installatore);
- si blocca su una schermata vuota.

Nota: **F12 da sola** può essere il boot menu del firmware — non è una scorciatoia del
nostro sistema; in tinymacOS è «R» all'avvio.

## 7. Installazione su disco interno (opzionale, distruttiva)

Se il superamento di H1/H2 è confermato e hai il backup:

1. Dalla sessione live, avvia **«Installa tinymacOS»** (dal Dock o Finder).
2. Segui i pannelli (selezione volume → accordo → conferma «Controlla» → installazione —
   richiede il segno di spunta esplicito su «Controlla», altrimenti il pulsante resta
   disabilitato).
3. Al termine: rimuovi la chiavetta, riavvia dal disco interno.
4. Primo avvio: crea un account. Apri Terminale (`sw_vers` → **tinymacOS 2.4.0 (21B1)**).
5. Spegni (Terminale: `off` oppure menu  → Spegni). Riaccendi dal disco:
   schermata di blocco → password → Finder popolato, account e documenti presenti.

**FAIL**: spegnimento forzato, schermata di blocco che non si apre, kernel panic
(riquadro scuro con codice).

## 8. Test H3 — `csrutil disable` in Recovery dà errore NVRAM

Assicurati di essere nella **shell di Recupero** (non nel Terminale di sistema) — la
Recovery accetta `csrutil`, il sistema installato no:

1. Entra in Recupero (H2). Scegli **Terminale** (icona terminale, quarta voce). Si apre una
   finestra normale con sfondo scuro (vedi §9 per la resa visiva attesa).
2. Digita:
   ```
   help
   sw_vers
   csrutil status
   csrutil disable
   csrutil status
   reboot
   ```
3. **Atteso (PASS)**:
   - `sw_vers` → `tinymacOS Recovery 2.4.0 (21B1)`
   - `csrutil status` prima → **enabled** (SIP attiva)
   - `csrutil disable` → nessun errore di NVRAM. Le scritture sulle variabili di Recovery
     avvengono su GUID di produttore (non più EFI globale): il firmware reale può essere
     severo con nomi sconosciuti, quindi questo è il test che conta.
   - `csrutil status` poi → **disabled**
   - dopo `reboot`: se rientri in Recupero (H2), `csrutil status` resta **disabled**
     (persistenza NVRAM)
4. Verifica anche il rifiutato atteso: in Terminale di **sistema** (non Recupero):
   `csrutil disable` → rifiuto (SIP si tocca solo dalla shell di Recupero).

**FAIL se:** `csrutil disable` stampa di nuovo un errore di scrittura NVRAM o
`parameter not valid`.

## 9. Test U1 / U3 — finestre della Recovery "normali", non piccole o ovali

Sempre dalla stessa Recovery, apri ogni voce e controlla **visivamente**:

- [ ] Ogni pagina ha: **barra titolo chiara** 40 px, **semaforo** (rosso/giallo/verde) in
      alto a **sinistra**, **titolo centrato** («Recupero tinymacOS», «Seleziona un disco»,
      «Utility Disco», «Terminale», «Conferma l'operazione», «Spegnere il computer?»),
      **card arrotondata ~1012×560** su 1280×800, riga di 1 px sotto la barra.
- [ ] **Terminale** (U1): il corpo non è una finestra piccolina né un pannello ovale:
      riempie tutta la card, sfondo **scuro**, **16+ righe di testo** in scrollback,
      prompt `recovery %` e un pulsante **«Indietro»** in basso a sinistra.
- [ ] Pagine operative hanno **«Indietro»** in basso a sinistra e l'azione in basso a destra
      (**«Continua»**, **«Conferma»**, **«Spegni»** o **«Fine»** a seconda del punto).
- [ ] Pagina iniziale («Scegli come proseguire»): 5 voci con icone e una riga di
      descrizione ciascuna:
      1. Ripristina da una copia locale
      2. Reinstalla tinymacOS
      3. Utility Disco
      4. Terminale
      5. Disco di avvio
- [ ] Barra di menu in alto (nera, con  sul sinistra, «Recupero tinymacOS» e «Spegni…» a
      destra) — non compare alcuna finestra di tipo legacy.

**FAIL se:** Terminale o le altre pagine tornano a essere una finestrella flottante/ovale,
senza semaforo o senza barra titolo, o con i pulsanti fuori dalla card.

## 10. Test U2 — applicazioni separate

Niente da provare a mano: in questo giro le 15 applicazioni desktop (14 + NetSurf) sono
bundle Mach-O separati con CFBundleIdentifier unici, verificati offline con
`python3 tools/app_audit.py` (sul volume live: 15/15 OK; sul volume disco: 14/14 OK). Gli
strumenti Unix in `/bin`, `/usr`, `/sbin` sono multi-call busybox-style **by design** e non
sono applicazioni GUI.

A prova di curiosità, dall'installato apri ciascun bundle da Finder → anteprima binaria
(non deve cambiare il comportamento al variare delle altre app).

## 11. Zona opaca — operazioni distruttive (solo se ti serve)

Queste cancellano o rimpiazzano dati. Da eseguire solo con backup completo e consapevolezza.
Non fanno parte del "PASS" minimo di questo test.

- **Ripristina da una copia locale** (menu voce 1): richiede una copia creata prima dal
  sistema (Tempo Machine/timer o Terminale di Recupero seguendo la procedura salvata).
- **Inizializza i dati utente…**: cancella account/documenti/backup locale. Richiede la
  parola «INIZIALIZZA» nel campo. Irreversibile.
- **Reinstalla tinymacOS**: sostituisce `/System`, NON tocca `/Users` (i documenti restano).

Se li provi, annota il prima/dopo con foto e prepara il rollback fisico (backup della
partizione).

## 12. Cosa mandarmi dopo il test

Compila (anche in chat, anche a voce — non serve formato):

```
Macchina: produttore / modello / CPU / RAM / GPU
Firmware: produttore, versione UEFI, data
Secure Boot: on/off · CSM: on/off · Modalità avvio: UEFI (ok)
Chiavetta: marca / capacità / porta (diretta o hub)
ISO: SHA-256 usato:
Rufus: versione, modalità scelta (ISO/DD), GPT/UEFI confermati
H1 (avvio normale, non Recupero): PASS / FAIL — foto:
H2 (R apre Recupero): PASS / FAIL — foto:
H3 (csrutil disable): PASS / FAIL — testo esatto dell'errore o del successo:
U1/U3 (look Recovery): PASS / FAIL — foto della home e del Terminale:
U2 (nessun controllo manuale): —
Installazione completa (se provata): PASS / FAIL — a che punto:
Orario dei test (per correlare eventuali log):
Note libere:
```

## 13. Matrice di risoluzione rapida

| Sintomo | Causa probabile | Cosa fare |
|---|---|---|
| Chiavetta non appare tra i dispositivi di boot | Secure Boot on / CSM on / mediatura UEFI | spegni Secure Boot; disattiva CSM; altra porta USB |
| Compare immagine vuota in terminalino di GRUB/quesito "missing bootloader" | Scrittura in DD mode | rifai con **ISO Image mode** (§3) |
| Avvia direttamente in **Recupero** | Regressione H1 | foto + riferimento a §5; NON procedere con l'installazione |
| R non apre Recupero | H2 | insisti tenendo R durante tutta la mela; se persiste, report |
| csrutil disable → errore NVRAM | H3 | testo esatto dell'errore; NON cambiare altro in NVRAM |
| Schermata nera dopo la mela | Frequenza/EDID del display | scollega schermi esterni, riprova nativo 1280×800 o stretch |
| Niente rete/audio/touchpad sul desktop live | Driver assenti su questo hw | atteso su macchine senza e1000/dispositivi supportati; non è un panic |
| "Kernel panic" con testo | BUG serio | **foto integrale**; NON riavviare più volte senza dirmelo |

## 14. Cosa questa immagine NON promette

- **Secure Boot**: non supportato (caricatore e binari non firmati).
- **BIOS/CSM/Legacy**: non supportato.
- **Rete sul desktop**: la 2.4.0 in QEMU ha e1000 (driver en0). Su hw reale può essere
  assente qualunque scheda; il Browser NetSurf non raggiungerà Internet senza driver
  nativo — nessun crash, semplicemente nessuna rete.
- **Wi-Fi**: radio simulata; nessun hardware Wi-Fi reale è supportato in 2.4.0.- **Schermo oltre 1280×800**: la GOP nativa è 1280×800; su pannelli diversi l'immagine è
  centrata/estesa dal firmware, non scalata dall'OS.

---

*Fonti esterne consultate per le modalità Rufus:*
[rufus.ie](https://rufus.ie) · [Rufus ISO vs DD — guida](https://laptopjudge.com/rufus-linux-usb-boot-failure/) · [ISO Image vs DD Image mode](https://www.itechguides.com/how-to-create-a-bootable-usb-drive-with-rufus/)

*Riferimenti interni: `docs/QUICKSTART.txt`, `tools/iso.sh`, `recovery/main.c`,
`storage/maintenance.c`, `boot/loader.c`, `boot/splash.h`, `boot/recoveryboot.h`,
`tools/app_audit.py`, piano `/home/user/piano-levante-2.4.0.md`.*
