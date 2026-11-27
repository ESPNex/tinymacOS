# Macroblocco 2 — completamento e verifiche

Riferimento esclusivo: punto 2 di `tinymacOS/docs/piano.pdf`, invariato. Questo documento registra risultati, non sostituisce il piano. Il macroblocco 1 resta chiuso su indicazione dell’utente con collaudi fisici non verificati. Nessun lavoro avviato sul macroblocco 3 e nessun incremento di versione/release.

**COMPLETO per implementazione e collaudi locali host + sistema reale eseguito in QEMU. Hardware fisico e Windows reale NON collaudati.** Le ricevute finali sono in `completamento/`; hash della build in `artifacts-final.json` e 1167 input congelati verificati invariati in `completamento/source-inputs-final.json`.

## Risultato funzionale

- Utility Disco presenta il catalogo snapshot una volta sola sotto il disco montato: nessuna duplicazione di «Copia locale» o azioni snapshot. Paginazione, rotellina, tastiera, selezione stabile per nome, errori espliciti e usato/capacità con unità SI. Gli snapshot si aprono nel Finder come vere viste congelate in sola lettura.
- `/Volumes/diskNsN` e sidebar Finder espongono volumi dati reali. NTFS, FAT32 ed exFAT hanno driver nativi **lettura/scrittura**, collegati a FD/syscall e normali operazioni FS2-facing: create/read/write/truncate/append, rename/delete, copia/taglia/incolla, Terminale e drag & drop anche fra filesystem.
- Gli spostamenti cross-filesystem copiano e verificano tutti i byte prima di rimuovere la sorgente. Un errore non viene presentato come successo/rollback atomico; un’eventuale copia parziale può restare visibile. Rename/remove esterni invalidano prudentemente gli FD path-based, evitando scritture su un nuovo file che riutilizza lo stesso nome.
- La scrittura NTFS usa libntfs-3g reale con preflight distinto pulizia/ibernazione, blocco dei metadati e file protetti. FAT32/exFAT usano FatFs R0.16 patch 3 bloccato per commit/SHA, non codice ospitato/FUSE. I callback sono limitati alla partizione e non possono scrivere tabelle dischi. Nessuna promessa assoluta su corruzione o interruzioni di alimentazione.
- FileVault e snapshot **esclusi sui filesystem esterni**, come richiesto. I volumi dati nativi distinti usati come sorgente di esportazione restano in sola lettura.
- **«Esporta una copia exFAT» crea una NUOVA PARTIZIONE REALE**, non un VHD e non una conversione/formattazione di partizioni esistenti. Si sceglie `diskN`, si esegue anteprima senza scritture e si conferma esplicitamente. La tabella deve essere verificata e invariata; si usa soltanto un intervallo realmente non allocato. La copia viene verificata con readback e la voce GPT/MBR pubblicata per ultima. Origine BPB uguale all’LBA reale. Originali e snapshot conservati. Su errore prima della pubblicazione può restare materiale nello spazio non allocato, non una falsa partizione completata. Le scritture GPT verificate non sono atomiche rispetto a perdita di corrente.
- Terminale: `diskutil exportExFAT /.snapshots/nome diskN` mostra l’anteprima; `--confirm` autorizza la copia. La GUI esegue la stessa procedura anteprima/conferma. L’export 4Kn non supportato viene rifiutato prima delle scritture; questo non è un collaudo di Windows o dell’hardware fisico.

## Prove eseguite

