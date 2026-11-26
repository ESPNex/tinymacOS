# 10.2.5 / 30B16 — sviluppo, NON release

Branch `work/10.2.5-scalable-fs-nvme`, base 10.2.4
`1a4ca9af4c4e7bb42cc819e7f931da34f6a4af69`. Primo checkpoint `9ef9e03`.
**Accettazione finale NON superata; nessuna pubblicazione/merge/tag 10.2.5.**
L'obiettivo richiesto è completare stanotte; non è una garanzia di completamento.
La release pubblicata 10.2.4 e le sue 34 verifiche restano invariate.

## Implementazione

- `storage/tmcbfs2.c`: nuovo formato a blocchi immutabili 16 KiB, ID/dimensioni e
  offset a 64 bit; alberi dinamici per inode, dati e snapshot; CoW reale mediante
  condivisione delle radici, non clonazione dei volumi. Pubblicazione ordinata con
  flush e due superblock CRC/generazione; recupero dei blocchi non raggiungibili
  da ENTRAMBE le radici di fallback e da tutti gli snapshot.
- Bitmap sparse paginabili, nessuna allocazione superiore a una pagina d'albero:
  risolta la richiesta contigua di 16 MiB incompatibile con l'heap kernel da 8 MiB.
  A volume pieno le mappe richiedono comunque memoria fisica proporzionale ai
  blocchi rappresentati. Adapter kernel: pagine bitmap da `page_get`, non kmalloc.
- Rename/move con rifiuto cicli; truncate pota solo il percorso di confine e
  azzera le code, senza riesporre dati dopo estensione. Slot account privati fuori
  dal namespace pubblico, condivisi/restaurati con gli snapshot.
- `fs/tmcbfs2_runtime.c`: backend kernel con FD separati da bootfs, seek 64 bit,
  namespace, lettura/scrittura, preferenze, account e snapshot. Dispatch opt-in
  `TMC_FS2_BACKEND=1`; non formattazione implicita. ABI Dirent/FD ampliata.
- `drivers/nvme.c`, `storage/nvme_native.c`, `storage/blockdev.c`: NVMe nativo
  polling, Identify e NSID sparsi, PRP-list, MDTS, read/write/flush, timeout e
  isolamento dopo esiti ambigui. DMA allocato solo per controller presenti.
- DXE NVMe pre-OS compilato da sorgente AUDK fissato, modulo non modificato;
  artefatto compresso/hash e licenze in `vendor/tmc-nvme-dxe`.
- `tools/nvme-boot-helper.py`: immagine nuova BIOS/UEFI su USB/SATA, senza kernel
  né documenti. OpenDuet/OpenCore + DXE leggono il sistema installato su NVMe.
  Il BIOS deve poter leggere l'helper: nessun software sul solo NVMe inaccessibile
  può fornire il primo codice al processore. Nessun flash del firmware.

## Prove completate

- `test-tmcbfs2.sh`, normale e ASan/UBSan: **PASS** su un VERO filesystem da 1 TiB
  in file sparse (non una piccola partizione ad alto LBA). Dati multi-MiB, mappa
  dati e catalogo su più pagine, ultimo byte a 1 TiB−1, 1.000 file, 192 snapshot,
  rimontaggio, restore/delete/GC, rename/truncate e account privati; errori/torn
  write/flush e corruzione CRC. Nessun hash/copia/scansione dei buchi da 1 TiB.
- `test-fs2-runtime.sh`, normale e sanitizer: **PASS** sul vero backend kernel,
  file 64 KiB, 1.100 file/ID oltre 1024 senza collisione bootfs, seek oltre 16 GiB,
  overwrite/truncate, snapshot, rename e account. Non sostituisce il test guest.
- `test-nvme.sh`, normale e sanitizer: **PASS** sul trasporto di produzione con
  modello indipendente di controller, alto LBA 1 TiB, PRP/MDTS/4Kn RMW/errori.
