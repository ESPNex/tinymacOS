# tinymacOS 10.2.6 Tramontana — 30C1

## Base USB confermata dall'utente

L'utente ha confermato che la 10.2.5/30B21 muove correttamente il mouse sullo
stesso PC fisico. Il ripristino del profilo canonical USB/HID è quindi la base
della 10.2.6. Non equivale a certificare ogni periferica o la nuova build fisica.

Rimossi gli adattatori di classe/enumerazione alternativi nativi, i relativi
retry/fallback e le prove specifiche obsolete. Le unità OS xHCI/EHCI conservano
solo i servizi dei controller originali condivisi con canonical. PIT e servizi
PS/2 sono separati: non contengono un secondo percorso USB. La compilazione
include sempre USB core/HID canonical; il controllo dei kernel live e Recovery
rifiuta i simboli del percorso ritirato. I sorgenti upstream fissati da manifest
restano invariati e disponibili per i controller e per la provenienza.
La cronologia Git non è stata riscritta: le vecchie revisioni restano recuperabili.

## Drag-and-drop ripristinato

Desktop e coda input recuperati dal commit della 30B18
`8f586a05c361ba49858dd49197d7c0a4a57d0d8e`, annullando il precedente rollback
alla 10.2.4. Ripristinati drag_ready/drag_deliver, modificatori del puntatore e
budget per-frame di input/IPC. Restano Finder Taglia/Copia/Incolla e appunti
Terminale. Nessuna riattivazione dell'adattatore USB che aveva causato il blocco.

## Installer: MBR protettivo e GPT

La correzione della firma 55AA mancante era già presente nella 10.2.5, ma non
risolveva un MBR protettivo con copertura diversa dalla geometria del disco.
La 10.2.6 distingue il caso: se entrambe le GPT sono valide, identiche e senza
sovrapposizioni, con un'unica voce EE non ambigua, l'installer può normalizzare
la sola lunghezza protettiva con consenso esplicito. Questo include una lunghezza
obsoleta o saturata a FFFFFFFF su un disco più piccolo. Lo scanner non scrive.

Il preflight e l'annullamento restano senza scritture; immediatamente prima del
piano di installazione vengono ricontrollati MBR, due header e array GPT.
L'MBR originale completo viene salvato nel nuovo extent. Vengono preservati
identificativo disco, campi PMBR non coinvolti, voci GPT preesistenti e partizioni
occupate. La firma 55AA è ripristinata soltanto se manca. Come nell'installer
precedente, l'attivazione BIOS installa il menu di coesistenza nel codice MBR.

Restano bloccati GPT discordanti/degradate, MBR ibridi, voci residue, flag di
avvio o LBA iniziale della voce EE ambigui. Non si classificano RAW, MSR o NTFS
come spazio libero. La scrivibilità dei file Windows non certifica questi
metadati. Il nuovo caso è riprodotto su fixture; il preciso contenuto PMBR del
PC dell'utente non è stato acquisito e non si dichiara una riparazione fisica.

Recovery Reinstalla senza un sistema preesistente continua a indicare il desktop
live per una nuova installazione, senza accusare genericamente la GPT Windows.

## Molte finestre

- Il kernel non aumenta la sequenza grafica per pubblicazioni di comandi
  identiche: evita copie/invalidate a cascata dei materiali delle altre finestre.
- Modifiche a geometria, testo, conteggio, titolo o payload raster non sono
  soppresse. In particolare i raster non sono mai deduplicati per puntatore.
- Il refresh periodico dello scanout riusa il canvas già composto: non forza
  il ridisegno di ogni finestra aperta quando la scena è immutata.
- Input e IPC hanno un budget per-frame, senza scartare la coda residua.

Questi interventi riducono lavoro CPU/ridisegno, non il solo consumo RAM.
La composizione rimane software: nessuna promessa di accelerazione GPU o di
prestazioni indipendenti dal numero/contenuto delle finestre traslucide.

## Pulizia e conservazione

Circa 34 MB di confronti generati, firmware state temporaneo e artefatti storici
rimossi dal workspace. Gli archivi storici tracciati sono recuperabili dal commit
pubblicato 30B21; manifest con percorsi e hash in
`workspace/local-10.2.6/workspace-cleanup.json`. Conservati font UI/Mono forniti,
font lock, sfondo Recovery, FS2/4Kn/cifratura, sorgenti Safari/WebKit e ABI native.
Nessuna modifica alla visibilità dei file EFI/Recovery.

## Limiti

Build sperimentale. Backup obbligatorio prima di installare. Test sintetici/QEMU
non certificano un disco Windows reale. ISO non USB-DD ibrida. Safari/WebKit
completo non incluso né presentato come pronto. Risultati e pubblicazione devono
essere letti nella validazione della 30C1, non dedotti dalle vecchie build.
