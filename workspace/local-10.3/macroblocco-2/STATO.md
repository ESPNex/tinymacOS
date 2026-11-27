# Macroblocco 2 — risultati in corso

Riferimento esclusivo: punto 2 del PDF fornito dall’utente, `tinymacOS/docs/piano.pdf`. Il macroblocco 1 è chiuso su indicazione esplicita dell’utente, mantenendo non verificati i collaudi fisici. Questo registro non è un piano sostitutivo.

**Macroblocco 2 IN CORSO, non completo e non pubblicato come release.**

## Aggiornamento vincolante della richiesta utente

La validazione finale è stata **fermata**. Il prototipo non pubblicato esportava un VHD contenente exFAT: **non soddisfa la richiesta** di una partizione exFAT reale e non viene accettato come completamento.

**Destinazione scelta dall’utente:** nuova partizione exFAT nello spazio realmente non allocato, da popolare con la copia. Non una partizione esistente.

Il blocco 2 comprende ora anche lettura **e scrittura** su NTFS, FAT32 ed exFAT tramite le normali operazioni del filesystem: Finder, Terminale, taglia/copia/incolla, rinomina, eliminazione e drag & drop, anche fra filesystem diversi. FileVault e snapshot restano esclusi per i filesystem esterni. I lettori in sola lettura e il prototipo VHD sono lavoro intermedio, non prova di completamento di questi requisiti. Nessuna partizione esistente va formattata o convertita implicitamente; sorgente e snapshot devono restare preservati.

Pulizia locale eseguita prima di riprendere la validazione: da **126,14 MB a circa 112,3 MB** (circa **107,1 MiB**), margine di circa **15,7 MB** sul tetto di 128.000.000 byte. Rimossi bytecode rigenerabile e 31 duplicati locali delle schermate storiche M0–M3. I PNG originali sono stati recuperati da un object store indipendente creato dal bundle e controllati byte per byte tramite SHA256; nessuna perdita delle prove. Il ripristino del bundle richiede il confine shallow documentato in `../history-boundary.json`. Manifest: `../historical-image-compression.json`. Sorgenti, foto utente, font, USB, lavoro M4 e prove del blocco corrente preservati.

**Nuovo backend FAT32/exFAT, solo test isolati:** aggiunti sorgenti FatFs R0.16 con patch 3, bloccati per commit/SHA256 in archivio compresso, adapter freestanding con vista limitata alla partizione e formatter exFAT reale (nessun VHD/MBR/GPT scritto dal driver). PASS normale + ASan/UBSan: file Unicode da 65.537 byte, letture disallineate/EOF, scrittura/troncamento/append, create/rename/remove, rifiuto sovrascrittura e cicli di directory, paginazione, errori I/O e sola lettura, canarini esterni invariati. Filesystem indipendenti FAT32/exFAT e partizione exFAT formattata dal nuovo driver superano i rispettivi fsck. Log: `completamento/fat-volume-rw-host.log`.

**Non ancora integrato** nel namespace `/Volumes`, syscall, Finder, Terminale e operazioni fra filesystem. La scrittura NTFS e l’esportazione in una nuova partizione da spazio non allocato restano da completare. Questi test di backend non sono accettazione finale del blocco 2.

Le prove descritte sotto sono checkpoint storici: non certificano il nuovo perimetro di scrittura/esportazione. Nessuna nuova release, incremento di versione, chiusura del blocco 2 o avanzamento al blocco 3.

## Correzioni eseguite

