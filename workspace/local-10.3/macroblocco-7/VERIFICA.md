# Macroblocco 7 — verifica locale

**Perimetro:** estensione autorizzata dall’utente al PDF, che termina al blocco 6. Cursori dal suo archivio, supporto nativo CUR/ANI, finestre e font proporzionale. Non è un nuovo rilascio pubblico né certificazione hardware.

## Implementazione

- Importate **17 risorse originali** (15 CUR, 2 ANI, 322.066 byte) in `/System/Library/Cursors/`, con manifest di provenienza e SHA-256. Stessi file effettivi nei volumi sigillati **live, installato e Recovery**.
- Decoder CUR con DIB 1/4/8/24/32 bit, AND/alpha/inversione, hotspot e selezione dimensione; ANI RIFF ACON con frame, sequenze e rate. Limiti 64 frame/256 passi, bounds verificati, rifiuto delle varianti non supportate. Nessuna conversione offline in sostituzione dell’animazione runtime. Riduci movimento ferma il primo frame.
- Contesti `C_CURSOR` nel protocollo grafico e notifiche locali enter/leave/update. Campo testo, pulsante, movimento, quattro orientamenti di resize, installazione occupata; risorse aggiuntive disponibili senza inventare capacità applicative.
- Accesso prima dell’autenticazione mediante la sola API delle risorse sigillate `S_ART`, **desktop-only e lista fissa**: nessun percorso fornito dal client. Gate ordinario dei file e autenticazione **identico byte per byte** al blocco 6, verificato dal contratto sorgente.
- Otto bordi/angoli di resize con ancora opposta e limiti; accorpamento di resize adiacenti nel kernel, un’ultima geometria per finestra/frame con retry dopo backpressure e rilascio. Movimento con riuso delle superfici, niente rigenerazione completa del vetro per ogni rapporto del mouse; ripristino backdrop definitivo al rilascio.
- TextEdit, Note e Memo usano gli avanzamenti reali del **font proporzionale di sistema `F_BODY`**, identico file TFN2 dei blocchi precedenti, anche per wrap/caret/clic. Avanzamenti memorizzati una volta per processo: niente lookup font per ciascuna lettera durante il reflow. Terminale resta monospace.
- `C_BODY` serializza riferimenti ai glifi senza trasformarli in celle monospace. Budget condiviso **144 → 256 comandi**, esplicitamente autorizzato nel contratto ABI: struttura Draw e ID preesistenti invariati. Necessario per finestre alte con glifi molto stretti (`i`/`Ì`); fixture reale su 45 combinazioni fino a 1278×1024.

Dettagli e limiti: `tinymacOS/docs/CURSORI.md`.

## Gate e ricevute

1. **Foundation completa normale + ASan/UBSan:** PASS sul codice finale, inclusi decoder/context/compositore pixel reali, coda scheduler, otto resize, font/caret/wrap, sicurezza e regressioni precedenti.
2. **M0 workspace:** PASS, 568 input vincolati; USB ritirato, overlay 29 sorgenti/12 ABI, inventario UI 153 file/2.500 letterali, versione pubblica intatta. Font deterministici dal materiale fornito, nessun rimpiazzo.
3. **Storage M2, 12 famiglie correnti normale + ASan/UBSan:** PASS. Un’unica esecuzione wrapper valida; primo tentativo senza `TMC_BUILD` fermato prima delle suite. Include 16 TiB/high LBA, 4Kn, rifiuto legacy senza scritture, TMCFS2 install/runtime/import/conversione e NVMe. Sorgenti storage invariati durante l’esecuzione; build/foundation/guest finali seguono le modifiche UI/ABI.
4. **Build retail finale e volumi sigillati:** PASS. Confronto integrale di tutte le risorse e manifest nei tre volumi, font invariato e plist italiani mantenendo percorsi eseguibili/versione pubblica.
5. **Guest retail reale QEMU, input USB e framebuffer:** PASS sullo stesso ISO finale. Setup con Text.cur e Select.cur originali/hotspot, testo proporzionale nelle tre app, movimento con posizioni intermedie e tutti gli otto resize sia live sia dopo boot installato freddo. Confronto immagini originali al ≥97% con tolleranza massima 3 livelli RGB per pixel; nessuna soglia ridotta per superare i test. Busy.ani: più frame originali riconosciuti durante l’installazione dal timer OS.
6. **Installazione GUI 4Kn e Recovery interna fredda, senza ISO:** PASS. Disco sintetico 3,2 TB con vero gap 2,7 TB, intero gap assegnato a TMCFS2; BootOrder OpenCore verificato prima del primo boot. Finder/Utility/Terminale coerenti, partizioni estranee preservate integralmente ai punti/blocchi verificati. Risorse cursore presenti sul filesystem e realmente usate anche dal Terminale Recovery.
7. **Dialoghi nativi e Calcolatrice sul disco installato finale:** PASS. UTF-8 reale, Open/Save As, annullamento senza alterazioni, Quick Look con byte corretti, `7 + 5 = 12` nella cronologia e geometria vetro +8 px conservata; spegnimento ordinario.

