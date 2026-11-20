# tinymacOS 2.4.5 Levante — manutenzione input

**Build 21B6 · x86-64 · BIOS e UEFI · senza Safari/WebKit**

Questa manutenzione anticipa il lavoro browser su richiesta dell’utente, per il mouse USB cablato che funziona in Recovery ma resta fermo al login live, e per il ritardo del cursore. Non è una ISO diagnostica: contiene il desktop, le app, l’installer e il Recupero offline normali. La diagnostica F11 è una funzione del login, non sostituisce le app.

## Modifiche

- **Cursore al login:** i movimenti ridisegnano e presentano i due rettangoli del vecchio/nuovo cursore, come sulla scrivania, invece di ridisegnare tutto il login per ogni report. Hover, clic, campi, orologio e messaggi continuano a invalidare la scena quando necessario.
- **Coda input:** accorpamento dei movimenti consecutivi mantenendo la somma relativa o l’ultima posizione assoluta. Le transizioni di pressione/rilascio restano eventi separati e non vengono spostate dai movimenti successivi. Tasti e rotella interrompono l’accorpamento. Contatore esplicito degli eventi persi per coda piena.
- **USB xHCI:** recupero differito e limitato per gli errori di endpoint in halt riconosciuti: reset, clear-halt, nuovo dequeue e riarmo. Un timeout non autorizza il riciclo di DMA di proprietà incerta. Gli errori persistenti non provocano tentativi infiniti.
- **Protocollo mouse:** se un mouse dichiara Boot ma rifiuta SET_PROTOCOL(Boot), il driver recupera EP0, legge e valida il report descriptor e usa il decoder Report solo dopo conferma del protocollo. Gestisce anche errori riportati sui TRB Setup/Data, non soltanto Status. Nessuna interpretazione alla cieca di report-ID o coordinate a 16 bit come tre byte Boot.
- **DMA:** buffer report e controllo allineati a 1024 byte per evitare trasferimenti Normal TRB che attraversano un confine di 64 KiB. Maggiore budget limitato di drenaggio degli eventi xHCI.
- **F11 prima dell’accesso**, oppure pulsante «Diagnostica USB»: controller rilevati/attivi, puntatori configurati, report, eventi accorpati/persi ed errori. Nessun contenuto dei tasti, password o indirizzo DMA viene mostrato.
- Identità 2.4.5/21B6 aggiornata in metadati sistema e app, installer, Impostazioni, Info, comandi, Recovery e documento iniziale. Mantenuti i percorsi BIOS/OpenDuet/OpenCore e UEFI dell’installer OC2.

## Limiti importanti

**Il difetto sul PC fisico non è ancora confermato risolto.** Le correzioni sono implementate; i test automatici usano QEMU e l’iniezione controllata di errori su host. La causa esatta sul computer dell’utente non è stata identificata con una traccia hardware.

Recovery usa i driver firmware, il desktop usa il driver nativo: il funzionamento del primo non prova quello del secondo. Il driver nativo supporta HID su porte radice **xHCI**, fino a quattro controller. Hub USB ed EHCI/OHCI/UHCI non sono implementati; i controller legacy vengono contati ma non resi supportati da questa patch. I test non provano hotplug, tutti i mouse HID, né latenza fisica universale. Il parser Report resta limitato ai layout supportati.

L’ISO non è USB-DD ibrida. Per la prova su chiavetta mantenere la modalità ISO già prevista; non confondere il percorso ottico BIOS validato con una certificazione di tutte le chiavette/firmware. Secure Boot non supportato. Non reinstallare sopra dati importanti per provare il mouse: avvia prima la live.

## Prova richiesta sul PC

1. Avvia **questa nuova ISO** in UEFI e verifica movimento, clic e fluidità già al login, prima di creare l’account.
2. Ripeti con il percorso BIOS/CSM se disponibile sul tuo PC.
3. Se resta fermo, premi **F11** sulla tastiera e fotografa il pannello dopo aver mosso il mouse per qualche secondo. Indica modello PC/scheda madre e porta USB usata, compreso l’eventuale hub.

Safari/WebKit reale resta il prossimo lavoro, da riprendere in grandi blocchi: questa release non dichiara supporto HTML/CSS/JavaScript/HTTPS del futuro browser.