- QEMU precedente ISO `8fbd3bfe3d1624e4d166f391037e09c9067e6dd3d8edc96578382a2b0017098e`:
  installer NVMe, UEFI dal disco senza ISO, Recovery interna e documento persistente
  **PASS**, ancora con TMCBFS1. Live BIOS con NVMe **PASS**; BIOS dal solo NVMe
  senza helper **FAIL** (BOOT MISMATCH / BOOT FAIL), non convertito in successo.
- Nuova prova BIOS: SeaBIOS commit `a6ed6b701f0a57db0569ab98b0661c12a6ec3ff8`,
  compilato con `# CONFIG_NVME is not set`, SHA firmware
  `2d3ef5bccbb371c3a72baa7f9d52694d171024a985625dcccf81a7c491977de8`.
  Sistema NVMe installato avviato automaticamente con helper SATA, senza ISO;
  login e documento persistente: **PASS**. Helper protetto da overlay QEMU,
  proprio boot manager nascosto al picker per evitare ricorsione.
- Il backend è ora provato ANCHE nel sistema reale QEMU/NVMe: **PASS** mount
  TMCBFS2, stream SHA-256 di un file 128 KiB, snapshot creato dal guest,
  modifica/ripristino CoW, documento e account dopo riavvio senza ISO.
  KernelCache provato: `c91a05c791134e55b062a281985fbaf55d1697c1b7bd634afda1acf7ac036ef7`.
  Fixture: 1.100 ulteriori file e 20 snapshot su tutta la partizione dati
  (~37 MiB, NON una prova guest di filesystem 1 TiB). Il formato 1 TiB è
  verificato separatamente dal test host. Le aggiunte a verifica contenuti,
  preflight readonly e conteggio inode erano allora coperte solo dai test host;
  la successiva prova installer reale sotto copre il kernel aggiornato. Il default resta spento.
- Helper BIOS: **PASS anche controllo negativo senza helper** (nessun device
  avviabile), sistema e Recovery interna con verifica volume, nessuna ISO,
  firmware NVMe disabilitato, hash del file helper immutato mediante overlay QA.
- Dopo ABI/backend: **PASS** i sette gruppi host installer-flow, partitions,
  native-cow, native-resize, large-disks, nvme e fs2-runtime, più il core TMCBFS2.
  Non sono una regressione finale integrata né una prova del nuovo installer.
- Aggiunto controllo segmenti ELF al build: un primo DMA statico eccedeva il
  limite del loader. Corretto senza alzare arbitrariamente il limite.

## Gate ancora aperti

L'installer opt-in ora crea TMCBFS2 e importa la sessione live verificata:
prova GUI/NVMe, boot installato, Recovery interna, stessa app Reinstalla e
riavvio dopo reinstallazione **PASS** (vedi aggiornamento sotto). Il default
resta spento. Mancano importazione/restore da backup scalabile, gestione della
copia locale, browsing snapshot completo, paginazione UI/catalogo e cifratura
autenticata scalabile. FileVault v1 è a geometria fissa e NON va applicato ai
blocchi liberi del nuovo formato; anche Recovery riconosce ora il formato v2.
L'adapter sperimentale v2 è per ora SOLO plaintext: deve rifiutare l'attivazione
FileVault, mai dichiarare protetti dati non cifrati. Vecchio formato conservato.
Scritture multiblocco: commit per blocco, possibile prefisso dopo crash, non
atomicità dell'intero file. La verifica snapshot ora legge anche contenuti e
account con CRC; il preflight ricorsivo rifiuta readonly prima delle mutazioni.

NVMe attuale: 4 controller, 8 namespace/controller e 8 dischi esposti, ABI dischi
fino a 1 TiB/settori 512 B. Nessun troncamento silenzioso. RMW 4Kn nel trasporto
non significa installazione/boot 4Kn: quest'ultimo non è esposto né certificato.

