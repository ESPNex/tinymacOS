# tinymacOS 2.4.5 Levante — revisione 21B8

**Sostituzione della stessa release v2.4.5, dopo il mouse ancora fermo sulla 21B7.**

**DEPLOY COMPLETATO E VERIFICATO:** release ID `453129451` invariato; nuova ISO riscaricata sotto il nome definitivo e confrontata con il checksum. Tag `v2.4.5` al commit sorgenti `f4e499bd2dcea87bbf2eae86e81e714b1b7c0e8b`; vecchi sorgenti conservati come `v2.4.5-21B7`. Ricevuta `github-release-verification.json`.

L’utente ha confermato che la tastiera ora funziona, ma il mouse USB no, indicando una **ASRock Q1900M**. Non consideriamo i PASS QEMU precedenti una conferma su quel PC. Recovery usa il firmware e non prova la correttezza del driver nativo.

## Mouse USB: lacune corrette

Il driver xHCI precedente cercava solo mouse e tastiere sulle porte radice: **un hub veniva scartato**, senza enumerare i figli. Una presa fisica può essere collegata tramite un hub interno anche senza un hub esterno sul cavo. Inoltre il codice non gestiva il mux Intel delle porte condivise fra EHCI e xHCI.

Questa revisione aggiunge:

- **Enumerazione nativa degli hub USB2 su xHCI**, alimentazione delle porte, attesa power-good, debounce, reset con polling limitato e reset-recovery. Hub annidati fino a cinque livelli, entro i limiti di capacità statici.
- Slot distinti riservati prima di enumerare i figli: il padre non viene sovrascritto. Route string, porta radice e transaction translator del più vicino hub high-speed propagati ai figli low/full-speed.
- Contesto hub con numero porte e TT think-time; uso del comando Configure Endpoint per xHCI successivi a 0.95, Evaluate Context per 0.95. I bit riservati del contesto dei figli restano zero. Gli hub multi-TT capaci vengono usati nell’alternativa zero single-TT.
- **Instradamento delle porte sui controller Intel switchable riconosciuti**, incluso Bay Trail `8086:0f35`: prima le terminazioni USB3, poi le linee USB2, usando le maschere PCI pubblicate dal controller e conservando i bit preesistenti. Solo dopo l’avvio di un xHC funzionante e prima di enumerare EHCI. Nessuna scrittura di questa quirk su QEMU, altri vendor o ID sconosciuti.
- Attese xHCI legate a millisecondi, non soltanto al numero di iterazioni CPU; breve attesa dopo il reset del controller, power-on delle porte quando previsto da PPC e attesa di stabilizzazione dopo la commutazione.
- Migliorato anche il reset dei figli di hub EHCI: debounce, verifica stato e polling limitato, invece di una singola lettura dopo un’attesa fissa.
- Conservati PS/2 tastiera/mouse, EHCI e companion UHCI già introdotti. **Nessun driver basato sul nome ASRock:** il supporto riguarda controller e protocolli; la commutazione Intel è una quirk PCI circoscritta.

La vecchia 21B7 è stata riprovata con mouse USB forzato dietro un hub: **cursore fermo**, ma accesso da tastiera PS/2 riuscito (`old-hub-reproduction.json`). Senza selezione esplicita, QEMU riattivava il mouse PS/2 e nascondeva il difetto. Il harness ora forza il dispositivo relativo richiesto e verifica la selezione iniziale/finale; nei profili tastiera USB **i8042 è disabilitato**, quindi nessun input può ripiegare su PS/2. Tutti i receipt input precedenti a questo irrigidimento sono stati esclusi dal gate finale.

La matrice nuova include mouse dietro hub xHCI e due hub in cascata, senza una tastiera USB che possa nascondere il percorso PS/2. È aggiunto anche il profilo tastiera+mouse dietro lo stesso hub.

## Grafica