## File e riproduzione

- ISO: `tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso`
- Dimensione: **221489152 byte**
- SHA-256: `8988c9ce7627d191edc2225ea36ee6f834a49a89bceeabe39ed9c65874a79da7`
- Checksum: `SHA256SUMS.txt`
- Ricostruzione pulita indipendente: stessa ISO byte per byte; manifest sorgenti in `source-hashes-before-rebuild.json`. Il nuovo hash include anche l’allineamento del buffer dei descrittori/control Data Stage.

Dalla directory `tinymacOS`, con la toolchain installata:

```sh
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc245-final bash build.sh
TOOLROOT=/var/tmp/tmc/tools/root python3 tools/dualboot-iso.py \
 /var/tmp/tmc245-final/tinymacOS-2.4.5-x86_64.iso \
 /var/tmp/tmc245-final/tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso
bash tests/test-input-maintenance.sh
```

La conversione dual-boot richiede un percorso di output nuovo. Build e dischi QA sono file esterni usa-e-getta, mai dispositivi fisici.

## Selezione Recovery ottico BIOS: limite emerso nel collaudo

I tentativi automatici di selezionare Recovery con R sul percorso ottico BIOS non hanno raggiunto il menu: la VM è arrivata al setup nativo e le R sono finite nel campo nome. Questo percorso **non viene classificato PASS** né nascosto nei test mouse. La suite BIOS del puntatore esclude esplicitamente questa selezione (`--skip-recovery`). Il ripristino locale BIOS da disco con kernel danneggiato ha un test distinto, così come R/Recovery ottico UEFI. Questo limite riguarda la selezione firmware prima del kernel; non è una prova di guasto del mouse al login.

## Collaudo finale completato

| Verifica | Esito |
|---|---|
| Identità ISO/app/Recovery 2.4.5 / 21B6, etichette disegnate, assenza vecchia identità 2.4.4 nei volumi sistema | PASS |
| Live UEFI, mouse relativo e tablet: sprite, focus/clic account, desktop, trascinamento, menu/rotella e Recovery ottico | 12 verifiche PASS |
| Live BIOS, mouse relativo e tablet: sprite, focus/clic account, desktop, trascinamento e menu/rotella | 10 verifiche PASS; selezione Recovery ottico esclusa come sopra |
| Login installato senza ISO, mouse relativo su BIOS e UEFI: sprite bloccato, campo password/freccia cliccati, desktop e documento persistente | PASS |
| Installer grafico BIOS, ISO rimossa, riavvio BIOS, preferenza verbose, stesso disco avviato UEFI | PASS |
| KernelCache danneggiato su clone usa-e-getta: rifiuto, Recovery locale BIOS, ripristino sigillato e riavvio | PASS; documenti e ponte OpenDuet invariati |
| Host ASan/UBSan: coda 10000 report, barriere clic/ruota, errori trasferimento, recupero EP0, vera enumerazione con MMIO emulato, fallback e rifiuti | PASS |
| Regressioni kernel/runtime native + contratti host | **29/29 PASS** |
| Kernel ELF di produzione confrontato con i cinque build probe network/VM/sync/TLS/thread | Identico SHA-256 |
| Ricostruzione pulita indipendente dell’intera ISO | Identica byte per byte |

Receipt principale: `final-qa.json`; dettaglio kernel: `kernel-regression-result.json` e `kernel-production-match.json`. I probe usano volumi comando diagnostici separati; **non sono l’ISO pubblicata** e non validano un browser. Screenshot selezionati in `evidence/`, con manifest dei ritagli e hash originali. La suite completa conserva anche log e ricevute in `../webkit-tmc/test-results/`.

Durante l’adattamento del collaudo sono emerse assunzioni errate nell’harness: socket QMP creato prima di listen, default mouse diverso nel launcher BIOS, PS/2 inattivo presente nel modello QEMU BIOS, assenza del logo nel percorso verbose. La corsa socket è stata corretta, i profili USB resi espliciti e la presenza PS/2 inattiva distinta dal puntatore USB attivo. La selezione ottica Recovery BIOS è rimasta non superata anche con R ripetuta: non viene trasformata in un successo. Tutte le righe PASS sopra provengono dal candidato finale con SHA indicato, dopo l’ultimo allineamento DMA.