### Evidenza riproducibile

- ISO SHA-256: `ade556e160ddd2b0e7bd387e2ef9e4ff5a3f3705f6fa65e2dce4e217cbd0e988`.
- `production-inputs-final.json`: hash congelati di **1102 input**, con **41 percorsi di produzione modificati** dal blocco 6; `artifacts-final.json`: digest binari, delta dal blocco 6 e test specifici.
- `capacity-interaction-guest.json`, `dialogs-guest.json`, `cursor-bundles.json`, `bundle-plists.json`: ricevute macchina complete.
- Log compressi senza modificarne il contenuto. In `evidence/`, soli ritagli rettangolari lossless di framebuffer reali, con ricevute/hash degli originali: nessuna immagine generata o ritoccata.

## Problemi trovati e corretti; tentativi non riusciti conservati

- I primi guest rilevavano correttamente la freccia di emergenza nel setup. Primo fix letture ≤8.192 byte non sufficiente: la causa reale era il divieto pre-auth di `S_OPEN/S_READ`. Risolto con lista risorse sigillate `S_ART`, **senza** aprire accesso ai file utente. Il test successivo riconosce i cursori forniti al 100%.
- Il footer TextEdit su vetro non è un locator stabile. Il test finale legge i bordi reali della card opaca, correlando testo e contorni **nello stesso framebuffer**; attende l’esatta geometria finale anziché campionare durante un frame intermedio. Le asserzioni geometriche restano ±1 px.
- Il test ANI inizialmente campionava prima che il mouse raggiungesse la nuova posizione. Ora aspetta un frame originale interamente riconosciuto mantenendo soglia/hotspot, poi verifica più indici diversi.
- Revisione prestazioni: cache degli avanzamenti TFN2 verificata dalla fixture, con zero lookup aggiuntivi dopo il riscaldamento. Richiesta nuova build e ripetizione guest, non riutilizzata accettazione di un ISO precedente.
- `wc` del Terminale legge interamente file fino a 8 KiB: non è adatto al cursore ANI da 128.888 byte. La verifica Recovery usa `ls -l` (dimensione reale da stat), oltre al confronto integrale del volume sigillato e al rendering reale dei cursori. Non è stata alterata la capacità dichiarata di `wc`.
- Un tentativo di input USB troppo breve produceva comandi persi/ripetuti sotto carico host. Il collaudo finale imposta pressioni di 70 ms e intervalli di 100 ms; non cambia il driver. Il controllo Finder aspetta prima il titolo della finestra e poi il nome dentro la card per non riconoscere la riga del comando Terminale precedente.
- Le ricevute e i log dei tentativi falliti sono conservati separatamente; non vengono presentati come PASS.

## Confini della verifica

- Fluidità verificata tramite frame intermedi/geometria, backpressure e riuso delle superfici. **Nessuna promessa di FPS su hardware fisico**.
- CUR PNG-only e DIB compressi non supportati: rifiuto sicuro dichiarato, non supporto universale di ogni variante Windows.
- Partizioni straniere sintetiche: integrità verificata non equivale a certificazione Windows fisica.
- Nessun ritorno del filesystem legacy: restano vietati mount/conversioni implicite/modifiche dei volumi ritirati. TMCFS2 è l’unico filesystem nativo.
- Nessuna nuova versione/tag/release pubblica; nessun avvio da unità dell’utente. Le immagini grandi e i dischi temporanei restano esterni al repository.

## Ripetere i gate

Dalla directory `tinymacOS`, con toolchain e QEMU previsti dal progetto:

```sh
bash tests/test-10.3-foundation.sh
python3 tests/test-m0.py --workspace
export PATH=/usr/sbin:$PATH
export TMC_BUILD=/percorso/esterno/build-m7
TMC_ISO=tmc-block7-retail.iso bash build.sh
python3 tests/cursor7_bundle_check.py
python3 tests/localization6_bundle_check.py
bash tests/test-m2-acceptance.sh
export TMC_STAGE_ISO="$TMC_BUILD/tmc-block7-retail.iso"
export TMC_QA_DIR=/percorso/esterno/qa-m7
export TMC_QA_KEY_HOLD=.07 TMC_QA_KEY_GAP=.10
python3 tests/interaction7_guest.py
python3 tests/interaction7_dialogs_guest.py
```

I guest creano dischi sintetici sparsi temporanei: occorre spazio host sufficiente, non una partizione fisica da destinare al test.