- **Alone del Dock dopo Mission Control:** la cache dei materiali includeva i pixel di sfondo attorno all’ombra, ma la sua chiave non includeva l’intensità dell’oscuramento. Muovendo il puntatore vicino al Dock durante Mission Control era possibile riutilizzare la cornice oscurata dopo l’uscita. Ora l’oscuramento viene aggiornato prima di calcolare la chiave della scena e ne fa parte.
- **Mela allineata al menu:** alzato il glifo nativo di due pixel. La misura dei pixel visibili della mela e della voce File dà lo stesso centro verticale, **14,5 px**.
- Il test sulla 21B7 riproduce **22.086 pixel esterni del Dock rimasti diversi** dopo l’uscita; sulla 21B8 **zero pixel diversi** nell’intera fascia verificata, in sei cicli F10/Escape, Dock in basso e a sinistra. Si usano framebuffer originali, non screenshot ricostruiti.
- La diagnostica F11 al login resta rimossa. Il normale F11 dopo l’accesso apre ancora l’installer nella live.

## Download

- Pagina: https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.5
- File: `tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso`
- Versione **2.4.5**, build **21B8**: riscaricare la ISO anche se il nome è uguale.
- Dimensione: **221509632 byte**
- SHA-256: `fd24fc89e28e4cbb20a05738e2ccfc29ad7597ddb742afe26ab85f1375cf47e2`
- Checksum: `SHA256SUMS.txt`

Il deploy è confermato solo dalla ricevuta `github-release-verification.json`, dopo un download sotto i nomi definitivi e una nuova verifica SHA-256. La build 21B7 viene archiviata come tag sorgenti `v2.4.5-21B7`; non viene creata un’altra release al suo posto.

## Limiti espliciti

**Non ancora verificato sul PC fisico dell’utente.** Il routing Intel e i contesti high-speed TT hanno contratti host con MMIO sostituito; non sono prove elettriche sul Bay Trail. QEMU verifica realmente il codice nativo guest con hub USB full-speed, non emula quella scheda madre né un hub high-speed con split EHCI. Il percorso EHCI split/TT resta da validare su hardware.

Nuovi hub xHCI: **USB2**, non hub SuperSpeed USB3. Capacità: quattro xHC, otto slot per xHC comprendenti gli hub, quattro interfacce HID per dispositivo; fino a quindici porte per hub e cinque livelli. Le porte oltre i limiti non sono dichiarate supportate. Parsing HID limitato ai layout gestiti. Il protocollo Boot resta usato per i mouse xHCI che lo accettano; non si dichiara supporto di tutti i report proprietari.

OHCI, hotplug/rimozione/riconnessione e classi USB general-purpose bulk/isochronous non sono implementati da questo stack input. Collegare mouse e tastiera prima dell’avvio. Supporto ottico BIOS/OpenDuet/OpenCore e UEFI, non immagine ibrida USB-DD; Secure Boot non supportato. La selezione Recovery ottico BIOS con R non è dichiarata risolta. Safari/WebKit non è incluso né simulato in questa manutenzione.

## Evidenze e riproduzione

- `artifact.json`, `final-qa.json`: identità del solo candidato finale e gate fail-closed.
- `input-*.json`, `installed-*.json`: topologia, firmware, SHA della ISO, movimento renderizzato al login, clic, password, trascinamento, rilascio, menu contestuale e tastiera nativa.
- `chrome-result.json`, `chrome-old-reproduction.json`: confronto pixel nativo prima/dopo Mission Control e allineamento del glifo.
- Contratti host ASan/UBSan: `host-maintenance.log`, `host-legacy.log`; distinti dalle prove guest.
- Installer BIOS, riavvio senza ISO/crossboot UEFI e ripristino sigillato da Recovery locale hanno ricevute separate.
- Suite kernel/runtime con 29 casi; il gate confronta i cinque kernel dei probe con il kernel distribuito, non solo l’esito dei comandi.
- Ricostruzione indipendente in `/var/tmp/tmc245-input3-repro`: ISO identica byte per byte.

Build dalla directory `tinymacOS`:

