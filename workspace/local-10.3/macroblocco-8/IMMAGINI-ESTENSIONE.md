# M8 — rimozione limiti immagini: implementazione in corso

## Regola di accettazione

Richiesta attiva: rimuovere **una per una tutte le limitazioni funzionali** del precedente documento, poi test finale e commit. RAW/IMG/ISO9660/DMG compressi, anche >128 MB, HFS/HFS+/HFSX/APFS in sola lettura; cifratura/password escluse.

**M8 NON concluso; supporto universale DMG/APFS NON raggiunto.** “Implementato” nelle tabelle significa codice scritto e compilato, NON una verifica funzionale. Non sono stati avviati nuovi test host/sanitizer/guest dell’estensione. I PASS storici appartengono al vecchio backend.

Main pubblicato invariato: `2c3146adc4c0c3e128891005f6f13d125a8c0a4d`. Nessun nuovo commit/push, release, backup o deployment. La vecchia ISO candidata non contiene questa estensione e non è distribuibile come risultato corrente.

## 1. Limiti numerici e sorgenti

| Limitazione precedente | Intervento nel codice | Accettazione |
|---|---|---|
| Caricamento integrale e cap totale 64/128 MiB | File backing posizionale, cache sorgente/decodificata da 64 KiB, niente copia integrale dell’immagine | Da fare |
| Indirizzamento immagini limitato a 16 TiB | Range di byte u64 a settori interi; `partition_scan_image` separato dalla policy dei dischi fisici scrivibili | Da fare |
| Due immagini simultanee | Registro immagini collegato dinamicamente, enumerazione `IMAGE_NEXT` | Da fare |
| Otto slot comuni per immagini | Registro block-device estensibile a pagine, slot immagini scollegati riutilizzabili; tombstone fisici non riutilizzati | Da fare |
| 16.384 run DMG | 128 run inline più radix tree di pagine allocate a richiesta; accesso u64, heapsort e ricerca binaria | Da fare |
| XML massimo 16 MiB | Parser XML/base64 in streaming, bounds sulla regione dichiarata senza tetto XML provvisorio | Da fare |
| Finestra input LZFSE massima 1 MiB | Input iniziale 64 KiB, crescita per il blocco FSE codificato richiesto; storia 256 KiB e output a finestre | Da fare |
| Dizionario XZ/LZMA2 massimo 4 MiB | Intero range uint32 dichiarabile dal formato; esaurimento RAM riportato come errore | Da fare |
| Solo bootfs/TMCFS2 di avvio | `ImageSource`: reader privato boot/native/snapshot e viste stabili RO dei volumi esterni; TMCFS2 indipendente, ForeignFs indipendente, ISO/HFS/APFS con stato locale | Da fare |
| FD esterni invalidati da `vol_refresh` | Copia persistente della geometria della partizione, pin sorgente, freeze scritture, guardie unmount/eject e writer preesistenti | Da fare |
| Snapshot rifiutati | Pin snapshot e protezione cancellazione/sostituzione della copia locale; guardie ripristino per sorgenti native | Da fare |
| Immagini annidate rifiutate / guardia provvisoria di profondità 16 | Dipendenze verso immagini già pubblicate; guardia di rientro per sorgente contro cicli reali; stack supervisor dedicato da 64 KiB per ogni sorgente, senza ricorsione sullo stack syscall da 16 KiB | Da fare |
| ForeignFs usabile solo FAT32/exFAT/NTFS | Aggiunti FAT12 e FAT16, root directory fissa, entry FAT12 packed, FAT16, FAT32 con selezione FAT attiva | Da fare |
| Espansione NTFS limitata a 64 run | Lettura diretta dei mapping pairs codificati, conservando quelli MFT; nessun array espanso di 64 run o cap provvisorio VCN a 2^40 | Da fare |

`image_memory` usa heap per piccole allocazioni e span di pagine fisiche per workspace grandi. Le allocazioni possono fallire per RAM insufficiente o frammentazione: non sono una garanzia di poter decodificare un dizionario arbitrariamente grande su qualsiasi macchina. L’allocatore e il trampoline di stack richiedono ancora verifica funzionale, contabilizzazione memoria e stress/concorrenza.

La policy di capacità dei filesystem nativi **scrivibili** e dei dischi fisici non è stata allargata per aggirare i loro contratti. Il limite u64 è quello dell’ABI, non una promessa di volumi di tale dimensione collaudati. L’API parser senza allocatore resta utilizzabile solo per mappe inline; il manager reale e il test storico aggiornato usano l’API allocante, con release anche dopo parse fallito.

