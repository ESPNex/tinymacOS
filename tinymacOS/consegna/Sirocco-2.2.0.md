# tinymacOS 2.2.0 Sirocco

**Build 20C2 · ISO UEFI x86-64 · supporto unico con BaseSystem integrato**

**[Scarica la ISO da Google Drive — 51,67 MiB](https://drive.google.com/uc?id=1kI1hwA6oyiMClyCtENd3ophiCNoM9Xy_&export=download)** · [Checksum](tinymacOS-2.2.0-SHA256.txt)

Serve **solo questo file**: niente secondo download, niente immagine USB separata. Le release precedenti su Drive (2.1.0, 2.0.2, 2.0.1, 2.0 e 1.3) sono rimaste invariate.

## Il cambiamento di questa versione

La 2.2.0 «Sirocco» è la versione desktop di questo sistema: **dodici sfondi** (il nuovo *Sirocco*), **sette temi** e **Impostazioni ridisegnata alla macOS** con ricerca laterale e pannelli *Aspetto*, *Schermo*, *Sicurezza e privacy*, *Backup*, *Data e ora* e *Generali*. Il ripristino dell'account inaccessibile resta disponibile in due passi dal login (fail closed, conferma inlinea). Il modello del supporto unico non cambia dalla 2.0.2: esiste **un file `BaseSystem.dmg`** dentro la ISO, in `/Recovery/`, copiato identico byte per byte nella partizione Recovery dopo l'installazione.

Con questa build entrano anche tre rifiniture:

1. **CapsLock e host allineati**: il guest mantiene lo stato degli LED della tastiera (Caps/Num/Scroll) con un report HID di output inviato all'host e un flush subito dopo ogni report di input. CapsLock premuto fuori dalla finestra di QEMU resta quindi coerente con la digitazione dentro la VM.
2. **Cambio risoluzione a runtime**: da *Impostazioni → Schermo* il sistema prova una per una le risoluzioni offerte da GOP, da **800×600 fino a 1920×1080** (19 mode, allineate a 8 px, senza duplicati), i font restano a **DPI fisso** (mai troppo grandi o troppo piccoli) e le finestre rientrano nel nuovo schermo restando sotto la menu bar.
3. **Calcolatrice in vetro liquido**: foglio vetrato, tasti frosted con bordo, operatori arancio e *Cronologia* in vetro traslucido.

Con la build **20C2** entrano il rehaul completo di *Impostazioni* — impaginazione alla System Settings di Apple con **Chiaro/Scuro come pulsanti a anteprima**, sezione *Colori* (sette accenti, ognuno associato al proprio sfondo) e *Sfondi* ben separati, senza sovrapposizioni di date — **Mission Control con animazioni** (tasto F10 o menu *Finestra*: le finestre si dispongono in griglia su sfondo oscurato, Esc ripristina), le risoluzioni **fino a 1920×1080** selezionabili da *Schermo*, **quattro nuovi sfondi generati** (*Sirocco*, *Meriggio*, *Notturno*, *Brezza* — dodici in totale) e la data del pannello *Data e ora* riposizionata senza sovrapposizioni.

Nella shell di Recupero:

```
diskutil info /
```

mostra `Immagine: /Recovery/BaseSystem.dmg`, l'origine e i `byte verificati`, come nelle release precedenti.

## Avvio in QEMU

Scegli **Q35 x86-64, UEFI/OVMF senza Secure Boot, 512 MiB di RAM**, disco SATA vuoto dedicato (96 MiB), CD con la ISO e unico puntatore `usb-tablet` accanto alla tastiera USB:

```sh
-device qemu-xhci,id=xhci \
-device usb-kbd,bus=xhci.0,port=1 \
-device usb-tablet,bus=xhci.0,port=2
```

`./run.sh` del progetto configura già questo profilo; `TMC_POINTER=mouse` sceglie il mouse relativo. Durante la mela premi più volte **R** (o F12) per il Recupero. Dopo l'installazione: spegni, rimuovi la ISO, riavvia col solo disco (`BOOT_DISK=1 ./run.sh`) e premi di nuovo R.

## Cosa è stato verificato

- **Suite di accettazione completa sull'ISO finale**: live, guard, contained, pointer (tablet e mouse), disco, app, integrità, runtime, crypto e test HID — «all selected suites completed».
- **Impostazioni GUI**: 7/7 (card Chiaro/Scuro, sette colori e dodici sfondi con applicazione reale, pannello Schermo con risoluzione/uscita e switch Dock, Sicurezza, Backup con timeline, Data e ora live senza sovrapposizioni, Generali con versione 2.2.0 (20C2), ricerca laterale).
- **Matrice CLI**: 16/16, dalla creazione dell'account alla spegnimento pulito via Terminale (ACPI S5).
- **Cambio risoluzione a runtime**: ciclo completo delle 19 risoluzioni GOP **fino a 1920×1080** con ritorno a 1280×800, pannelli e finestre coerenti a ogni passaggio (nessuna corruzione); digitazione con CapsLock attiva e rientro normale.
- **Mission Control**: F10 apre la griglia animata delle finestre aperte, il clic seleziona e porta in primo piano, Esc chiude; la schermata di verifica mostra le tre finestre disposte correttamente.
- **Installazione dalla sola ISO**: account, installer da desktop, GPT primaria/secondaria, dati reali; il disco installato contiene `/Recovery/BaseSystem.dmg` identico a quello della ISO.
- **Recupero locale senza CD e senza rete**: NVRAM nuova; R → shell → `diskutil info /`; rifiuto di DMG alterato e di KernelCache corrotto senza scritture.
- **Puntatore**: gruppi tablet e mouse su setup, scrivania, app e Recupero; rotella, clic destro e trascinamenti pixel-precisi.
- **Riproducibilità**: due build indipendenti con **ISO byte-identiche** (`cmp`); su Drive un solo file 2.2.0, MD5 remoto `2b76d5d7b9e7667e7a654f4432bee8d8` identico a quello locale.

## Limiti

Nessun componente Apple, nessuna firma o Secure Boot Apple, nessuna compatibilità con macOS o APFS, nessun Command-R fisico. Il sistema non cifra i dati. La sincronizzazione degli LED CapsLock segue la specifica HID (report di output verso l'host): in QEMU headless la spia fisica non è osservabile, la coerenza della digitazione è verificata in VM. Profilo DMG accettato: UDIF non compresso con digest incorporato, non un decodificatore generico. Non usare dischi fisici con dati. Il riferimento al Mac è di **design**, non un supporto di esecuzione.

[Guida di avvio](../docs/QUICKSTART.txt) · [Architettura](../docs/ARCHITECTURE.md) · [Collaudo completo](../docs/VALIDATION.md)