Richiesti prima del rilascio: completamento di tutti i gate sopra, verifica
BIOS/UEFI e Recovery sull'artefatto definitivo, regressione completa sull'ISO
finale, quindi revisione/merge/tag/pubblicazione con readback SHA indipendente.
Nessuna prova fisica. Evidenze e dischi virtuali: `/var/tmp/tmc-10.2.5/`.

## Aggiornamento: installer reale e TextEdit

- Rimosso da TextEdit il vecchio modal con campo percorso; la conferma delle
  modifiche non salvate ha pulsanti reali **Annulla / Non salvare**, separati
  dal selettore moderno esistente. Solo un file effettivamente accettato può
  sostituire i byte correnti: annullare anche DOPO la conferma li conserva.
- **PASS QEMU**, `tests/filepanel_ui.py`: Apri da toolbar, File e Ctrl+O,
  documento modificato, annullamento con mouse/tastiera, selettore moderno,
  salvataggio/overwrite/readonly e file byte-esatto dopo annullamento.
- Corretto un difetto reale in `fs2_list`: il risultato di readdir sovrascriveva
  l'ID della directory usato all'iterazione successiva. Test indipendente:
  1.101 file enumerati tutti in pagine da 37, senza duplicati né interruzione
  dopo il primo elemento, normale + ASan/UBSan. Questo NON aggiunge da solo
  la paginazione nelle app Finder/Time Machine/selettore.
- `storage/tmcbfs2_seed.c`: valida l'immagine LIVE plaintext PRIMA di scrivere
  il target; rifiuta cicli, nomi duplicati/illegali, cipher legacy e troncamenti.
  Mappa slot sorgente sparsi/non ordinati, conserva file/readonly e due record
  account. Verifica integrale dopo import e dopo remount; snapshot iniziale
  **Installazione** con vera condivisione delle radici CoW.
- `fs/tmcbfs2_install.c` + installer: formato esplicito solo su destinazione
  appena selezionata e validata; nessuna formattazione al mount. Tutti i blocchi
  della partizione dati (arrotondata a 16 KiB) sono indirizzabili dal nuovo
  formato. GPT/MBR attivati soltanto dopo rilettura del nuovo filesystem.
- **PASS normale + ASan/UBSan**, stato installer EFFETTIVO, GPT e MBR con altre
  partizioni FAT32/NTFS/RAW: import/core reali, IO limitato all'extent scelto,
  hash delle partizioni esistenti invariati. Sorgente invalida: zero scritture;
  errore target: nessuna attivazione con falso successo.
- **PASS guest installazione reale**: ISO
  `1a3b3b7db2dab8415b1a8ac1d13f10af1e4c7628d8597cc7f5c045094c44d96d`,
  KernelCache `e9bb5b52a8961c7f9f87de211224e8d558e7a87a2cde20b9bb5870c03ee169ac`.
  NVMe QEMU, dati NON preformattati dall'host; account e documento importati
  dalla live, boot senza ISO, modifica/ripristino CoW e persistenza al reboot.
- **PASS Recovery interna sul nuovo formato**: stesso installer contestuale
  Reinstalla, verifica CRC di dati/account/snapshot e sigillo sistema;
  SHA-256 dell'INTERA partizione dati invariato durante la reinstallazione:
  `7b0cdab54669ff8a84fb117f71597dc0d3d9cfb4172be1a0304d26cecdcb259e`.
  Successivo boot/autenticazione/documenti/verifica snapshot: PASS.
- Blocchi di sicurezza: operazioni locali backup/reset TMCBFS1 non vengono MAI
  eseguite su TMCBFS2. Per ora restituiscono errore esplicito: non sono una
  implementazione di backup/restore/reset scalabile. V2 rimane plaintext-only.
- Queste prove NON certificano installazione guest 1 TiB, boot/install 4Kn,
  supporto hardware fisico o la release finale; **10.2.4 resta pubblicata**.

