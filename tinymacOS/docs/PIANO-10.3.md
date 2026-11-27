# tinymacOS 10.3 — piano operativo per macroblocchi

Stato: sviluppo avviato dalla 10.2.6/30C1; **non è una release pronta**. Safari/WebKit escluso. Le tre fotografie IMG_20261127_114817256_HDR.jpg, IMG_20261127_114657681_HDR.jpg e IMG_20261127_114637455_HDR.jpg in uploads sono la fonte dei requisiti. Il titolo «10.2.7 changelog» nelle foto è superato dalla richiesta esplicita di versione 10.3.

## Decisioni confermate

- OpenCore sostituisce Windows Boot Manager come avvio principale. Non basta rinominare una voce. Installare il vero bootloader, configurare tinymacOS/Recovery e Windows, aggiornare BootOrder in modo verificabile e predisporre rollback. Conservare i file EFI Microsoft e le altre installazioni; non sovrascriverli né cancellarli. Un eventuale intervento sui file originali richiede autorizzazione separata.
- «Converti in exFAT» significa **esportare una copia** di volume o snapshot su destinazione dedicata. Originale e snapshot conservati; avvertire che la copia è in chiaro se la sorgente è cifrata. Non promettere snapshot CoW/cifratura nativa su exFAT.
- Nessun Safari in questa versione. Conservare sorgenti e risorse esistenti.
- Conservare canonical USB/HID della base funzionante, font forniti, FS2, NVMe/4Kn, Recovery e protezioni GPT/PMBR della 10.2.6. Non confondere un test QEMU con una prova sul PC dell'utente.

## M0 — Baseline, inventario e verifiche ripetibili

Salvare hash dei file iniziali modificati e copia pre-intervento. Registrare attività e risultati separatamente dalle note di rilascio. Nessun cambio di versione pubblica, tag o release prima della chiusura dei gate. Inventario delle stringhe visibili in app, desktop, setup e Recovery; eliminare diagnostica e testo ridondante dalle superfici utente, non cancellare messaggi di errore/consenso necessari né falsificare il formato dei filesystem chiamandoli APFS. Usare terminologia italiana concisa e coerente con macOS dove descrive correttamente il comportamento.

Accettazione: build ripetibile, sorgenti overlay sincronizzati, test delle protezioni USB e disco, niente regressioni sui font. 10.2.6 pubblicata intatta.

**Ritorno esplicito a M0 dopo M2:** completato inventario statico con gate anti-deriva e revisione mirata dei testi; baseline estesa ricostruita dai commit originali, non retrodatata. Verifiche e limiti in `audit-10.3/README.md` e `../../workspace/local-10.3/m0/VERIFICA.md`. M3 non viene avanzato da questo intervento. La ripetibilità richiede confronto effettivo fra due build pulite, non due compilazioni dichiarate equivalenti.

## M1 — TextEdit e interazione con il testo

Correggere ritorno a capo visivo e lettere troncate: larghezza effettiva della finestra, confini UTF-8, nessuna alterazione del file per il solo wrapping. Scroll rotella indipendente dal cursore, click collocato sulla posizione corretta, Home/End sulle righe visive. Ridimensionamento e ridisegno non devono riportare la vista al cursore.
### Precisazione del requisito input

Il requisito principale è il **cursore di inserimento del testo (caret) visibile nel campo attivo, anche mentre si scrive**; non il puntatore del mouse e non soltanto lo spostamento del cursore al click. Renderlo visibile quando il campo riceve il focus, anche vuoto; a ogni inserimento, cancellazione o movimento da tastiera ripristinare immediatamente la fase visibile e poi riprendere il lampeggio. Nessun lampeggio deve lasciare il caret invisibile durante una sequenza di battitura. Non disegnare caret di inserimento nei campi non attivi o di sola lettura. La posizione al click resta un requisito complementare distinto.

Comportamento di riferimento macOS:

- **TextEdit, Note e Memo:** caret di testo normalmente a barra verticale nell'area di modifica attiva, presente anche durante la digitazione. Queste tre app **non sono escluse**: verificare e uniformare il caret già eventualmente implementato, senza aggiungerne uno duplicato. Nelle parti di sola lettura o fuori dall'area di modifica non serve un caret di inserimento.
- **Campi delle altre app, dialoghi, setup e Recovery:** stesso criterio di focus e visibilità. Campi password grafici: caret presente, caratteri mascherati; non mostrare la password in chiaro per rendere evidente la posizione.
- **Terminale:** cursore proprio del terminale nella posizione di input/output corrente, non necessariamente una barra verticale. In macOS Terminale la forma è configurabile (blocco, sottolineatura o barra) e il lampeggio è configurabile. Per tinymacOS prevedere un cursore chiaramente visibile durante la digitazione al prompt; la configurabilità della forma non è un requisito aggiunto. La scelta del punto di modifica con un click non deve essere imposta come in un editor: dipende dalla shell o dal programma in esecuzione. Ai prompt password senza eco non mostrare né lettere né pallini e non far avanzare il cursore a ogni carattere: non deve rivelare la lunghezza della password.