- **PASS normale + ASan/UBSan:** backend FAT32/exFAT e NTFS, nomi Unicode, file da 65.537 byte, letture disallineate/EOF, truncate/append, rename senza overwrite, rifiuto directory cicliche, remove/nonempty, paginazione, sola lettura/errori I/O e canarini esterni. `fat-volume-rw-host.log`, `ntfs-volume-rw-host.log`.
- **PASS namespace e vera partizione exFAT host, normale + sanitizzatori:** driver attraverso i veri adapter volume/FD, anteprima senza scritture, tabella stale rifiutata, partizione esistente conservata, pubblicazione per ultima, snapshot/live distinti e fsck indipendenti. `foreignfs-rw-real-partition-host.log`.
- **PASS regressioni:** foundation, risorse USB/font/overlay, browser, catalogo Utility Disco, runtime FS2, partizioni, disk editor, resize nativo; NTFS shrink/grow/ibernazione/errori con verifiche indipendenti. Log `final-*-host.log` e `ntfs-resize-regression.log`.
- **PASS stessa ISO finale, GUI reale:** installazione, OpenCore primario verificato prima del primo avvio, NVMe senza ISO, account/home nominale, CoW restore, reinstallazione Recovery con SHA dell’intera partizione dati invariato, riavvio e verifica snapshot. `fs2-install-guest-result.json`.
- **PASS stessa ISO, filesystem esterni scrivibili:** comandi Terminale reali; copia di stream da 65.537 byte in entrambe le direzioni FS2/foreign; catena S_MOVE FAT32→NTFS→exFAT→nativo; Taglia/Incolla contestuale Finder FAT32→NTFS; trascinamento reale con pulsante tenuto NTFS→exFAT; contenuti riletti. Export da Terminale di volume nativo e dalla GUI di snapshot in due nuove partizioni exFAT. `volume-export-guest-result.json`.
- **PASS indipendente:** fsck FAT32/exFAT, consistency scan NTFS e lettore `dissect.fat` 3.13 per contenuti effettivi, Unicode, stream da 65.537 byte e BPB/LBA. SHA completo della sorgente nativa e della partizione preesistente di destinazione invariati. `independent-exfat-reader.json`.
- **PASS stessa ISO, 43 snapshot:** catalogo, pagine con mouse/tastiera, unicità Copia locale, Finder snapshot congelato. Gli snapshot stress sono dichiaratamente aggiunti dall’host a disco già installato dalla GUI. `diskutility-catalogue-guest-result.json`.

## Tre screenshot richiesti

Soltanto tre schermate finali, reali e non ritoccate: `schermate/finder.png`, `schermate/terminale.png`, `schermate/utility-disco.png`. Provenienza e SHA256 in `screenshots-final.json`. Nessun mockup, fotomontaggio o schermata generata. Non sono una certificazione di hardware fisico.

## Conservazione e limiti

Pulizia locale effettuata prima della validazione: bytecode rigenerabile rimosso e duplicati di 31 schermate storiche verificati tramite SHA contro il bundle indipendentemente ripristinato. Originali storici recuperabili con `../history-boundary.json`; prove M0–M3, M4 sospeso, upload, font e USB preservati. Vedere `../historical-image-compression.json`.

I vecchi log/checkpoint VHD/sola lettura non certificano il completamento: sono superseduti dalle ricevute finali sopra. Non viene dichiarata eseguita alcuna prova su GeForce/HDMI fisico, hardware USB reale aggiuntivo, Windows reale o chainload Windows. Nessuna release pubblicata.

## Follow-up Rinomina e TMCFS2

Completato e verificato localmente: dialogo modificabile/cliccabile e azione contestuale Rinomina su tutti i backend scrivibili; nuovi volumi Utility Disco realmente TMCFS2 CoW. Nessuna conversione dei volumi legacy. Build/host/GUI e controllo a guest spento PASS; dettagli e limiti in [rinomina-tmcfs2/VERIFICA.md](rinomina-tmcfs2/VERIFICA.md). Le evidenze precedenti in questo documento restano riferite alla ISO storica, non alla nuova build.

## Rimozione del filesystem legacy

Su richiesta esplicita, il backend nativo di prima generazione è stato rimosso, non nascosto. I volumi precedenti sono non supportati, senza conversione o modifica dei dati. Anche live/Recovery e trasporto Installer sono ora indipendenti dal vecchio formato. Build/host/guest e rifiuto zero-write verificati: [rimozione-legacy/VERIFICA.md](rimozione-legacy/VERIFICA.md). Le evidenze antecedenti rimangono storiche.