- **PASS BIOS senza NVMe anche sul nuovo installer TMCBFS2**: stessa installazione
  guest, stesso KernelCache `e9bb5b52...`, SeaBIOS compilato con NVMe disabilitato;
  controllo negativo senza helper, boot sistema con account/documento persistenti,
  boot Recovery e verifica volume. Nessuna ISO; hash helper invariato. Questo
  sostituisce per il nuovo formato il precedente test svolto solo su TMCBFS1.
- **PASS 12 gruppi** sull'attuale sorgente, normale e sanitizer ove previsti:
  textedit-open, fs2-seed, fs2-install, fs2-runtime, tmcbfs2, installer-flow,
  partitions, native-cow, native-resize, large-disks, nvme, vault. FileVault è
  la regressione del vecchio formato, NON cifratura del nuovo filesystem.
  Non è la matrice finale completa richiesta per pubblicare.


## Aggiornamento: backup locale e UI scalabili, reset atomico

- Snapshot montati in `/.snapshots`, FD readonly legati all'ID del catalogo;
  eliminazione/sostituzione invalida i vecchi handle. `Copia locale` sostituita
  atomicamente, mai eliminata prima di creare il nuovo checkpoint.
- Finder/selettore: pagine limitate a 64 elementi, Time Machine a 8 snapshot.
  I/O condiviso in streaming da 8 KiB; TextEdit fino a 256 KiB con caricamento
  staged, senza sostituire il documento aperto dopo una lettura fallita.
- **PASS normale + ASan/UBSan**: browser-pages, ui-stream, textedit-open,
  fs2-runtime, fs2-install e tmcbfs2. Checkpoint sottoposti a 32 interruzioni
  write/flush/torn, con una sola copia vecchia o nuova ancora leggibile.
- **PASS reale QEMU** su ISO
  `599e14f7bdd7639008713d6778e1a9bdb2a6718b8bf49a78503da128562f2088`:
  installer GUI, account/documento importati, boot NVMe senza ISO, Reinstalla
  nativa senza modificare la partizione dati, riavvio; Finder E selettore
  raggiungono il file 1099, Time Machine verifica lo snapshot 20; TextEdit apre
  e salva 128 KiB con SHA esatto; Recovery crea la copia locale, poi ripristina
  il documento modificato, account, file grande e snapshot dopo riavvio.
  I 1100 file/20 snapshot sono aggiunti dall'host DOPO l'installazione reale,
  non sono prova di creazione massiva dal guest. Il test visivo attende ogni
  pagina con soglia bitmap rigorosa .985: il riconoscimento .88 dei singoli
  numeri produceva falsi positivi ed è stato scartato.
- Ricevuta persistente:
  `workspace/local-10.2.5/scalable-backup-ui-verification.json`.
  ISO precedente eliminata solo dopo registrazione SHA; evidenze PNG/JSON e
  fixture installata conservate. Nessuna modifica alla release 10.2.4.
- Iterazione successiva: cookie sugli alberi evita di rileggere i sottoalberi
  precedenti. **PASS normale/sanitizer**, 1001 elementi in <=2100 letture di
  pagina, catalogo/view snapshot, EOF stabile e cookie UINT64_MAX.
- Reset logico scalabile: costruisce un nuovo albero home completo PRIMA
  della pubblicazione unica; account vuoti e catalogo snapshot vuoto sono
  pubblicati insieme. Contatori ID mai riavvolti, entrambi i fallback ritirati
  prima di restituire successo. Rifiuta il volume montato e piani seed invalidi
  prima delle mutazioni. **PASS normale/sanitizer**, 28 interruzioni
  write/flush/torn: o account/file/snapshot precedenti, o struttura nuova intera.
  Non è cancellazione sicura dei residui plaintext sul dispositivo fisico.