Implementare una gestione condivisa di focus, posizione e fase visibile dove appropriato; rispettare la differenza fra campi grafici, editor e terminale. Non introdurre ridisegni completi continui per il solo lampeggio.

Accettazione: file UTF-8 lungo con/senza newline, round-trip byte-identico, click/scroll/resize, inserimento e cancellazione ai confini delle righe; test host sanitizzati e guest con input reale. Separare a capo visivo da newline realmente presenti nel documento. Per il caret verificare campo vuoto, focus via click/Tab, digitazione continua, pausa/lampeggio, frecce e cancellazione, perdita del focus, password grafiche e input Terminale senza eco. Verificare esplicitamente TextEdit, Note, Memo, Terminale, setup e Recovery. Questa precisazione modifica il piano, non certifica l'implementazione.

**Chiusura M1 (gate locali):** completata la matrice host normale/sanitizzata e guest USB/pixel su setup, Terminale, TextEdit, Note/Memo, Installer e Recovery nativa. File UTF-8 con/senza newline da 141.735/144.000 byte salvati e riaperti con SHA-256 invariato; prompt senza eco verificato anche su FV003 realmente creato dalla GUI. Evidenze e limiti: `../../workspace/local-10.3/m1/VERIFICA.md`. Non chiude M2 né la certificazione hardware/release.

## M2 — Unità, spazio e nomi

Un solo formatter decimale (B/kB/MB/GB/TB) con conversione numerica corretta; non sostituire «MiB» con «MB» lasciando una divisione per 1048576. Correggere doppia unità «KB usati MiB» in Utility Disco. Le dimensioni distruttive in input devono avere unità non ambigue e conversione verificata; migrare insieme validazione, limiti e messaggi, non con ricerca/sostituzione cieca.

Cartella home: visualizzare il nome dell'utente, senza rinominare alla cieca percorsi persistenti e file cifrati. Eliminare duplicazione «copia locale»/snapshot in Utility Disco individuando prima se è la stessa operazione backend.

Spazio non allocato dopo installazione: confrontare tabella GPT riletta, extent FS2, partizione dati e viste dell'interfaccia. Verificare su disco appena installato e dopo cold boot. Correggere cache o allocazione secondo la causa; non assorbire automaticamente gap/partizioni Windows e non fingere che un gap reale sia allocato.

Accettazione: test limiti 999/1000 e 10^6/10^9/10^12 byte, dischi >2 TB/4Kn, partizioni esterne intatte, stato coerente tra Installer, Utility Disco e Terminale.

**Chiusura M2 (gate locali):** ABI/LBA a 64 bit, contenitore v3 oltre 32 bit con lettura v1/v2 preservata, FS2 fino al limite supportato di 16 TiB. Suite host normale/sanitizzata completa; tre guest freschi sulla stessa ISO finale con dischi da 3 TiB (NVMe 4Kn, NVMe 512 B, AHCI 512 B), gap reale da 2,5 TiB, partizioni esterne preservate, avvio nativo e Recovery senza ISO e confronto delle tre viste PASS. M1 ricollaudato sulla stessa build. Evidenze, riproduzione e limiti: `../../workspace/local-10.3/m2/VERIFICA.md`. Non è certificazione Windows/hardware né release; M3 resta il prossimo macroblocco, non svolto qui.

## M3 — Monitoraggio CPU e memoria reali

Audit della provenienza di total/free, core_ticks/core_busy, tick processo e scheduler SMP. RAM fisica usata/libera da allocatore, distinguendo riservata, cache e memoria processi laddove disponibili. CPU per intervalli con denominatore corretto per core e totale, non conteggi cumulativi o percentuali inventate. Nessuna rimozione cosmetica della parola «stima» per nascondere limiti dei contatori.

Accettazione: idle, carico su un core, carico multicore, sleep e allocazione/rilascio; campioni senza underflow o salti di overflow, dati e limiti documentati. La percentuale rimane necessariamente una misura su una finestra temporale.

