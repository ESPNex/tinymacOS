# M8 — toolbar Utility Disco, riferimento Dock, backup su Drive

**Checkpoint parziale verificato. M8 non concluso, nessuna release anticipata.** Supera `PRIORITA-E-MOUNT.md` per UI Utility Disco, Dock, backup e stato pubblicazione. I risultati precedenti restano storici, non vengono cancellati.

## Commit di sicurezza immediato

Prima delle modifiche di questa richiesta è stato creato e pushato `80537612eeea68bda1efe8cc4b643d0cbc9c3937` su `ESPNex/tinymacOS`, branch main. `git ls-remote` autenticato ha restituito lo stesso SHA. Nessun tag, cambio versione, release ISO o video finale M8.

## Utility Disco

- Pulsante Monta **oppure** Smonta nella toolbar, subito accanto a S.O.S. Non due comandi contemporanei.
- Stato ricavato dal backend a ogni paint e ricontrollato al clic; nessun target contestuale memorizzato. Il kernel rivalida richiesta, FD aperti, flush e protezioni come prima.
- Disabilitato su disco intero, spazio non allocato, snapshot, ruoli protetti o selezioni non gestibili.
- Rimossi handler mount del menu contestuale Utility Disco e relativo inoltro dal desktop. Il menu generico finestra può ancora apparire; non offre Monta/Smonta. Solo Finder riceve la richiesta contestuale per queste azioni.
- Recupero e Nuovo snapshot spostati a destra per fare spazio; test della Draw list reale a larghezze 560, 600, 808 e 1100: quattro pulsanti senza sovrapposizioni e interamente nella finestra.
- Guida aggiornata: `tinymacOS/docs/VOLUMI.md`.

Riferimento Apple: https://support.apple.com/it-it/guide/disk-utility/dskutl1027/mac (selezione volume e pulsante Monta). Il nome e l’aspetto tinymacOS restano propri del progetto.

## Mission Control: come si presenta il Dock in macOS

È stato esaminato il riferimento visivo **macOS Sonoma 14**, non solo la guida testuale Apple:

- Archivio: https://512pixels.net/projects/aqua-screenshot-library/macos-14-sonoma/
- Frame Mission Control: https://media.512pixels.net/downloads/macos-screenshots/14/14-Sonoma-Mission-Control-scaled.png

Il Dock è visibile in primo piano, con icone luminose; non è sotto il velo uniforme che scurisce lo sfondo. Si mantiene quindi questa separazione già presente nel compositore, invece di oscurare artificialmente anche il Dock. Il codice ora documenta esplicitamente la scelta. Non si afferma di aver eseguito o misurato macOS nell’ambiente di lavoro, né di riprodurre ogni variante/versione del suo compositore.

La barra dei menu rimane oscurata come richiesto precedentemente; miniature finestre non oscurate in aggiunta. `priority8_guest.py` ora verifica anche il Dock: nuclei bianchi opachi delle icone identici **al 100%** prima/durante Mission Control. Il test esclude bordi antialias/traslucidi, legittimamente dipendenti dal backdrop (maschera luminosa erosa, non soglia scelta su pixel finali). Blend barra 112/256 esatto al 100%; uscita ripristina la barra. Non è una rivendicazione di parità visiva completa di Mission Control.

## Prove e limiti

ISO candidata, non release: SHA-256 `97104d7c09285284f39e23d7bf425e72728ce0366385c7c235ce4e1c8acf57f7`.

- Build integrata: exit 0, `build-toolbar.log.gz`.
- Suite host finale: `toolbar-host-final.log.gz`, normale + ASan/UBSan per geometria/toolbar reale, catalogo snapshot, appearance, apps, desktop/input/campi, cursori; contratto M0 e inventario UI aggiornato dopo revisione. Stato finale del processo registrato prima del commit.
- `toolbar-mount-guest.log`: PASS, Terminale nativo, menu Finder, toolbar Utility Disco in entrambe le direzioni, assenza voci mount nel menu Utility Disco, lettura finale dello stesso file. Intero disco FAT32 disposable identico prima/dopo: `3a4b2c481ef6984013142e10808b58ec02af46d08d498809d535e10687ab2d6c`. Rilettura indipendente mtools PASS. Trasporto NVMe, **non prova storage USB**.
- `toolbar-visual-guest.log`: PASS, Libreria senza velo/ombra, accento con cambio sfondo originale, Calcolatrice 7+5=12, barra e Dock Mission Control.
- Tutti i **473** file della baseline USB/input ancora byte-identici. Nessuna modifica all’input hardware.