## 2. Contenitori e codec

| Voce | Stato codice | Residuo |
|---|---|---|
| RAW/IMG/ISO file-backed | Implementato | Verificare >128 MB e >16 TiB con sorgenti sparse/sintetiche |
| UDIF v4 a segmento singolo, multi-blkx XML | Implementato | Compatibilità produttori, offset data-fork tra tabelle e corruzioni da verificare |
| Plist binario `bplist00` | Aggiunto parser posizionale di offset/object/ref table e schema resource-fork → blkx → Data, senza caricare il plist intero | Fixture indipendenti e malformed/cicli/range da verificare |
| UDIF con sole mappe nel resource fork | Aggiunto parser resource map `blkx`, reference a 24 bit e payload `mish` length-prefixed | Fixture indipendenti e bounds da verificare |
| ADC / zlib / bzip2 / LZFSE-LZVN / ULMO XZ-LZMA2 | Implementati | Tutti da verificare sul backend corrente, incluse grandi finestre/dizionari |
| Lunghezza/consumo input/fine stream | Implementati; Adler32 zlib, CRC blocco/stream bzip2, XZ CRC32/CRC64/SHA256 | Negative e truncated fixtures da verificare; ADC/LZFSE non hanno checksum di stream equivalente |
| Checksum UDIF data-fork / blkx / master | **Non implementati come verifica completa del contenitore** | Aperto |
| Segmenti UDIF multipli | **Non implementati** | Aperto: risoluzione segmenti, identità e pin dell’intero gruppo |
| NDIF e ulteriori varianti legacy del contenitore DMG | **Non implementati** | Aperto; non confondere con i filesystem nativi legacy rimossi, che restano vietati |
| Cifratura/password | Esclusa per scelta utente | Non da implementare nel perimetro concordato |

Metadati sovrapposti, overflow, troncamenti e codec sconosciuti devono continuare a fallire esplicitamente. Non eliminare le guardie di corruzione per far risultare un attach riuscito.

Dipendenze originali upstream, pin e SHA-256 per file in `vendor/{zlib-1.3.1,bzip2-1.0.8,lzfse,xz-embedded}/ORIGIN.json`; licenze in `licenses/`. Nessun codice GPL dei reader Apple adottato. `tools/build-image-codecs.py` ricompila decode-only con allocatori nativi.

## 3. Filesystem e namespace

### HFS / HFS+ / HFSX

Implementati: driver RO, wrapper embedded, catalogo B-tree, nomi MacRoman/UTF16 → UTF8, data fork, resource fork accessibile come `file/..namedfork/rsrc`, extents-overflow distinti per tipo fork. Il bootstrap dell’extents file può ora cercare overflow attraverso il prefisso già indirizzabile; rifiuta cicli reali e usa buffer separati per non sovrascrivere nodi esterni parzialmente letti.

Ancora aperti:

- hardlink di file e directory; cartelle private con nomi speciali;
- decmpfs/xattr e compressione dei file, non equivalente alla compressione DMG;
- risoluzione symlink e alias Finder;
- confronto/normalizzazione Unicode completi;
- journal replay non distruttivo o vista coerente di volumi dirty; attualmente tali volumi sono rifiutati;
- bootstrap overflow complessi da validare. La guardia corrente contro catene di dipendenza patologiche non dimostra copertura di qualsiasi frammentazione.

La dimensione dei nodi è validata contro il campo u16 e le potenze di due del formato; il massimo 32 KiB non è una policy sulla dimensione totale dell’immagine. Resource fork di file ancora classificati speciali restano rifiutati: non dichiarare già completo il supporto resource-fork/decmpfs.

### APFS

Implementati: reader APFS v2 RO, Fletcher metadati, checkpoint NX lineari, OMAP fisico `(OID,XID)`, mappe volumi/oggetti, B-tree, inode/directory, data stream, extent e sparse extent espliciti, stream ID clone. Il contenitore espone ora nomi reali dei volumi se rappresentabili e univoci; gli alias stabili `/Volume N` restano validi. Etichette troppo lunghe, duplicate o ambigue usano l’alias, non un nome troncato.

Ancora aperti:

- volumi sealed, hashed/fext tree;
- Fusion e checkpoint descriptor tree;
- snapshot e firmlink;
- xattr-compression e altri file speciali, symlink;
- normalizzazione/case-fold Unicode completi;
- nomi completi nella UI invece del fallback ai 47 byte della directory ABI;
- evitare scansioni integrali ripetute e ottimizzare lookup/read.

