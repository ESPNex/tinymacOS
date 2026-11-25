# Dischi e BTRFS nativo

## Formato dati

Il volume dati corrente usa **BTRFS nativo (subset)**, implementato in `fs/btrfs.c`: intestazione `TMCBFS1`, tabella dei nodi, dati, superblocco con magic Btrfs e CRC32C, catalogo degli snapshot e copia locale. È un formato proprio di tinymacOS: **non è un’implementazione completa del formato Btrfs Linux** e non va montato o riparato con `btrfs-progs`.

Il lettore mantiene la compatibilità di importazione con il formato legacy v1. Le sue firme binarie non vengono rinominate: cambiare una firma non converte un filesystem e renderebbe illeggibili vecchi dati e fixture. I documenti sotto `docs/storico/` restano evidenza delle versioni corrispondenti.

I limiti attuali sono quelli definiti da `NSLOTS`, `NPART`, `NDATA` e dal formato dei nodi; aggiungere capacità alla partizione non aumenta automaticamente il numero o la dimensione massima dei file.

## Layout del disco

La 10.2.3 supporta il layout dedicato su disco interamente vuoto e la destinazione nativa separata di 96 MiB nello spazio non allocato GPT/MBR. Quest’ultima contiene prefisso FAT16 di avvio, dati BTRFS nativi, Recovery, sorgente di ripristino e payload BIOS. Vedere `NOTE-10.2.3.md` e `include/container.h` per geometria e vincoli.

L’inventario distingue FAT12/16/32, NTFS, exFAT, BTRFS nativo, magic Btrfs esterno e RAW/non riconosciuto. Il riconoscimento è in sola lettura e non certifica l’integrità del filesystem. Una partizione RAW non è spazio non allocato.

## Namespace e protezione

Il catalogo sigillato espone risorse immutabili sotto `/System`, `/Applications`, `/bin`, `/sbin` e `/usr`. I dati utente e le preferenze usano il profilo `/Users/User`; nella sessione live il volume dati è in RAM. Finder e Terminale vedono lo stesso namespace.

FileVault cifra il volume dati nativo, inclusi nomi, metadati, account, snapshot e copie locali. EFI e sistema di Recupero restano pubblici. La sorgente di reinstallazione e la copia dei dati sono distinte. Nessuna copia sullo stesso disco protegge da un guasto fisico.

Un ridimensionamento sicuro deve modificare prima il filesystem e poi i confini della partizione per una riduzione, nell’ordine inverso per una crescita. Non è sufficiente cambiare la tabella GPT/MBR. FAT32 e NTFS richiedono motori specifici e controlli sui metadati, sullo stato del volume e sulle allocazioni da spostare.