Durante lo sviluppo sono state corrette tre aspettative dei test, non difetti del volume: fixture host senza numero partizione; cursore sopra la scritta Monta che impediva il riconoscimento dei pixel; maschera Dock iniziale che includeva bordi traslucidi. Inventario M0 rigenerato dopo revisione delle modifiche. Le prove definitive sono le esecuzioni PASS e la nuova ISO sopra, non i tentativi intermedi.

Tre nuovi frame interi VM, sola ricodifica JPEG senza ritocchi, caricati e verificati per size e MD5 nella cartella:
https://drive.google.com/drive/folders/13rcn8izgQOjVSbgxALro1ftP9oqwmj3R

- Smonta nella toolbar: https://drive.google.com/file/d/1aZqnyngMO12CJGeQdOY7iLF8IWEa95sX/view
- Monta nella toolbar: https://drive.google.com/file/d/1qbJJTMr9v3k4tk1X7WLe0pViOf8hRBXR/view
- Mission Control con Dock luminoso: https://drive.google.com/file/d/15TJqkbW37tazTUv69d-MxD3ec0oUyUhe/view

Ricevute: `toolbar-screenshots-drive.json`. Staging locale rimosso dopo verifica; frame PNG originali fuori dal workspace nella directory scratch dei test.

## Backup Git spostato su Drive

Bundle storico preservato byte per byte, più incremento fino al commit di sicurezza e metadati necessari al ripristino:

1. Base fino a M7: https://drive.google.com/file/d/101FfA1ByltMiCsYt9csHeU0lhJNZs3-k/view — 61.934.891 byte, MD5 `92593ced101265f05f1f78c91412f4ec`, SHA-256 `093b045b0979965aacaafd4599cfa36c847c6c5e882982f11c9ee9f5835a9ca1`.
2. Incremento M8 fino a `8053761`: https://drive.google.com/file/d/1qP1B1t7bAZYRyneN0VDqPOUBUcsKXuPh/view — 4.398.816 byte, MD5 `5ac01962f95e6ded59bf9c3917e8ea18`.
3. Boundary/istruzioni, ricevute e archivio dei sorgenti originali modificati: https://drive.google.com/file/d/1mBrcaJDjtI9_mnY6Sr_rBWnZv_OuoCye/view — 120.308 byte, MD5 `6a6cb7d958315af787b4b951d9c9cdb2`.

Size/MD5 confrontati con i file locali prima della rimozione; ulteriore get_file Drive conferma tutti presenti e non nel cestino, checksum e dimensioni identici. SHA-256 locali conservati nel manifest `git-backup-drive.json`; non si presenta uno SHA locale come hash ricalcolato da Drive.

Ripristino indipendente base + incremento in nuovo repository e `git fsck --full` PASS, main ricostruito esattamente `80537612eeea68bda1efe8cc4b643d0cbc9c3937`. Il repository storico è **shallow** al boundary `c4597948f6b48ea9ed9e6e95c039a8edb0e8ee0e`: impostare il file `shallow` prima di unbundle, come spiegato nei metadati. Un fetch ingenuo senza boundary fallisce; non si inventa storia antecedente che non era disponibile.

Rimossi i bundle locali pesanti dopo upload verificato, conservando repository attivo `/var/tmp/tmc-103.git`, piccoli metadati e credenziali private non toccate. Nessuna credenziale è stata inclusa nei bundle pubblicati, nei report o nell’archivio metadati. Il backup Drive documentato copre il commit di sicurezza; eventuali commit successivi della correzione toolbar sono pubblicati su main, senza alterare i due bundle verificati.

## Ancora aperto

Storage USB canonico con bulk/BOT/SCSI, `hdiutil` nativo RAW/IMG/ISO/DMG, accettazione finale integrata, prestazioni finali, versione/release GitHub 10.3 e video M8 soltanto dopo i gate. Vedere `NEXT.md`. Nessuna certificazione fisica o dichiarazione M8 concluso.
