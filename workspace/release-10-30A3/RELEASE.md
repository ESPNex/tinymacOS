# tinymacOS 10 · Tramontana — manutenzione 30A3

**Versione 10**, build tecnica **30A3**. ISO: `tinymacOS-10-x86_64.iso`. Release v10 normale/Latest; le quattro precedenti rimangono Pre-release. Repository e download privati, accessibili con account GitHub autorizzato.

## Recovery dal supporto ottico BIOS

- Corretto il firmware OpenDuet del percorso ottico: l'immagine Block I/O originale non includeva lo stack UHCI/EHCI/UsbBus necessario a enumerare l'input USB. Il build integra tre moduli originali, immutati, dalla stessa distribuzione OpenCore 1.0.8 già fissata nel repository. Conserva tutti i 29 moduli preesistenti e il percorso BIOS Block I/O; non modifica i file vendor né il firmware usato dal disco installato.
- Derivazione deterministica e verificata: controlli su digest dei moduli, contenitore compresso, limiti, checksum e tabelle di avvio. Il firmware viene estratto anche dalla ISO finale e confrontato byte per byte con quello derivato.
- Recovery richiede al dispositivo tastiera HID Boot un aggiornamento periodico dello stato ogni 32 ms tramite `SET_IDLE`. I report identici non producono caratteri duplicati; il rilascio interrompe la ripetizione locale. Un dispositivo che rifiuta la richiesta opzionale non viene escluso. La mappa rimane esclusivamente italiana.
- Prova effettiva in QEMU: selezione con **R e F12**, tastiera su **EHCI e xHCI**, sia **mouse USB relativo** sia **tablet USB assoluto**. In ciascuna delle otto combinazioni si entra nel menu, si aprono Terminale e Continua tramite clic e si legge la versione con `sw_vers`. Nessun disco installato o Recovery esterno collegato. Il vecchio supporto 30A2 riproduce invece il mancato ingresso nel menu.

La selezione va eseguita **durante la mela di tinymacOS**, dopo il menu OpenCore: un tasto premuto nel menu OpenCore può fermarne il conto alla rovescia.

## Funzionalità e manutenzione conservate

- Versione interna **10**, senza `.0`, in Informazioni, Impostazioni, installer, Terminale, Recovery e metadati.
- Artwork importato solo per le applicazioni. Filesystem, Libreria, categorie di Impostazioni e Centro di Controllo procedurali; installer Tramontana procedurale originale. **In Recovery tutto procedurale tranne Terminale.**
- Teal verde distinto da Tramontana blu ardesia, in modalità chiara e scura. Associazioni **Blu → Prisma, Azzurro → Brezza, Tramontana → Tramontana**; nessuna modifica a Menta.
- Sfondo Tramontana non procedurale, wordmark macOS con peso autentico 600 e tiny invariato, mela allineata, titolo ridotto in Recovery, Mission Control senza alone.
- Tastiera fisica italiana end-to-end; nessuna diagnostica F11 al login. Il collegamento all'installer sulla scrivania e F11 nel live rimangono disponibili.
- Conservate le correzioni generali di routing USB2 con EHCI e negoziazione HID verificata con `GET_PROTOCOL`, senza eccezioni dedicate a una singola motherboard.

## Collaudo

I risultati sono legati al digest della ISO finale: due build indipendenti identiche; risorse effettive del filesystem e Recovery; dieci combinazioni tema/modalità; 17 profili input live e 10 installati senza ISO; assi X/Y indipendenti e controllo negativo; tastiera italiana in applicazioni e Recovery; installer, avvio incrociato BIOS/UEFI e ripristino locale sigillato; Recovery ottico UEFI e BIOS; Mission Control e allineamento menu; contratti host con ASAN/UBSAN; **29 regressioni kernel** con kernel delle sonde identico alla produzione.

Ricevute: `final-qa.json`, `artifact.json`, `supplementary-selector-result.json`, `bios-input-artifact-result.json` e `SHA256SUMS.txt`. La verifica dei download pubblicati è registrata separatamente in `github-release-verification.json`. Immagini diagnostiche e sonde non sono distribuite.

## Limiti espliciti

**Sul PC fisico ASRock Q1900M/J1900 l'ultimo riscontro rimane mouse fermo sia nel live sia in Recovery. Non è stata effettuata una nuova prova fisica e la soluzione su quella macchina NON è confermata.** I PASS QEMU verificano le configurazioni emulate, non dimostrano la risoluzione del problema hardware, né certificano la selezione firmware con una tastiera PS/2 fisica.

Sistema sperimentale con kernel proprio, non Linux/XNU e non macOS Apple. Safari/WebKit reale è sospeso e **non incluso**: le sonde runtime non sono un browser. Nessun supporto dichiarato per OHCI, hotplug generale, hub SuperSpeed o HID universale. ISO ottica BIOS/OpenDuet/OpenCore e UEFI; non immagine USB avviabile in modalità DD.
