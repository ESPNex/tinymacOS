# tinymacOS 2.4.2 · Levante 21B3 — manutenzione USB e password

[**Scarica la nuova ISO da Google Drive**](https://drive.google.com/file/d/1W68tT8mUeS964heba5SWY4jpwjBbfxC-/view)

Eccezione di manutenzione esplicitamente richiesta prima del fix SMP e del resto del port. **Non contiene Safari/WebKit funzionante.** Non dichiarata risolta la compatibilità con il mouse fisico dell'utente, non ancora provato.

## Artefatto verificato

- File: `tinymacOS-2.4.2-x86_64.iso`
- Build: **21B3**
- Byte: **118.243.328**
- SHA-256: `36654e5a92e8c60d5f012003f2b7f6cd19fbab8cc5bef060773816f7226dcbd0`
- MD5 Drive: `7212133d10a9d2012a8df94adffa964b`
- Upload completato con stato `final`; dimensione e SHA-256 riletti da Drive e corrispondenti al file locale.
- Permessi non modificati; il link utilizza gli accessi correnti del tuo Drive.
- Ricompilazione finale con sorgenti salvati: stesso digest della ISO usata nei test di manutenzione.

## Correzioni

### USB: una causa riprodotta e corretta

La 2.4.1 selezionava solo il primo controller xHCI PCI e la prima interfaccia HID utile di ogni dispositivo. Ora:

- enumerazione fino a **4 controller xHCI**, ognuno con proprie code, event ring, DCBAA e dispositivi;
- fino a **4 interfacce HID** per dispositivo, con endpoint/buffer/layout/protocollo indipendenti; un mouse composito non viene ignorato soltanto perché la prima interfaccia è una tastiera;
- endpoint interrupt-IN fino a 1024 byte, layout pointer decodificati entro il limite dichiarato del parser;
- i completamenti input incontrati durante attese di controllo, ad esempio gli aggiornamenti LED tastiera, vengono gestiti invece di scartati;
- mantenuti boot keyboard, mouse USB relativo e tablet assoluto; rotella dei report boot a quattro byte inoltrata quando presente.

**Prova negativa/positiva:** con identica configurazione QEMU, tastiera sul primo controller e mouse sul secondo, la 2.4.1 NON sposta il cursore nel punto richiesto; la 2.4.2 lo sposta e lo sprite coincide pixel per pixel con quello distribuito. Questo dimostra la correzione di quel caso, **non identifica con certezza il problema del computer dell'utente**.

Il parser di configurazione composita è testato su host, ma non è stato eseguito un test con un vero mouse composito fisico.

### Password: disegno senza righe bianche di contorno

Rimossi completamente bordo e riflesso bianchi dai campi account/password; focus indicato dal riempimento azzurro scuro. Nessun caret nei campi segreti. Puntini più distanziati, colore azzurro tenue anziché bianco, massimo 16 visibili anche con password più lunga.

Screenshot di setup con 0/1/16/64 caratteri, più fasi temporali, poi creazione account, blocco, password a 64 caratteri e riautenticazione. I campioni nuovi del contenuto password non hanno una lunga banda di pixel quasi bianchi; la freccia di invio è esclusa dalla misura del campo bloccato.

La riga segnalata sul PC non è stata riprodotta con certezza in QEMU: **non viene dichiarata dimostrata la causa originaria né garantito il risultato sul display fisico**. Nel confronto si vede esattamente il cambiamento del disegno.

## Test superati

| Prova | Esito |
|---|---|
| Parser HID: tablet, mouse relativo/report ID/16 bit, troncamenti | PASS · ASan/UBSan |
| Configurazione composita keyboard+mouse+report, endpoint indipendenti, alternate setting e limiti | PASS · ASan/UBSan |
| Controller standard: tablet e mouse separatamente, setup, desktop, drag, menu, Recovery | PASS |
| Primo controller vuoto, tastiera e puntatore sul secondo: tablet e mouse | PASS |
| Tastiera sul primo, mouse sul secondo: setup, desktop, drag e Recovery | PASS |
| Confronto mouse 2.4.1/2.4.2 sul secondo controller | FAIL atteso prima / PASS dopo |
| Password lunga, setup, blocco e riautenticazione | PASS · screenshot salvati |
| Avvio solo USB GPT/FAT32, ISO estratta, nessun CD, 2 vCPU | PASS · framebuffer setup verificato |
| Installer reale su SATA sacrificabile, boot senza ISO, accesso e sw_vers 2.4.2 | PASS |
| ESP e Recovery installate confrontate byte per byte con il supporto | PASS |
| Audit cataloghi/kernel/DMG/digest, SHA-256/PBKDF2 contro hashlib | PASS |
| Suite backend/runtime completa dopo le modifiche, incluse VM e rete/file guest | **15/15 PASS** |

Le prove normali del sistema con **2 vCPU** passano. Il lavoro ancora aperto riguarda la **nuova API VM S_VM**: le sue modifiche rimangono limitate a una CPU e rifiutate con più CPU. L'ISO include questa API sperimentale già presente nell'albero di sviluppo; non è stato eseguito né dichiarato il fix SMP richiesto per il futuro browser.

Nessun test JSC/WebCore/HTTPS è stato aggiunto: il motore non è ancora compilato.

## Come preparare la chiavetta

**Rufus cancella i dati della destinazione. Usare una chiavetta sacrificabile, non un disco con dati importanti.**

1. Seleziona `tinymacOS-2.4.2-x86_64.iso` e controlla che la destinazione sia la chiavetta corretta.
2. **GPT**, **UEFI senza CSM**, **FAT32**.
3. Se richiesto, scegli modalità **ISO**, non DD: la ISO non è un'immagine disco hybrid.
4. Disattiva **Secure Boot** e avvia la nuova chiavetta dal menu UEFI.
5. Per verificare davvero questa release, scegli la chiavetta: un sistema 2.4.1 già installato non viene aggiornato automaticamente.

Non ho eseguito Rufus su Windows né provato hardware fisico. Il test riproduce l'estrazione della ISO su un nuovo supporto GPT/FAT32 e il suo boot da USB emulata, in sola lettura e senza CD.

Se il mouse non va ancora: prova una porta **USB 3.x collegata a xHCI**, direttamente, senza hub/dock. L'essere USB non basta: **EHCI/OHCI/UHCI non sono implementati**, né hub/hotplug generali; restano limiti dei descrittori HID (più report ID, layout complessi) e dei dispositivi. Serviranno modello del PC, marca/modello del mouse, VID:PID e idealmente una foto della riga per individuare il caso reale, senza presumere un'altra causa.

## Sorgenti e ripetibilità

- OS completo estratto: `/home/user/tinymacOS/`.
- Backend/port: `/home/user/workspace/webkit-tmc/`.
- Copie dei file cambiati rispetto allo stato iniziale del lavoro 2.4.2: `sources/`; digest in `source-sync.json`.
- Harness aggiuntivi: `qa/`; prove e screenshot: `evidence/`.
- Tutti i sorgenti compilati erano salvati sotto /home/user prima della compilazione. Nessuna ISO/ZIP nel workspace; build e media sono in /var/tmp e si ricostruiscono dai sorgenti.
- Budget locale controllato: non equivale a conferma dello snapshot della piattaforma.

```sh
cd /home/user/tinymacOS
python3 tools/local-tools.py /var/tmp/tmc/tools/root
chmod u+x build.sh run.sh tools/run-qemu.sh
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-maintenance-242 bash build.sh
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-maintenance-242 \
  python3 tests/riviera_pointer.py /var/tmp/qa-242-standard --profile both
TMC_USB_LAYOUT=second TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-maintenance-242 \
  python3 tests/riviera_pointer.py /var/tmp/qa-242-second --profile both
TMC_USB_LAYOUT=split TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-maintenance-242 \
  python3 tests/riviera_pointer.py /var/tmp/qa-242-split --profile mouse
python3 /home/user/workspace/webkit-tmc/tools/run-tests.py --guest
```

Le immagini diagnostiche generate dalla suite WebKit sostituiscono i comandi con probe: **non sono la ISO di manutenzione caricata e non vanno distribuite**.
