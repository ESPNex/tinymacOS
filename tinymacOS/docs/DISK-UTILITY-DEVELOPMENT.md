# Utility Disco, CoW e OpenCore — revisione 10.2.3

La revisione conserva il formato proprietario `TMCBFS1` e aggiunge snapshot **Copy-on-Write reali**, resize nativo a capacità variabile e il motore NTFS freestanding. Non è una conversione ad APFS Apple o Btrfs Linux; `TINYFS1` resta soltanto import legacy. Note complete e precauzioni: [NOTE-10.2.3.md](NOTE-10.2.3.md).

## Implementazione

- `storage/native_cow.c`: mappe iniziali senza copie complete, vecchi settori condivisi fra tutti gli snapshot interessati, conteggi ricostruiti dalle mappe, riuso del pool all’ultimo riferimento, journal/staging e replay delle scritture interrotte. Import legacy tramite permutazione in-place. I/O esclusivamente attraverso il dispositivo del volume autenticato da FileVault.
- `storage/native_resize.c`: geometria `TMCGEO3` nel campo 8–32 MiB, slot allocabili e dati effettivamente variabili, ricollocazione di mappe/pool, immagini legacy e backup. Riduzioni incompatibili vengono rifiutate prima delle scritture; i marcatori delle operazioni interrotte impediscono un montaggio silenzioso, senza promettere rollback.
- `storage/ntfs_resize.c`, `compat/ntfs/`, `vendor/ntfs-native/`: motore originale ntfsresize/libntfs-3g 2022.10.3 con patch Debian 5+deb13u3, namespace privato, runtime freestanding, arena recuperata anche in errore e adapter limitato alla partizione. Nessun eseguibile o syscall Linux nel kernel. Sorgenti GPL, licenze e provenienza riproducibile inclusi.
- Backend unico per Utility Disco, `diskutil` e Recovery: anteprima senza scritture, digest e selezione ricontrollati, gate filesystem, flush/rilettura. L’intero disco montato è protetto. Le modifiche offline invalidano cache e chiavi FileVault del disco interessato.
- Creazione nativa nel gap selezionato di GPT/MBR valida; eliminazione di primarie, contenitori estesi e logiche EBR; resize FAT32 limitato alla coda libera e alla capacità della FAT esistente, senza rilocazione.
- Immagine raster dell’installer con alpha reale; provenienza/SHA-256 in `assets/icons/installer-generated.json`.

## UEFI senza ricorsione OpenCore

Il primo avvio installato, dopo verifica di kernel e sistema, registra una voce dedicata `tinymacOS OpenCore` verso `\EFI\OC\OpenCore.efi` e la antepone a BootOrder. Salva il primo ordine precedente in una variabile vendor e non sovrascrive file o variabili Microsoft. Non registra dalla sessione ottica/live; richiede NVRAM scrivibile e può servire selezionare il nuovo volume per raggiungere quel primo avvio.

I file `.contentVisibility` con contenuto `Disabled` accanto a OpenCore e al suo bootstrap impediscono al picker di proporre i propri eseguibili come sistemi operativi. Non disabilitano l’avvio firmware e non nascondono il loader nativo in `\EFI\TMC`. Il filtro è presente nell’ISO, nell’ESP d’installazione e nel payload BIOS. La prima variante senza filtro è stata respinta dalla matrice guest; gli avvii finali non usano selezioni aggiunte dal test per aggirare il ciclo.

## Verifiche riproducibili

```sh
bash tests/test-native-cow.sh
bash tests/test-native-resize.sh
bash tests/test-boot-default.sh
bash tests/test-diskedit.sh
bash tests/test-partitions.sh
bash tests/test-vault.sh
# Strumenti ntfs-3g host indipendenti nel PATH:
python3 tests/test-ntfs-native.py
TMC_NTFS_SANITIZE=1 python3 tests/test-ntfs-native.py
```

Le suite eseguono il codice reale, normalmente e con ASan/UBSan, su RAM/file temporanei: condivisione/reclamation, generazioni, riapertura, pool pieno, errori e settori strappati, migrazione sovrapposta, cifratura reale e autenticazione, resize 32→24→32 MiB e limite allocabile 382 contro 286 nuovi file da 8 KiB.

Le prove guest correnti comprendono `native_install.py`, `native_cow_guest.py`, `ntfs_native_guest.py`, `diskedit_guest.py`, `dualboot_guest.py` GPT/MBR, Recovery BIOS/UEFI, FileVault BIOS anche sul disco misto rilocato e regressioni dell’interfaccia. Il controllo CoW indipendente trova **2 settori storici**, non 12.576 settori di due copie complete. `VALIDAZIONE.json` nella release associa risultati e hash esatto dell’ISO; [VALIDATION-10.2.3-COW.md](VALIDATION-10.2.3-COW.md) riassume la matrice.

## Limiti conservati

- Massimo 384 voci e 8 KiB/file, 8 snapshot e pool limitato; gli snapshot sullo stesso disco non sostituiscono un backup esterno. Resize cifrato non supportato.
- Dopo il resize NTFS è previsto il flag dirty di upstream: controllo `chkdsk` prima di un altro resize, nessuna forzatura nativa. Gli strumenti host usano `-f` solo per la verifica indipendente del risultato.
- Niente nuova tabella da Utility Disco, creazione di logiche o montaggio arbitrario dei volumi separati nel Finder.
- Gli aggiornamenti GPT/MBR, conversioni legacy e resize non promettono atomicità o rollback contro ogni perdita di alimentazione. Fermarsi dopo un errore e verificare esternamente.
- Nessuna certificazione Windows reale, hardware ASRock o audit crittografico indipendente. Le fixture BIOS usano un MBR sentinel dichiarato, non Windows.
