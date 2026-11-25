# tinymacOS 10.2.3 Tramontana · 30B14

## Installer e partizioni

- Installer a pagina unica, senza colonna dei passaggi: immagine centrale, destinazione, conferma e avanzamento, ispirato al flusso di installazione macOS. Non è l’installer Apple e non installa macOS.
- Inventario condiviso, in sola lettura, di GPT e MBR: FAT12/16/32, NTFS, exFAT, formato dati nativo e RAW/non riconosciuto. Le firme sono controllate con la geometria del volume; il riconoscimento non equivale alla verifica completa del filesystem.
- Stessi filesystem, dimensioni, identificatori `diskNsM` e LBA in Utility Disco e in `diskutil list`, `diskutil list diskN`, `diskutil info diskNsM` nel Terminale, anche in Recovery.
- **RAW non è spazio libero**: può contenere dati o un formato non supportato. Nessuna partizione esistente viene scelta o inizializzata dall’installer.
- Installazione su dischi GPT o MBR con almeno **96 MiB contigui non allocati** e una voce libera nella tabella. Viene mostrata e usata la più grande estensione libera allineata; non si ridimensionano o spostano partizioni.
- GPT: CRC di intestazioni e voci, entrambe le copie concordanti, limiti e sovrapposizioni controllati. GPT ibride/degradate, dischi dinamici, tabelle piene o strutture ambigue vengono protetti dalle scritture.
- MBR: conservazione delle partizioni primarie e dei flag attivi; inventario dei volumi logici nella catena EBR. L’intera partizione estesa resta riservata: non viene installato nulla nei suoi buchi interni.

## Avvio e protezione dei dati

La nuova destinazione è una singola partizione nativa di 96 MiB, con prefisso FAT16 per l’avvio e aree interne distinte per dati/FileVault, Recupero, sorgente di ripristino e OpenDuet. Non si riutilizza l’ESP di Windows e non si scrive nei filesystem preesistenti. Non è APFS né un container Apple.

L’installer modifica necessariamente la tabella delle partizioni e il codice di avvio MBR: **non promette un disco interamente identico**, ma conserva identici i byte delle partizioni preesistenti, i relativi identificatori/estensioni, la firma disco e i flag attivi. Il vecchio settore MBR completo viene salvato nel settore relativo 10 della nuova destinazione; il descrittore con CRC è nel settore relativo 9, entrambi riservati fuori dalle strutture FAT.

- **BIOS/Legacy:** menu testuale `1 Previous system` / `2 tinymacOS` (Invio). La prima scelta esegue il precedente MBR conservato; non modifica il suo sistema operativo. La seconda legge OpenDuet dalla nuova destinazione. La scelta 1 richiede un precedente avvio BIOS/Legacy realmente valido: non converte un Windows UEFI in Legacy. Per un sistema UEFI si conserva e usa la sua voce nel firmware.
- **UEFI:** avviare `\EFI\BOOT\BOOTX64.EFI` sul **nuovo volume tinymacOS**. Se il firmware continua ad avviare Windows, aggiungere/selezionare questa voce dal suo menu di avvio da file. L’installer nativo non registra automaticamente `Boot####` nella NVRAM e non sostituisce Windows Boot Manager. Secure Boot disattivato.
- Dati, backup, ripristino, Recovery e FileVault usano gli indirizzi della nuova destinazione, non indirizzi fissi nel disco.
- I dischi completamente vuoti mantengono il percorso BIOS/UEFI della 10.2.2; l’intero disco viene letto e verificato vuoto prima della prima scrittura.

## Prima di provare

**Fare un backup verificato dell’intero disco e dei metadati di partizionamento.** Liberare lo spazio dal sistema che gestisce quel filesystem; tinymacOS non riduce NTFS/FAT. Il progetto è sperimentale: i controlli non garantiscono recupero da guasti, rimozione del disco o interruzione durante l’aggiornamento GPT/MBR. Un’interruzione può richiedere la riparazione della tabella/avvio con strumenti esterni, pur senza sovrascrivere le partizioni esistenti.

Limiti attuali: SATA/AHCI, settori logici da 512 byte, fino a 128 GiB e 128 voci GPT. Nessuna creazione di partizioni logiche, conversione MBR/GPT, riparazione NTFS/FAT o aggiornamento automatico di una destinazione tinymacOS già presente. Utility Disco rifiuta le operazioni native su partizioni esterne. Non è certificato il dual boot con Windows reale né l’hardware ASRock Q1900M; le fixture di avvio precedenti sono sintetiche e identificate come tali nei test. La ISO resta ottica, non USB-DD ibrida.

La barra desktop trasparente, Recovery scura/opaca e FileVault integrato in Impostazioni senza pulsante Indietro restano invariati.

## Riferimenti interfaccia e formati

- Apple: https://support.apple.com/en-us/102655 (flusso Recupero → Installa → scelta del volume → avanzamento).
- Microsoft: https://learn.microsoft.com/en-us/windows/win32/api/vds/ne-vds-vds_file_system_type (stati/tipi RAW, FAT32 e NTFS).
- I test e i loro limiti sono pubblicati in `VALIDAZIONE.json`; nessuna verifica virtuale sostituisce il backup o una certificazione hardware.
