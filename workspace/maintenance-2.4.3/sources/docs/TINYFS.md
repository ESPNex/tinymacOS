# Dischi di Riviera

Settori logici da 512 byte. Installer: disco SATA da **64 MiB a 128 GiB**, dedicato e interamente a zero. Nessuna sovrascrittura forzata, riduzione o partizionamento di un disco preesistente. Q35/OVMF collaudato; non usare su un disco fisico con dati. Il disco consigliato per una VM è da 96 MiB.

## GPT e immagini

Con `Z` numero di settori, `S = Z - 33 - 40960 - 1`, `R = S - 8192`:

| Settori inclusi | Contenuto |
|---|---|
| 0; 1 e 2–33 | protective MBR, header GPT primario e tabella |
| 2048–43007 | ESP FAT16 da 20 MiB: boot.efi (percorso firmware EFI/BOOT/BOOTX64.EFI), KernelCache e catalogo installato |
| 43008–(R−1) | Macintosh HD - Dati: TinyFS e spazio per una singola copia locale verificata |
| R–(S−1) | Recovery HD, FAT16 da 4 MiB: `/EFI/BOOT/BOOTX64.EFI` (bootstrap) e `/Recovery/BaseSystem.dmg` (immagine integrata, byte identici al file sulla ISO) |
| S–(Z−34) | sigillo `TMCSYS20` con GUID, misura e digest, poi una copia pulita della sola ESP installata |
| Z−33–Z−1 | entry e header GPT di backup |

Il settore relativo 8 della ESP è riservato per la richiesta una tantum di riavvio in Recupero; non è un file di debug. Il loader consuma e cancella il flag. La geometria e i sigilli sono controllati prima delle operazioni di manutenzione. Una sola GPT valida può servire al SOS per riparare l’altra. Su una destinazione sconosciuta la recovery non tenta di indovinare gli offset. Un errore di scrittura o l’interruzione dell’alimentazione **non garantiscono rollback**; lavorare su copie.

## Volume dati (riferimenti relativi a LBA 43008)

- **0:** magic `TINYFS1\0`; u32 versione 1, 96 nodi, 16 settori per nodo, stato di attivazione. **1–12:** 96 nodi da 64 byte (`kind`, `read_only`, `parent`, `length`, `generation`, `name[48]`, campo data DOS usato da `touch`). Directory radice: nodo 0.
- **13 e 14:** record account privati con verificatore e controllo SHA-256. Non sono accessibili come file. **15:** marcatore di re-inizializzazione esplicita per il primo avvio dopo «Inizializza» in Recupero.
- **16 + id×16:** estensione fissa di 8.192 byte per nodo; la lunghezza logica decide il contenuto visibile. Creare un file non richiede un processo speciale. Un’apertura in scrittura da sola non tronca; una scrittura con un descriptor nuovo sostituisce il documento. Eliminazione e riuso del nodo invalidano i descriptor obsoleti.
- **4095 e 4096–5647:** sigillo `TMCSNAP2` e copia dei primi 1.552 settori. Il salvataggio invalida il sigillo precedente, scrive e rilegge i dati, e solo alla fine pubblica il nuovo sigillo. In Recupero la copia viene verificata *prima* del ripristino. «Inizializza» cancella anche la copia dei dati, non la sorgente del sistema.

Il CD, senza altri dati, usa lo stesso formato in RAM. L’installatore prende un’istantanea coerente della sessione al momento della copia. Un volume TinyFS precedente riconosciuto viene importato in RAM dal CD prima della migrazione, non modificato sul disco d’origine. I sorgenti e gli eseguibili di sistema non occupano slot utente.

## Namespace

`/System`, `/Applications`, `/bin`, `/sbin` e `/usr` sono montati dal catalogo in sola lettura (compresi bundle, font, immagini e strumenti). `/Library`, `/Users/User`, `~/Documents`, `~/Desktop`, `~/Library/Preferences` e `~/Library/Application Support` sono cartelle reali del volume dati. Il profilo singolo usa `/Users/User` come percorso stabile, indipendentemente dal nome di accesso. `com.tinymacos.appearance` conserva tema, sfondo, posizione Dock, movimento ridotto e materiali opachi. Le app hanno lo stesso namespace: nessun archivio fittizio per Terminale o Finder.

`/Volumes/Macintosh HD` sul disco e `/Volumes/tinymacOS Live` sul CD risolvono alla radice montata, non a duplicati dei file. Lunghezza massima del percorso: 143 byte; componente: 47. I nomi distinguono maiuscole e minuscole. Niente APFS/HFS+, journaling, ACL, FileVault o hard link. La dimensione fisica del disco non cambia gli attuali limiti di 96 nodi e 8 KiB per documento. Conservare inoltre una **copia esterna** del disco raw a VM spenta.
