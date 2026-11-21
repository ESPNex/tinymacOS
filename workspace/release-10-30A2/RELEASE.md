# tinymacOS 10 · Tramontana — manutenzione 30A2

Sostituzione della **release v10 esistente**, non una nuova versione. Nome ISO: `tinymacOS-10-x86_64.iso`. Release normale/Latest; le quattro precedenti rimangono Pre-release. Download privato, accessibile con account GitHub autorizzato.

## Correzioni

- Versione interna **10**, senza `.0`: Informazioni, Impostazioni → Generali, Terminale (`sw_vers` / `uname`), installer, Recovery e metadati dei bundle/volumi. Build tecnica **30A2**.
- Artwork importato riservato alle applicazioni. Cartelle, documenti, dischi, Libreria e altri simboli di sistema tornano alle risorse procedurali originali. Nessun simbolo App Store usato al posto della Libreria.
- Controlli e categorie di Impostazioni/Centro di Controllo procedurali anche quando condividono un ID con un'applicazione. Le icone delle vere app restano importate; installer Tramontana originale procedurale.
- **Recovery: tutte le icone procedurali, tranne Terminale**, che conserva l'artwork importato. Verifica byte-per-byte delle otto risorse realmente incorporate.
- **Teal e Tramontana distinti**, sia nei selettori sia nelle palette complete chiare/scure: Teal verde, Tramontana blu ardesia. Nessuna modifica a Menta.
- Associazioni esplicite, senza aritmetica sugli ID: **Blu → Prisma; Azzurro → Brezza; Tramontana → Tramontana**. Mantenute le altre associazioni tematiche esplicite.
- Conservati sfondo Tramontana non procedurale, wordmark “macOS” con autentico peso 600 e “tiny” invariato, allineamento mela e titolo ridotto in Recovery, tastiera fisica italiana end-to-end e assenza della diagnostica F11 al login. F11 sulla scrivania live apre ancora l'installer.

## Mouse USB: modifiche e limite di verifica

**L'ultimo riscontro sul PC fisico ASRock Q1900M/J1900 è mouse fermo sia nel live sia in Recovery. La causa sul PC non è accertata e la soluzione su quella macchina NON è confermata.** Il precedente riscontro “funziona in Recovery” è stato ritirato dall'utente.

Questa revisione corregge percorsi generali, senza eccezioni per una singola motherboard:

- Sui controller Intel switchable supportati, se è presente EHCI, mantiene l'instradamento USB2 scelto dal firmware invece di spostare indiscriminatamente tutte le porte USB2 su xHCI. L'abilitazione SuperSpeed resta limitata alle maschere supportate; senza EHCI resta il percorso xHCI previsto.
- Un rifiuto `SET_PROTOCOL` non elimina più un mouse HID Boot già nello stato richiesto se `GET_PROTOCOL` conferma realmente quello stato. La gestione è coerente in xHCI, percorso HID USB2 condiviso e accesso USB HID del Recovery.
- Recovery preferisce un descrittore Report validato e permette ripiego Boot solo su interfaccia Boot mouse e dopo conferma del protocollo. Risposte vuote/stato sconosciuto non autorizzano un layout presunto.

I test host usano ASAN/UBSAN e casi positivi/negativi, inclusi STALL, risposta corta, stato sconosciuto, assi indipendenti, maschere di routing e presenza/assenza EHCI. I test QEMU provano il percorso emulato selezionato, **non equivalgono a una prova sull'ASRock**.

## Collaudo

Il gate `collect_qa.py` richiede risultati nuovi legati allo SHA-256 di questa ISO: ricostruzione indipendente identica, risorse del filesystem/atlas/Recovery, dieci combinazioni tema/modalità, icone procedurali nel framebuffer, associazioni sfondi, identità, 17 profili input live e 10 installati senza ISO, app e Recovery con tastiera italiana, Mission Control, installer e ripristino locale sigillato, 29 regressioni kernel e kernel delle sonde identico a quello di produzione. Gli esiti effettivi e i checksum sono in `final-qa.json`, `artifact.json` e `SHA256SUMS.txt`; il deploy è attestato separatamente da `github-release-verification.json`.

Output voluminosi, dischi, ISO e screenshot integrali restano in `/var/tmp`; nel repository si conservano ricevute ed evidenze compatte. I risultati storici 30A1 restano nella directory `release-10.0` e non valgono come test della 30A2.

## Prova supplementare non superata: selezione Recovery ottico BIOS

Il tentativo aggiuntivo di entrare nel Recovery **ottico BIOS** iniettando R non raggiunge il menu nella fixture QEMU. Lo stesso test fallisce nello stesso punto anche con la ISO 30A1 attualmente pubblicata (SHA-256 verificato): è un limite preesistente riprodotto nel confronto, non un nuovo PASS. Non è stato possibile distinguere definitivamente il limite dell'automazione da quello della selezione firmware; **non si dichiara collaudato il mouse nel Recovery ottico BIOS**.

Il gate copre invece Recovery **ottico UEFI** con mouse e tablet USB e Recovery **locale BIOS da disco installato**, con ripristino effettivo e mouse/tablet. Il risultato supplementare negativo, i due hash ISO e i log sono conservati in `supplementary-selector-result.json`. Non è conteggiato fra i test superati e rimane esplicitamente aperto.

## Limiti

Sistema sperimentale con kernel proprio, non Linux/XNU e non macOS Apple. Safari/WebKit reale è sospeso e **non incluso**; le sonde backend non sono un browser. Nessun supporto dichiarato per OHCI, hotplug generale, hub SuperSpeed o HID universale. ISO ottica BIOS/OpenDuet/OpenCore e UEFI; non immagine USB da scrivere con DD. Nessuna conferma fisica implicita nei PASS.