## M4 — Desktop, scorciatoie e prestazioni

Creazione cartella dal desktop con dialogo diretto senza aprire Finder. Trascinamento e posizionamento libero delle icone desktop, persistenza e collisioni, spostamenti file distinti dal solo riordino. Scorciatoie stile Mac adattate a tastiera Windows italiana: tabella unica, modificatore scelto e mostrato nei menu, accesso Recovery alternativo a F12 verificato e documentato prima di togliere quello esistente. Controllare AltGr e caratteri italiani.

Profilare Mission Control, libreria app, trascinamento finestre e animazioni dell'intero sistema. Ridurre composizioni e danni inutili senza perdere click/tasti o abbassare arbitrariamente l'interattività. Sistemare pannello cronologia della calcolatrice leggermente staccato a destra dal contorno.

Accettazione: prove con molte finestre, tick/tempo/frame e latenza input prima/dopo, nessuna stima spacciata per benchmark GPU; drag rilasciato fuori finestra, resize, menu e dialoghi funzionanti.

## M5 — Volumi e copia exFAT

Rendere esplorabili in Finder i volumi supportati NTFS, FAT32 ed exFAT, con sidebar, rilevamento mount/unmount, etichette e permessi. Audit prima dei driver esistenti: lettura/scrittura non si presume dal solo riconoscimento del tipo. Read-only quando il backend non garantisce scrittura sicura, NTFS ibernato/dirty o volumi cifrati non sbloccati.

Sostituire «Copia dati» con il flusso di esportazione exFAT solo quando esiste realmente: sorgente volume/snapshot, selezione destinazione, spazio richiesto, avviso su formattazione/dati in chiaro, conferma, progressione, annullamento, verifica contenuti e flush. Escludere dischi/partizioni sorgente e sistema dalla formattazione. Non abilitare un pulsante che non svolge l'operazione.

Accettazione: alberi UTF-8, file vuoti/grandi >4 GB, spazio insufficiente, interruzione, collisioni e verifica su un'implementazione exFAT indipendente; lettura Windows da certificare su Windows reale. Originale e snapshot byte-identici.

## M6 — OpenCore e video UEFI reale

Integrare una versione precisa di OpenCore con provenienza, licenza, hash e configurazione minima. Verificare compatibilità del caricatore EFI tinymacOS e Recovery. Gestire ESP esistenti, backup configurazione e BootOrder, voce OpenCore principale, chainload Windows, riavvio e ripristino. Non usare il nome «OpenCore» per il caricatore interno esistente e non promettere Secure Boot senza firme e prove.

Video: audit GOP QueryMode/SetMode e filtri lato boot/kernel/UI. Caso utente GeForce 310 HDMI in UEFI nativo: visibili solo 1024×768 e 800×600. Enumerare e registrare modalità davvero offerte, EDID se accessibile, stride/pixel format/framebuffer; non inventare risoluzioni non esposte e non promettere driver NVIDIA accelerato. Identificare se la limitazione è firmware GOP o filtri nostri; OpenCore non garantisce da solo nuove modalità.

Accettazione: OVMF e BIOS conservato, Windows chainload in fixture, Recovery da scorciatoia e disco installato, installazione e rollback ripetuti; prova finale sul PC GeForce 310 necessaria.

## M7 — Integrazione e release 10.3

Eseguire suite host normali e sanitizzate, matrice guest BIOS/UEFI/USB canonical, installazione FS2/4Kn, GPT/PMBR, Recovery e cifratura. Aggiungere guest per testo, desktop, volumi, exFAT e OpenCore. Una sola ISO BIOS+UEFI con font corretti in live/installato/Recovery; audit contenuto, hash, sorgenti corrispondenti, changelog solo di funzioni effettive. Download/readback della release e ricevute.

**Gate di rilascio:** tutti i macroblocchi implementati e verificati, oppure riduzione di ambito approvata esplicitamente. Nessuna ISO intermedia presentata come 10.3 completa. Le prove hardware non eseguibili qui vanno elencate come mancanti, non PASS.

## Ordine di lavoro e stato

1. M0 inventario → M1 testo → M2 formatter e stato dischi.
2. M3 contatori → M4 interazione/composizione.
3. M5 volumi/exFAT e M6 OpenCore/GOP, ciascuno con gate propri.
4. M7 integrazione, candidatura e pubblicazione.

Stato dettagliato ed evidenze: `workspace/local-10.3/progress.json`. La presenza di questo piano non costituisce implementazione dei macroblocchi.
