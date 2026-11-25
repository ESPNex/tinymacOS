# Snapshot nativi Copy-on-Write

Il filesystem proprietario `TMCBFS1` usa snapshot **CoW a settori**, non una copia completa del volume a ogni creazione. Non è il formato APFS di Apple né un porting interoperabile di Btrfs Linux.

## Condivisione e scritture

- Ogni snapshot ha una mappa di riferimenti/checksum, non un'immagine completa. All'inizio i dati sono condivisi con il volume attivo: vengono scritti solo i metadati della mappa.
- Alla prima modifica di un settore ancora condiviso, il vecchio contenuto viene conservato **una sola volta** nel pool. Tutti gli snapshot che ne hanno bisogno fanno riferimento allo stesso blocco.
- Le scritture successive non duplicano nuovamente quel blocco per gli stessi snapshot. Un nuovo snapshot può conservare una generazione più recente.
- La cancellazione libera soltanto i blocchi senza più riferimenti. Un pool pieno rifiuta la scrittura prima di cambiare i dati attivi; eliminare snapshot può liberare lo spazio necessario.
- Un journal contiene l'intento, i riferimenti interessati e il checksum dei nuovi dati, scritti prima in un settore di staging. Al riavvio si completano mappa/catalogo/scrittura interrotta prima di esporre gli snapshot. Mappe o blocchi incoerenti non vengono accettati silenziosamente.

Le vecchie immagini snapshot vengono convertite una volta tramite una permutazione dei settori, gestendo le sovrapposizioni senza creare un'altra copia completa del volume. Una conversione o un resize interrotto lascia un marcatore che impedisce il montaggio automatico: **non è garantito il rollback di queste operazioni**. Conservare un backup esterno verificato.

## Integrazione

- Finder, Terminale, Utility Disco e Time Machine condividono lo stesso backend; le viste `/.snapshots/<nome>/` restano di sola lettura.
- Il ripristino di uno snapshot conserva gli altri punti storici. La copia locale indipendente di Recovery rimane un backup distinto; ripristinarla passa attraverso CoW e non sovrascrive il catalogo o il pool attuali.
- Il resize riorganizza il pool e le mappe senza trasformare gli snapshot in copie complete. Rifiuta una riduzione se inode o blocchi necessari agli snapshot non possono stare nella nuova capacità.
- Tutte le operazioni usano il dispositivo autenticato del volume: **FileVault cifra e autentica anche mappe, journal, staging e vecchi blocchi del pool**. Nessun percorso SATA in chiaro alternativo. La vecchia cifratura per-file non può essere attivata sopra CoW; FileVault corrente è l'interfaccia del volume intero.
- La modalità live/Recovery usa lo stesso motore in RAM; i dati non diventano persistenti per questo.

## Limiti espliciti

Il formato rimane piccolo: massimo 8 snapshot, 384 voci complessive e 8 KiB per file. Il resize nativo varia realmente slot disponibili e layout nel campo **8–32 MiB**; ad esempio 32→24→32 MiB cambia 384→288→384 slot e ricolloca mappe/pool/backup. I volumi legacy più grandi restano leggibili ma non diventano filesystem general-purpose. Gli snapshot condividono lo stesso disco: non proteggono da un guasto fisico.

## Prove

`bash tests/test-native-cow.sh` esegue motore, driver reale, resize e vero motore FileVault, normalmente e con ASan/UBSan. Copre condivisione iniziale, due snapshot sullo stesso vecchio blocco, generazioni diverse, riapertura del dispositivo, cancellazione/riferimenti, pool pieno, errori e settori strappati, conversione legacy sovrapposta, lettura dei file, resize e ripristino. `tests/native_cow_guest.py` verifica poi il kernel/CLI, i boot BIOS/UEFI, le mappe persistenti e il backup/ripristino in Recovery sull'ISO corrente. I risultati host non sostituiscono la matrice finale guest né una certificazione hardware/Windows.
