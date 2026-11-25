# tinymacOS 10.2.3 Tramontana · 30B14

## Aggiornamento: snapshot CoW reali e Utility Disco

Questa revisione sostituisce gli asset della 10.2.3, senza cambiare il numero di versione. ISO, sorgenti e `VALIDAZIONE.json` devono appartenere allo stesso commit e agli stessi hash pubblicati. Le release 10.2.2 e 10.2.1 restano invariate.

### Snapshot Copy-on-Write

- La creazione scrive mappe e checksum: **i dati iniziali restano condivisi**, non vengono copiati integralmente per ogni snapshot.
- La prima modifica conserva il vecchio settore una sola volta per tutti gli snapshot che ne hanno bisogno. Generazioni successive conservano versioni diverse; l’eliminazione libera soltanto i blocchi senza più riferimenti.
- Le viste `/.snapshots/<nome>/` restano di sola lettura. Il ripristino di uno snapshot mantiene gli altri punti storici. Finder, Terminale, Utility Disco e Time Machine usano lo stesso backend.
- Journal e settore di staging consentono il replay delle scritture interrotte. Mappe/checksum incoerenti vengono rifiutati; un pool pieno rifiuta la nuova scrittura senza cambiare il settore attivo.
- FileVault cifra e autentica anche mappe, journal e blocchi storici. La copia locale indipendente di Recovery resta distinta dagli snapshot e il suo ripristino li preserva.
- Le vecchie immagini snapshot vengono migrate senza una seconda copia completa del volume. Migrazione legacy e resize interrotti hanno un marcatore di rifiuto del mount, **non un rollback automatico**.

Il formato è proprietario `TMCBFS1`, con presentazione ispirata ad APFS: **non è Apple APFS né Btrfs Linux interoperabile**. Non usare `btrfs-progs` per ripararlo. Restano massimo 8 snapshot, 384 voci complessive e 8 KiB per file; il pool è limitato e gli snapshot sullo stesso disco non sono un backup contro guasti fisici. Dettagli: [NATIVE-COW.md](NATIVE-COW.md).

### Ridimensionamento effettivo del filesystem nativo

Il motore modifica capacità allocabile e tabella dei nodi entro **8–32 MiB**, non soltanto il confine della partizione. Ad esempio 32→24→32 MiB cambia 384→288→384 slot, ricollocando mappe/pool e backup senza materializzare gli snapshot CoW come copie complete. Rifiuta riduzioni incompatibili con dati o snapshot ancora necessari. Il resize di volumi cifrati non è supportato.

### Editor condiviso e FAT32/NTFS

- Utility Disco ha pannelli per creazione, eliminazione e resize, anteprima delle dimensioni e conferma distinta. Eliminare richiede `ELIMINA`. Non è una cancellazione sicura dei dati.
- GUI, `diskutil` e Recovery usano lo stesso backend: selezione e digest dei metadati riletti prima di scrivere, I/O limitato alla partizione, flush/rilettura e protezione dell’intero disco attualmente montato. Le modifiche vanno eseguite da Recovery o da un altro supporto.
- FAT32 primaria: riduzione con coda libera e crescita entro la capacità della FAT esistente, aggiornando BPB/backup/FSINFO. Nessuna rilocazione dei cluster o conversione FAT16/FAT32; dirty, metadati discordanti e allocazioni fuori limite sono rifiutati.
- **NTFS nativo**: port freestanding originale di ntfsresize/libntfs-3g 2022.10.3, patch Debian 5+deb13u3, con namespace privato e sorgenti/licenze GPL inclusi. Supporta riduzione con rilocazione e crescita nelle geometrie validate, senza processi o syscall Linux. Rifiuta ibernazione, dirty/journal non idonei e corruzioni riconosciute. Dopo il resize resta intenzionalmente impostata la richiesta di controllo: eseguire `chkdsk` da Windows prima di un altro resize, senza forzare il backend.
- Creazione di una partizione nativa nel gap esplicitamente selezionato di una GPT/MBR già valida, con almeno 32 MiB. Eliminazione di primarie GPT/MBR, contenitori estesi e volumi logici EBR. Nessun montaggio arbitrario dei nuovi volumi separati nel Finder, né creazione di partizioni logiche o nuova tabella da Utility Disco.

Senza `--confirm`, `diskutil addPartition`, `deletePartition` e `resizeVolume` eseguono soltanto l’anteprima. Nessun aggiornamento GPT/MBR o resize è garantito atomico contro ogni perdita di alimentazione; errore dopo l’inizio delle scritture non significa rollback.

## Installer e avvio

L’installer a pagina unica usa ora l’immagine raster con alpha reale. Installa su disco verificato interamente vuoto oppure in almeno **96 MiB contigui non allocati**, con voce GPT/MBR libera. **RAW non significa spazio libero.** L’installer non riduce automaticamente partizioni preesistenti.

L’inventario condiviso riconosce FAT12/16/32, NTFS, exFAT, nativo e RAW. GPT richiede entrambe le copie valide e concordanti, senza ibridi o sovrapposizioni; MBR riserva l’intera estensione delle partizioni estese. Il riconoscimento non certifica l’integrità del filesystem.

La destinazione contiene avvio FAT16, dati/FileVault, Recovery, sorgente di ripristino e OpenDuet. Le partizioni preesistenti, i loro identificatori, le estensioni, la firma disco e i flag attivi sono conservati; tabella delle partizioni e codice MBR vengono invece necessariamente aggiornati. Il vecchio MBR è salvato nella nuova destinazione, fuori dalle strutture FAT.

- **BIOS/Legacy:** menu `1 Previous system` / `2 tinymacOS`. La prima scelta esegue il vecchio MBR conservato e richiede un precedente avvio Legacy valido; non converte Windows UEFI in Legacy.
- **UEFI:** il primo avvio verificato della destinazione registra una voce dedicata `tinymacOS OpenCore` verso `\EFI\OC\OpenCore.efi` e la antepone a `BootOrder`, conservando il primo ordine precedente in una variabile vendor. Non sovrascrive i file o le variabili Microsoft. Serve NVRAM scrivibile; per raggiungere quel primo avvio può ancora occorrere scegliere il nuovo volume dal firmware. Il fallback rimane `\EFI\BOOT\BOOTX64.EFI` sul volume tinymacOS. Secure Boot disattivato.
- I filtri `.contentVisibility` impediscono a OpenCore di proporre se stesso come sistema operativo, evitando ricorsioni dopo la registrazione EFI; non disabilitano l’avvio dal firmware né il loader nativo.

FileVault resta integrato nelle Impostazioni senza pulsante Indietro; barra desktop trasparente, Recovery scura/opaca e font desktop/Recovery restano invariati.

## Sicurezza delle prove e limiti

**Backup esterno verificato dell’intero disco obbligatorio prima di installare, migrare o ridimensionare.** Un’interruzione può richiedere riparazioni esterne. Le prove normali, ASan/UBSan, lettori indipendenti e guest BIOS/OVMF sono identificate in `VALIDAZIONE.json` con l’hash esatto dell’ISO. Il test CoW guest confronta anche le mappe reali, non soltanto messaggi sullo schermo.

Limiti della piattaforma: SATA/AHCI, settori logici di 512 byte, dischi fino a 128 GiB e 128 voci GPT. Nessuna conversione MBR/GPT, riparazione generale NTFS/FAT, aggiornamento automatico di una vecchia installazione o ISO USB-DD ibrida. Nessuna certificazione Windows reale, hardware ASRock Q1900M o audit crittografico indipendente; il vecchio sistema nelle fixture BIOS è un sentinel sintetico dichiarato, non Windows.