- Nuova ISO con iterazione/reset:
  `dced20a3b59f2fb0f32cf817fb6a66b2c69b177e88bcc372c2247dda3a27a4e0`;
  KernelCache `15aad3e27d97ccbf08b1bbbfab0b8971a6ea3557a0bdf84a4e5e87d74fdbcb3f`.
  Build completo PASS, kernel e Recovery 578084 byte sotto il limite loader.
  Pipeline reale installazione + backup/UI + reset **PASS**, poi helper BIOS
  senza supporto NVMe nel firmware **PASS** sullo stesso kernel.
  Anche tutti i **15 gruppi host integrati PASS**. Questi gate circoscritti
  non equivalgono all’accettazione finale della release.

Restano bloccanti cifratura autenticata scalabile, importazione da copia esterna,
boot/installazione 4Kn nativi e matrice finale sull'artefatto definitivo. Il reset
è verificato anche nel guest: annullamento senza alcun byte dati modificato,
conferma senza modificare gli extent non-dati (SHA), nuovo enrollment e dati
nuovi persistenti dopo reboot. Ricevuta finale di questo checkpoint:
`workspace/local-10.2.5/scalable-reset-verification.json`.
Default FS2 spento; nessun commit finale/push/merge/tag/deploy 10.2.5.


## FV003 update — 2026-11-18

See [VAULT2-DEVELOPMENT.md](VAULT2-DEVELOPMENT.md) for the current format,
security limitations and exact scope. Standalone crypto and production kernel
adapter normal/sanitized host gates pass; independent libsodium verification
passes. The current ISO now passes actual GUI plaintext install/reinstall, then
host-prepared encrypted native NVMe login, 128 KiB TextEdit I/O, native Recovery
backup/restore and offline plaintext-marker scan. BIOS without firmware NVMe
also passes helper boot, encrypted Recovery and same-app reinstall with the whole
encrypted DATA hash unchanged. This is NOT encrypted fresh-installer acceptance
or in-place conversion acceptance. Encrypted reset is intentionally refused.

The persistent `workspace/local-10.2.5/vault2-verification.json` records exact
ISO/kernel/evidence hashes. Earlier reset/UI receipts remain historical evidence
on their own older ISOs, not current-artifact full regression. No release, merge,
tag, push or deployment; published 10.2.4 remains unchanged.


## Macroblocco installazione cifrata / enrollment / reset — completato

L’aggiornamento corrente supera le precedenti limitazioni «solo fixture cifrata
preparata sull’host»: l’installer GUI crea direttamente FV003 e importa il vero
account live. Conferma recovery prima delle scritture e controllo kernel della
modalità impediscono downgrade silenziosi a plaintext dopo errori/annullamenti.
Recovery offre un reset ESPLICITAMENTE IRREVERSIBILE con nuova chiave dati/UUID,
password e recovery slot nuovi, poi nuovo enrollment al riavvio. Non è atomico,
non garantisce rollback o secure erase fisico: la politica è documentata, non
sottintesa come reset sicuro per cessione del supporto.

**PASS 8 gruppi host normale+sanitizer e 4 gate guest sulla stessa ISO corrente**,
inclusa regressione plaintext install/reinstall, Finder/selettore 1099, snapshot
20, TextEdit 128 KiB, backup/restore, reset e account nuovi persistenti. Kernel e
Recovery mantengono il vecchio limite loader con 2099740 byte di margine, grazie
alla condivisione serializzata di un workspace duplicato, non a limiti ridotti.

Vedere [ENCRYPTED-ENROLLMENT.md](ENCRYPTED-ENROLLMENT.md) e la ricevuta persistente
`workspace/local-10.2.5/encrypted-enrollment-verification.json` per SHA/esiti e
limiti esatti. Restano conversione/deattivazione o decisione esplicita di scope,
backup esterno, installazione/boot 4Kn e intera matrice finale di rilascio. FS2
resta opt-in; nessun push, merge, tag o deploy; 10.2.4 non modificata.
