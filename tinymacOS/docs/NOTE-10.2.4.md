# tinymacOS 10.2.4 Tramontana / 30B15

Pre-release sperimentale. Nuova versione separata: la release 10.2.3 e i suoi asset restano invariati.

## Recovery: passaggio definitivo dalla barra al desktop

Tutti gli aggiornamenti della barra di avvio del kernel passano ora attraverso un unico proprietario protetto dallo stesso lock usato dal primo frame grafico. Al primo `present` viene revocata definitivamente la scrittura della splash. Un aggiornamento tardivo del BSP, anche se il compositor è già partito su una CPU secondaria, non può più ridisegnare la barra sopra la Recovery. La modalità verbose e il log seriale restano disponibili.

Regressione dedicata: codice effettivo del log e rasterizzatore splash, scritture tardive/concorrenziali e verbose in host normale e ASan/UBSan; controllo dei pixel del primo pannello Recovery e dei due secondi successivi in BIOS e UEFI con 1, 2 e 4 CPU.

## Un solo assistente per desktop e Recovery

La seconda voce della Recovery apre lo **stesso eseguibile nativo Ring-3** dell'installer del desktop, con il nome **«Installa tinymacOS Tramontana» nel desktop e «Reinstalla tinymacOS Tramontana» in Recovery**. Nessuna seconda implementazione dell'interfaccia di installazione: stessa finestra e stesso percorso di selezione/conferma/esito.

Dalla scrivania live l'assistente installa nello spazio non allocato. In Recovery seleziona un'installazione nativa con tabelle integre e ripristina **soltanto il sistema** dalla sua copia locale sigillata, verificandolo dopo la scrittura. Documenti, account, FileVault e snapshot non sono inizializzati. Non è un download/aggiornamento online e non consente una nuova installazione su disco vuoto dalla sola Recovery senza payload live. L'operazione resta non atomica in caso di perdita di alimentazione.

Il bundle mantiene un percorso interno senza slash; il nome completo con «/» è un'etichetta visualizzata. Il test del supporto confronta byte per byte gli eseguibili desktop/Recovery e i nomi contestuali nei metadati; il test di installazione verifica il wizard Recovery, la conferma obbligatoria, l'hash invariato dell'intera partizione dati e l'uguaglianza del sistema riscritto con la sorgente locale sigillata.

## Dischi SATA/AHCI fino a 1 TiB

Il vecchio controllo bloccava i dischi oltre **128 GiB** prima di riconoscere GPT/MBR. Utility Disco poteva quindi mostrare «Tabella non valida» anche per una tabella corretta. Il nuovo limite condiviso è **2.147.483.648 settori logici da 512 byte = 1.099.511.627.776 byte (1 TiB)**: comprende i dischi commerciali da **1 TB = 1.000.000.000.000 byte**.

Aggiornati scanner di partizioni, lettori di manutenzione, parser dei comandi `diskutil` e dimensioni inseribili in Utility Disco. Restano obbligatori CRC GPT primario/backup, geometrie coerenti, assenza di sovrapposizioni e spazio realmente non allocato. Non viene ignorata né riparata forzatamente una tabella danneggiata. Dischi oltre 1 TiB vengono rifiutati con il limite esplicito.

### Prove specifiche

- Installer e manutenzione effettivi su backend sparso con GPT da 1 TB, GPT da 1 TiB e MBR da 1 TiB: installazione vicino all'ultimo LBA, backup/ripristino nativo, controllo delle scritture e conservazione delle voci esistenti; normale e ASan/UBSan.
- Copia GPT di backup corrotta e capacità superiore al limite: rifiuto prima delle scritture.
- QEMU/AHCI con disco GPT sparso da 1 TiB: riconoscimento della capacità, anteprima da Terminale con LBA oltre il vecchio limite, installazione grafica nel piccolo spazio finale, avvio UEFI dal disco senza ISO, scrittura/lettura di un documento, avvio della Recovery interna e verifica del volume. CRC di entrambe le GPT, voce preesistente e settori sentinella esterni all'installazione verificati indipendentemente.

## Funzioni conservate e limiti

Snapshot **CoW reali**, resize nativo a capacità variabile, FAT32, motore NTFS freestanding, FileVault, Recovery e avvio/coabitazione BIOS/UEFI della 10.2.3. Il font accettato resta invariato.

**Il limite del disco non è la capacità del filesystem nativo.** Il volume dati proprietario resta da **8 a 32 MiB**, 64–384 slot, file fino a 8 KiB e fino a 8 snapshot. Non è Apple APFS né Linux Btrfs. Resize del volume cifrato non supportato; migrazione/resize interrotti sono fail-closed, non rollback riprendibili. Le scritture delle tabelle non sono atomiche rispetto alla perdita di alimentazione.

SATA/AHCI e settori logici **512 B**; niente supporto NVMe, dischi 4Kn o capacità oltre 1 TiB implicito in questa modifica. Il controllo conservativo di un disco completamente vuoto legge ancora tutti i settori: su 1 TB può richiedere molto tempo e non è stato simulato integralmente nel test virtuale. Su GPT/MBR si controlla soltanto l'estensione selezionata realmente non allocata. Sono necessari almeno 96 MiB contigui per una nuova installazione.

Dopo resize NTFS è richiesto il controllo da Windows. Non sono certificati hardware fisico, Windows reale o sicurezza crittografica indipendente. L'avvio BIOS usa OpenDuet/OpenCore; l'ISO non è un'immagine USB-DD ibrida.

**Prima di qualsiasi installazione, partizionamento, cifratura o resize: backup esterno verificato.** Una «tabella non valida» su un disco entro il nuovo limite può indicare un vero errore: non inizializzarlo per tentativi se contiene dati da conservare.

I risultati finali e l'hash dell'ISO sono nell'asset `VALIDAZIONE.json` della release.

## Regressione finale

**34 gruppi PASS**, sulla stessa ISO BIOS/UEFI con nomi contestuali Installa/Reinstalla. [Matrice, prove e limiti](VALIDATION-10.2.4.md). Gli asset della release includono `VALIDAZIONE.json` con hash e risultati completi.