L’esposizione di slot NX con volume non supportato deve dare un errore esplicito, non una directory apparentemente vuota. La cifratura resta esclusa.

### FAT / exFAT / NTFS / TMCFS2 / ISO

- Contenuti IMG FAT32 ed exFAT: percorsi RO esistenti, nuovo backend ancora non collaudato. Aggiunto preflight reale con mount/root del reader indipendente, non solo firma.
- FAT12/FAT16: aggiunti anche al reader indipendente delle sorgenti esterne; da verificare root fissa, catene, confini FAT12 e alias.
- NTFS indipendente: rimosso il cap di 64 run, ma **attribute list, compressione NTFS e record oltre 4 KiB restano aperti**. Non è ancora una sorgente NTFS universale. Non restituire ciphertext o dati compressi grezzi.
- TMCFS2: preflight reale mount/root RO aggiunto anche all’attach, rispettando i limiti del formato nativo.
- ISO9660/HFS/APFS: preflight reale dei reader, non solo signature probe.
- Apple Partition Map: inventario RO, nessuno spazio libero/installabile sintetizzato; installer rifiuta APM. Nessuna migrazione o modifica dei filesystem nativi legacy.
- La namespace ABI resta `PATHLEN=144`, nomi Dirent da 48 byte: **limite non rimosso**. Non dichiarare copertura trasparente di tutti i nomi Apple/NTFS.

## 4. Reattività e altri requisiti M8 ancora aperti

- Le finestre decodificate richiedono ancora decodifiche complete ripetute del run per garantire validazione della fine stream: CPU/latency da risolvere, non soltanto RAM.
- `fs_gate` resta IRQ-off. Stack separati e callback fuori da `disk_lock` non rendono automaticamente reattivo l’input durante una grande scansione/decodifica. Non modificare gli handler HID/input per mascherarlo.
- Thumbnail reale del contenuto trascinato ancora da fare; l’icona del tipo esistente è soltanto fallback.
- USB sidebar/Espelli hanno prove storiche, ma la matrice integrata finale USB TMCFS2/exFAT/NTFS/4Kn, stress/fault, live/Recovery/installato e prestazioni resta aperta.
- Inventario nuove stringhe UI, versione finale, release GitHub ISO/checksum, backup indipendente e video solo M8 su Drive: dopo i gate, non Vercel.

## 5. Compilazione effettuata — NON test finale

Log esterno più recente: `/var/tmp/tmc-image8-implementation/compile-link-checkpoint.log`.

- Compilate 17 unità freestanding, inclusi `image_source`, `image_memory`, allocatore kernel e `arch/image_stack.S`; ricompilate le dipendenze codec con i nuovi simboli malloc/free.
- Link **incrementale** del kernel riuscito usando per le unità invariate gli oggetti della precedente build completa. Successivo ricompila/relink dei preflight ForeignFs/TMCFS2 riuscito. Non è una build completa riproducibile del prodotto o una nuova ISO.
- `build.sh` include i nuovi file tramite glob `arch/*.S`, `fs/*.c`, `storage/*.c`.
- Analisi `-fstack-usage` eseguita per source/HFS/ForeignFs/manager; non è prova di corretto cambio stack, guard page o stress guest.
- Adattato il test parser storico all’API allocante e alla release dei run; **non eseguito**. Nuove fixture indipendenti per tutte le estensioni ancora da completare.
- Ultimo ricompila dei mapping sparse NTFS, header, reader Apple, sorgenti e parser completato senza diagnostiche; kernel rilinkato incrementalmente. Fixture host storica compilata ma **non eseguita**. Questo checkpoint include quindi anche gli ultimi ritocchi.

Nessun nuovo test funzionale, ASan/UBSan o guest; nessun commit o push. Workspace circa 70,4 MB, sotto 128.000.000 byte. Oggetti/log esterni, nessuna fixture grande nel workspace.

## Ordine di prosecuzione

1. Chiudere le voci funzionali ancora aperte delle sezioni 2–4, senza cambiare i requisiti o cancellare le limitazioni dalla documentazione come surrogato dell’implementazione.
2. Verificare sorgenti/pin/write freeze, stack dedicati e allocatore, lifetime dei run e rollback; completare fixture indipendenti.
3. Solo dopo il completamento richiesto: test host/sanitizer, build completa, guest live/Recovery/installato, matrice USB, prestazioni e regressioni grafiche/input.
4. Solo con i gate superati: commit/push e successiva pubblicazione 10.3 secondo la richiesta utente.