- Il catalogo `S_SNAP` appartiene al volume montato, non al disco selezionato. Utility Disco ora lo mostra una sola volta sotto quel disco: né «Copia locale» né gli snapshot vengono replicati sotto un altro disco nativo, live o Recovery. Le operazioni sullo snapshot controllano la selezione prima di agire.
- Rimossa la limitazione ai primi otto snapshot nell’app: pagine da 16 con look-ahead, «Altri snapshot…» e «Snapshot precedenti…». La rotellina rende raggiungibili i controlli fuori dall’area visibile; la tastiera attraversa le stesse pagine. Nessun array proporzionale al catalogo e nessuna troncatura silenziosa a 8/16 elementi.
- Il refresh conserva l’identità dello snapshot selezionato, non lo slot: cancellazione o riordino non possono reindirizzare silenziosamente Ripristina/Elimina allo snapshot successivo. Se l’ultima pagina scompare si torna a una pagina valida; gli errori di lettura sono espliciti e non mostrano un catalogo vecchio.
- «Utilizzo» mostra usato e capacità reale, ciascuno formattato una sola volta con unità SI, per esempio `8.1 MB su 10.0 GB`.
- «Mostra nel Finder» apre la vera vista in sola lettura `/.snapshots/<nome>`, non la directory live. Selezioni di altri dischi e snapshot scomparsi vengono rifiutati.
- Aggiunta la posizione «Snapshot» nel Finder. Painting, clic, menu contestuale e destinazioni DnD condividono lo stesso hit test delle posizioni. Corretta anche la soglia verticale che avrebbe nascosto l’ultima riga alla dimensione predefinita del Finder (782×392).

## Test host

- PASS normale + ASan/UBSan sul modello della vera app: 43 snapshot raggiunti una volta ciascuno, una «Copia locale», due dischi, live/Recovery, pagine, rotellina, identità della selezione, letture fallite, apertura Finder e snapshot scomparso. `diskutility-catalogue-host.log`.
- PASS normale + ASan/UBSan su usato/capacità con unità SI e buffer limitati. `used-capacity-host.log`.
- PASS Finder/Time Machine/file panel normale + ASan/UBSan, inclusi 1100 file e hit test della nuova voce alla dimensione predefinita, nascosta correttamente nelle finestre troppo piccole. `browser-pages-host.log`.
- PASS foundation completa, backend montato FS2, preservazione USB/font/overlay e build completa BIOS/UEFI/Recovery/app: log dedicati. Gli esiti del guest hanno ricevute autonome, non sono dedotti dai soli test del modello.

## Collaudo guest

**PASS guest finale**, ricevute `fs2-install-guest-result.json` e `diskutility-catalogue-guest-result.json`, stessa ISO di sviluppo SHA256 `3debba9e7e65245d015bbe2b086b90977098812c3506be97023dc15dfef6a765`. Tutti i processi di build/test sono terminati.

- Installazione da GUI, OpenCore primario verificato prima del primo boot installato, avvio NVMe senza ISO, import dell’account e documenti, ripristino CoW, reinstallazione da Recovery con hash dell’intera partizione dati invariato: PASS.
- 43 snapshot, una Copia locale, paginazione con rotellina + clic su Avanti/Indietro e con tastiera, verifica reale dello snapshot ID 33: PASS.
- Apertura della Copia locale e dello snapshot della terza pagina nel Finder; i file congelati restano originali mentre il file live è diverso: PASS.
- Voce Snapshot visibile alla dimensione predefinita e navigazione reale al catalogo `/.snapshots`, controllata con font effettivo e OCR: PASS.
- 931 input della build confrontati e invariati, hash ISO/app in `artifacts.json`. Estratti lossless delle schermate con hash della fonte e rettangoli in `screenshots.json`; nessun montaggio o ritocco.

Le prove intermedie sono supersedute dalla build finale: l’ispezione delle schermate ha rilevato la riga Snapshot nascosta per pochi pixel e un falso positivo del matcher su un font numerico. Il test ora richiede il font effettivo del testo, seleziona la finestra root corretta e controlla anche con OCR il percorso aperto dalla sidebar. I fixture da 43 snapshot sono aggiunti dall’host a un disco già installato tramite la GUI: non vengono presentati come creazione di 43 snapshot da parte del guest.

## Limiti del macroblocco

**Non implementate:** esportazione di una copia exFAT e montaggio/esplorazione dei volumi dati utente esterni e dei filesystem NTFS/FAT32/exFAT dalla sidebar. Il riconoscimento della firma filesystem non è un driver di mount. La vista snapshot del disco montato non viene spacciata per supporto ai volumi esterni.

Rimane vincolante «Esporta una copia exFAT», preservando originali e snapshot. Nessun pulsante viene rinominato per simulare un’operazione non disponibile. Nessun passaggio al macroblocco 3, nessuna release e nessun incremento di versione.
