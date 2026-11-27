# Macroblocco 2 — follow-up Rinomina e TMCFS2

## Modifiche

- I volumi exFAT sono esplorabili e scrivibili dal Finder e dal Terminale nel namespace `/Volumes/diskNsN`; Utility Disco offre «Mostra nel Finder».
- Finder: azione contestuale «Rinomina», dialogo condiviso con Nuova cartella/Nuovo file, campo modificabile con selezione iniziale, cursore, navigazione e cancellazione UTF-8, conferma Invio o pulsante e Annulla. La sorgente resta fissata all'apertura; errori e collisioni mantengono aperto il dialogo. Le protezioni di sola lettura restano attive.
- Il formato proprietario di seconda generazione è identificato come **TMCFS2** (magic su disco invariato `TMCBFS2`). Btrfs Linux resta distinto. I supporti di prima generazione sono identificati come **TMCFS (legacy)**, senza conversione implicita.
- Nuovi volumi nativi da Utility Disco: uso reale del core CoW di seconda generazione (`tmc2_format`, verifica e rimontaggio prima di pubblicare la partizione), nello spazio non allocato verificato. Non è una semplice modifica dell'etichetta del vecchio formatter.
- Volumi esterni TMCFS2: creazione, lettura/scrittura, rinomina/spostamento e rimozione tramite il namespace comune. Le protezioni ereditate dalle directory restano rispettate.
- Il ridimensionamento TMCFS2 non è implementato: viene rifiutato senza scritture. Non dichiarato come funzionalità disponibile.

## Verifiche host e build

Build completa BIOS/UEFI/Recovery/applicazioni: PASS (`build.log`).

Suite PASS, normali e sanitizzate dove previsto dai rispettivi runner:

- `test-rename-dialog.sh`: macchina degli eventi Finder effettiva; contesto, campo UTF-8, selezione, navigazione, cancellazione, errori, sorgente congelata, annullamento e sola lettura.
- `test-diskedit.sh`: nuova formattazione TMCFS2 con il core distribuito, file Unicode da 65537 byte, rinomina, snapshot CoW immutabile, troncamento e rimontaggio a freddo.
- `test-foreignfs.sh`: filesystem esterni, namespace/FD, esportazione e operazioni native esterne; verifiche indipendenti dei filesystem esteri.
- `test-browser-pages.sh`, `test-diskutility-catalogue.sh`.
- `test-10.3-foundation.sh`, `test-partitions.sh`, `test-fs2-runtime.sh`.
- `test-m0.py --workspace`: 568 input protetti font/asset/vendor/USB, avvisi distruttivi e nomenclatura; overlay 29 sorgenti + 12 ABI e inventario statico UI.

Pulizia delle cache Python generate; build e supporti di prova grandi conservati fuori dal workspace persistente, in `/var/tmp`. Nessun font o caricamento utente eliminato.

## Accettazione grafica

**PASS sulla nuova ISO**, non sull'ISO storica: 7 controlli in `guest-result.json`.

- Creazione reale tramite Utility Disco nello spazio libero, identificazione TMCFS2 e apertura «Mostra nel Finder»; creazione directory/file e copia di 65537 byte.
- Menu contestuale e dialogo Rinomina effettivi per file e directory su Home nativa, FAT32, NTFS, exFAT e nuova TMCFS2. Conferma sia cliccabile sia Invio; collisione exFAT rifiutata e file preesistente conservato; contenuti dei file e dei figli riletti nel guest.
- Dopo lo spegnimento reale: estrazione in sola lettura con il core TMCFS2 distribuito del file esatto da 65537 byte e del figlio della directory rinominata; prime tre voci di partizione invariate; `fsck.fat -n`, `ntfsresize -i -f` (controllo informativo) e `fsck.exfat -n` PASS.

Tracciabilità degli errori di automazione: i primi tentativi si sono fermati su selettori grafici (spazio libero iniziale, nome account coincidente con Rinomina, font del menu contestuale). L'ultimo guest ha completato tutti i 6 controlli GUI e si è spento, ma l'estrattore host passava un callback write nullo non ammesso dall'API del core. Corretto **solo l'helper di test** con un callback che rifiuta ogni scrittura, mantenendo il disco aperto O_RDONLY. Il controllo post-spegnimento è stato ripreso sul medesimo disco, verificando hash ISO/kernel e le 6 ricevute GUI precedenti; risultato finale PASS. `guest.log` conserva il fallimento dell'helper e `postshutdown.log` il PASS successivo; `--verify-existing` rende esplicita e riproducibile questa ripresa. Nessuna modifica al core o alla ISO per superare questo controllo.

`artifacts-final.json` lega hash e dimensioni alla nuova ISO; `source-inputs-final.json` congela gli input. Le sole modifiche successive alla build riguardano lo script guest e l'helper host di estrazione; gli input di produzione sono invariati.

## Limiti e provenienza

Follow-up del commit pubblicato `667a815862013234d0add20d9fddf92a1ff584f0`. Le evidenze complete precedenti del macroblocco 2 rimangono storiche e non vengono attribuite alla nuova ISO.

Nessuna versione/release incrementata e nessun lavoro sul macroblocco 3. Nessuna accettazione fisica GeForce/HDMI o Windows dichiarata. Gli snapshot e FileVault dei filesystem esteri restano fuori ambito. Il lavoro M4 preesistente resta conservato.
