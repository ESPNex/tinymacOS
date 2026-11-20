# tinymacOS 2.0.2 Riviera

**Build 20A3 · ISO UEFI x86-64 · supporto unico con BaseSystem integrato**

**[Scarica la ISO da Google Drive — 51,63 MiB](https://drive.google.com/uc?id=1KMBbKetxhhZgVnsp7bL4GCBTDI9vrSCy&export=download)** · [Checksum](tinymacOS-2.0.2-SHA256.txt)

Serve **solo questo file**: niente secondo download, niente immagine USB separata. Le release 2.0, 2.0.1 e 1.3 su Drive sono rimaste invariate.

## Il cambiamento di questa versione

Nella 2.0.1 il Recupero sul disco era i settori grezzi di una FAT da 4 MiB e il supporto opzionale era una seconda immagine. Nella 2.0.2 il modello è quello dei Mac Intel studiato nelle [note di sicurezza Apple](https://support.apple.com/guide/security/recoveryos-and-diagnostics-environments-sec2512a0c09/web): esiste **un file `BaseSystem.dmg`** e la stessa copia vive prima nel supporto di installazione e poi sulla partizione interna.

1. **Nella ISO**: `/Recovery/BaseSystem.dmg` (3.147.889 byte, UDIF non compresso, SHA-256 `a3abd61b…cd22c6f`) è un file vero, visibile sia da ISO9660 sia dalla FAT di avvio El Torito. Contiene esclusivamente il runtime UEFI di Recupero.
2. **Sul disco dopo l’installazione**: l’assistente costruisce la partizione Recovery FAT16 da 4 MiB e vi scrive quel **file**, identico byte per byte (verificato estraendolo da entrambi i media). Non è un’immagine grezza e non serve la ISO per riaverlo.
3. **All’avvio**: premi **R durante la mela** — dal CD o, dopo l’installazione e rimosso il CD, dal disco. Il loader verifica l’intero DMG (SHA-256 incorporato) e lancia il programma che contiene. Se il file mancante o alterato, il messaggio di errore non tocca il disco.

Nella shell di Recupero:

```
diskutil info /
```

mostra `Immagine: /Recovery/BaseSystem.dmg`, l’origine (`ISO di installazione` oppure `disco interno, partizione Recovery`, lette dal device path UEFI usato per aprire il file) e i `byte verificati`.

## Avvio in QEMU

Scegli **Q35 x86-64, UEFI/OVMF senza Secure Boot, 512 MiB di RAM**, disco SATA vuoto dedicato (96 MiB), CD con la ISO e unico puntatore `usb-tablet` accanto alla tastiera USB:

```sh
-device qemu-xhci,id=xhci \
-device usb-kbd,bus=xhci.0,port=1 \
-device usb-tablet,bus=xhci.0,port=2
```

`./run.sh` del progetto configura già questo profilo; `TMC_POINTER=mouse` sceglie il mouse relativo. Durante la mela premi più volte **R** (o F12) per il Recupero. Dopo l’installazione: spegni, rimuovi la ISO, riavvia col solo disco (`BOOT_DISK=1 ./run.sh`) e premi di nuovo R.

## Cosa è stato verificato

- **Installazione completa dalla sola ISO**: account, installer da desktop, GPT primaria/secondaria, dati reali; il disco installato contiene `/Recovery/BaseSystem.dmg`.
- **Confronto diretto**: il file estratto dalla ISO e il file della partizione Recovery sono **identici byte per byte**; `dmg2img` indipendente decodifica lo stesso volume FAT16.
- **Recupero locale senza CD e senza rete**: NVRAM nuova, nessun supporto aggiuntivo collegato; R → shell → `diskutil info /` riporta origine disco e 3147889 byte verificati.
- **Recupero dalla sola ISO**: nessun disco SATA, nessuna USB storage; origine «ISO di installazione».
- **Rifiuto dell’immagine danneggiata**: un solo byte alterato nel metadato UDIF → errore UEFI, nessuna scrittura, nessun fallback nascosto.
- **Reinstallazione offline e dati**: SOS, ripristino copia, reinstallazione che conserva i documenti, disco di avvio, inizializzazione con conferma.
- **Puntatore**: 12 gruppi tablet/mouse compresi i clic in Recupero; 39 gruppi di regressione del sistema; console con un unico download ISO.
- **Riproducibilità**: due build indipendenti con **9 artefatti byte-identici**, ISO compresa; su Drive esiste **un solo file 2.0.2**, con MD5/SHA-256/locali identici e download verificato.

## Limiti

Nessun componente Apple, nessuna firma o Secure Boot Apple, nessuna compatibilità con macOS o APFS, nessun Command-R fisico. Il profilo DMG accettato è quello di questa build (UDIF non compresso con digest incorporato), non un decodificatore generico. Recovery HD da 4 MiB: il bootstrap occupa ~10 KiB, il DMG 3 MiB, il resto è padding. Non usare dischi fisici con dati. Il riferimento al Mac è di **design**, non un supporto di esecuzione.

[Guida di avvio](../docs/QUICKSTART.txt) · [Architettura](../docs/ARCHITECTURE.md) · [Collaudo completo](../docs/VALIDATION.md) · [Evidenze](../tests/evidence/riviera-2.0.2/)