```sh
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc245-input3-final bash build.sh
TOOLROOT=/var/tmp/tmc/tools/root python3 tools/dualboot-iso.py \
 /var/tmp/tmc245-input3-final/tinymacOS-2.4.5-x86_64.iso \
 /var/tmp/tmc245-input3-final/tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso
```

Il file di destinazione della conversione deve essere nuovo. La matrice usa `run-media-qa.sh`; le regressioni `../webkit-tmc/tools/run-tests.py --guest`. Tutti i dischi sono file esterni usa-e-getta, mai dischi fisici. RAM guest 512 MiB, al massimo due guest simultanei.

### Riferimenti per l’indagine hardware

- Specifiche del produttore: https://www.asrock.com/mb/Intel/Q1900M/
- Bay Trail: porte EHCI/xHCI switchable e ID PCI: https://www.spinics.net/lists/linux-usb/msg86204.html
- Registri di mux Intel: https://github.com/torvalds/linux/blob/master/drivers/usb/host/pci-quirks.c
- Contesti hub xHCI e comandi per versione: https://github.com/torvalds/linux/blob/master/drivers/usb/host/xhci.c
- Esempio pubblico Q1900M con xHCI e hub Genesys: https://linux-hardware.org/?probe=9173d6e4c9&log=lsusb

I probe pubblici non identificano il mouse dell’utente e non dimostrano come sia cablata la sua specifica porta.

## Esito del gate finale — 21B8

**PASS: 27 profili input nativi, 197 verifiche raggruppate; kernel/runtime 29/29.** Il gate `collect_qa.py` è stato eseguito sul candidato finale, non sui receipt segregati del primo batch.

| Prova | Esito |
|---|---|
| Live UEFI: PS/2; EHCI; companion UHCI; xHCI root/hub/hub+tastiera/due hub; UHCI; tastiera EHCI/companion | 10/10 PASS |
| Live BIOS: PS/2; EHCI; companion; xHCI root/hub/hub+tastiera/due hub | 7/7 PASS |
| Installato senza ISO: PS/2, EHCI, companion, hub xHCI e hub annidati, su BIOS e UEFI | 10/10 PASS |
| Selezione esplicita puntatore e conferma trasporto a inizio/fine; profili tastiera USB senza i8042 | PASS |
| Cursore al login, password e clic, Finder drag/rilascio e clic destro, Terminale/build, rotella nei profili dichiarati, documento persistente | 197 verifiche raggruppate PASS |
| Installer grafico BIOS, ISO rimossa, riavvio BIOS e crossboot stesso disco UEFI | PASS |
| KernelCache alterato su clone, Recovery locale e ripristino sigillato preservando documenti e ponte BIOS | PASS |
| Metadati, bundle, app, comandi e Recovery 2.4.5/21B8 | PASS |
| Sei cicli Mission Control con Dock in basso/a sinistra, intere fasce identiche prima/dopo | PASS: zero pixel diversi |
| Mela/File, centro verticale del disegno visibile | PASS: 14,5 / 14,5 px |
| F11 diagnostica assente dal desktop distribuito | PASS |
| Contratti host ASan/UBSan PS/2, HID, xHCI hub/TT/route/command e quirk PCI | PASS, transport sostituito |
| Kernel/runtime: rete, VM, sincronizzazione, TLS/errno, thread/TSD e consumatore C++ | 29/29 PASS |
| Cinque build probe confrontati con ELF kernel distribuito | Identici |
| File dell’albero di build non-QA confrontati con snapshot candidato | 489 corrispondenti |
| Ricostruzione indipendente della ISO dualboot | Identica byte per byte |

SHA-256 del kernel ELF comune: `9c38f54fb5627713ca18c0e3eff9dcd6342809d8148cb4ab8884defc389015cf`. Questi test runtime non sono test browser e i volumi comando probe non sono distribuiti nella ISO. Le screenshot conservate sono ritagli lossless dei pixel originali, senza resize o ridisegno; manifest con coordinate e hash in `evidence-manifest.json`.
