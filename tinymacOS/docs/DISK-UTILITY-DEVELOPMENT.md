# Utility Disco — sviluppo successivo alla 10.2.3 pubblicata

**Non è una nuova release né un aggiornamento già distribuito.** La 10.2.3 pubblicata rimane il riferimento; questo branch non soddisfa ancora l’intera richiesta di sostituzione. Non installare una build di sviluppo su un disco con dati importanti.

## Implementato nel branch

- Immagine raster generata per «Installa tinymacOS», PNG con canale alpha reale, importata nell’atlante senza il precedente disegno procedurale. Provenienza e SHA-256 in `assets/icons/installer-generated.json`.
- Distinzione fra il formato nativo proprietario («BTRFS nativo, subset») e una firma Btrfs esterna. Nessuna falsa conversione per cambio del nome.
- Bootstrap UEFI: durante il primo avvio della destinazione installata, **dopo aver verificato kernel e volume di sistema**, crea una voce dedicata «tinymacOS OpenCore» e la antepone a `BootOrder`. La prima lista precedente viene conservata in una variabile vendor; le variabili e i file Microsoft non vengono sovrascritti. La registrazione non avviene dalla sessione ottica/live e richiede NVRAM scrivibile. Per raggiungere quel primo avvio può ancora servire scegliere il nuovo volume dal firmware. Non è una registrazione fatta dal kernel durante l’installazione.
- Utility Disco: pannelli di creazione, eliminazione e ridimensionamento con anteprima, dimensione in MiB e conferma separata. L’eliminazione richiede di digitare `ELIMINA`; il contenitore esteso include i suoi volumi logici.
- Backend comune a GUI e `diskutil`, anche in Recovery: digest dei metadati, confronto della selezione, rilettura prima della scrittura, gate filesystem condiviso. Il disco attualmente montato è protetto interamente: avviare Recupero o un altro supporto. Prima di una modifica offline si invalidano cache del layout e chiavi FileVault di quel disco.
- Eliminazione di partizioni GPT, primarie MBR, contenitori estesi e volumi logici EBR indipendentemente dal filesystem. Non è una cancellazione sicura: i dati rimangono fisicamente presenti finché sovrascritti. La cancellazione di una voce ESP può rendere un sistema non avviabile ed è intenzionalmente distruttiva.
- Creazione di una partizione nativa nel **gap selezionato di una tabella GPT/MBR già valida**, con voce libera, LBA/dimensione allineati a 1 MiB e almeno 32 MiB. Formato attuale a slot fissi: non Linux Btrfs. Il limite dei file non cresce con l’estensione assegnata. Non è ancora disponibile il montaggio arbitrario di questi volumi separati nel Finder.
- Ridimensionamento reale **FAT32 primaria, con coda libera**, senza spostamento di cluster e senza modificare l’inizio dell’area dati. Aggiorna BPB principale/backup, FSINFO, FAT per eventuali cluster aggiunti e tabella. La FAT preesistente deve avere capacità sufficiente; niente conversione FAT16/FAT32, rilocazione o cambio della dimensione dei cluster. Rifiuta volume sporco, copie FAT discordanti, backup non coerenti, cluster allocati/riferimenti oltre il nuovo limite e strutture non supportate.

## Non completato: blocca la sostituzione della release

- **Ridimensionamento NTFS**: manca il motore nativo per bitmap, runlist, MFT/MFTMirr, BadClus, Boot, LogFile, rilocazione e verifica di coerenza. Non si esegue il binario Linux `ntfsresize` nel kernel tinymacOS: ABI ed eseguibili sono diversi. La richiesta viene rifiutata senza scritture.
- Ridimensionamento del filesystem nativo con capacità realmente variabile, anziché il solo confine della partizione.
- Sostituzione del formato proprietario con un filesystem **Btrfs realmente interoperabile**, più migrazione dei dati, snapshot e FileVault. Le firme legacy rimangono leggibili: rinominarle corromperebbe l’importazione.
- Creazione di una tabella su disco non inizializzato direttamente da Utility Disco, creazione di partizioni logiche e montaggio di volumi dati arbitrari.
- Verifica del percorso UEFI su firmware reale e del boot di Windows reale. Le prove unitarie delle variabili EFI sono simulate; una prova guest aggiuntiva ha eseguito davvero installer, avvio nativo e registrazione in OVMF, rileggendo indipendentemente `BootOrder` dal file NVRAM. Nessuna delle due dimostra una certificazione Windows/ASRock.

## Terminale

Senza `--confirm` questi comandi eseguono solo l’anteprima:

```text
diskutil deletePartition disk0s2
diskutil resizeVolume disk0s1 48
diskutil addPartition disk0 135168 32 Dati
```

Il parametro dopo `addPartition diskN` è l’**LBA iniziale**, poi la dimensione in MiB e un nome ASCII. Per applicare, ripetere il comando aggiungendo `--confirm`, dopo un backup verificato. Non usare esempi su un disco reale.

## Prove riproducibili

Dalla directory sorgente, con `TOOLROOT` configurato:

```sh
bash tests/test-boot-default.sh
bash tests/test-diskedit.sh
bash tests/test-partitions.sh
```

Le prove disco usano esclusivamente fixture temporanee sotto `/var/tmp`, mai device fisici. `test-diskedit.sh` usa un filesystem creato da `mkfs.fat`, vi copia un file con `mcopy`, esegue il backend effettivo su GPT e MBR, riduce 64 → 48 MiB e ricresce a 64 MiB. `fsck.fat -n` e `mtype` verificano indipendentemente struttura e contenuto. Si eseguono anche ASan/UBSan, rifiuto della coda allocata, conferma obbligatoria, metadati obsoleti, eliminazione EBR e guasti sulle scritture della tabella.

Le scritture sono seguite da flush/rilettura ma **non sono transazioni resistenti a ogni interruzione di alimentazione**. Una GPT aggiornata a metà è degradata e blocca ulteriori modifiche; richiede riparazione indipendente. Un errore dopo l’inizio di un ridimensionamento non è un rollback. Fermarsi e non usare il volume prima di una verifica esterna.

## Risultato delle verifiche di questo sviluppo

Build completa UEFI riuscita. Le suite host per editor e registrazione EFI passano sia normalmente sia con AddressSanitizer/UndefinedBehaviorSanitizer; passa anche la regressione dell’installer GPT/MBR. La prova guest esegue il nuovo ABI dal kernel, verifica i pannelli GUI, riduce FAT32, rifiuta correttamente NTFS, crea/elimina la partizione nativa, installa nello spazio rimasto libero, controlla i file con lettori indipendenti e avvia la destinazione senza ISO. La NVRAM OVMF contiene poi OpenCore al primo posto e la copia del vecchio ordine.

Questi risultati **non comprendono una nuova matrice completa FileVault/BIOS/Recovery né una pubblicazione della ISO di sviluppo**. Non convalidano funzionalità ancora assenti. Main, tag e asset della release esistente non vanno sostituiti finché i requisiti mancanti non sono completati.
