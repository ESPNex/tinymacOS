# tinymacOS 2.0.1 Riviera

**Build 20A2 · ISO UEFI x86-64 · QEMU USB Tablet**

**[Scarica la ISO da Google Drive — 52,63 MiB](https://drive.google.com/uc?id=1-Ja9qcEr34D8eSdNr9vsUULIwaleUHoI&export=download)**  
[Scarica solo il Recupero — 4 MiB](https://drive.google.com/uc?id=12HFtgI4Ma8_AkWs44Cg-UzTyC3ZK0QEg&export=download) · [Checksum dei due file](tinymacOS-2.0.1-SHA256.txt)

Per provare o installare il sistema serve **solo la ISO**. Il Recupero autonomo è già incluso. La versione 2.0 precedente è rimasta su Drive, senza modifiche.

## Avvio in QEMU

Scegli **Q35 x86-64, UEFI/OVMF senza Secure Boot, 512 MiB di RAM**. Collega la ISO come CD e selezionala per l’avvio. Imposta **USB Tablet** come unico puntatore, accanto alla tastiera USB. Esempio delle sole opzioni USB:

```sh
-device qemu-xhci,id=xhci \
-device usb-kbd,bus=xhci.0,port=1 \
-device usb-tablet,bus=xhci.0,port=2
```

Se il controller xHCI è già presente, usa il suo bus senza aggiungere un secondo controller. Nel launcher del progetto `./run.sh` sceglie già il tablet. Per il vecchio profilo: `TMC_POINTER=mouse ./run.sh`.

## Cosa è stato verificato

- **Setup e login:** clic sui campi, su *Crea account* e sulla freccia di accesso del disco installato. Il cursore raggiunge tutti e quattro gli angoli del framebuffer 1280×800.
- **Scrivania:** una finestra del Finder, trascinata di 87×41 pixel, si è spostata di 87×41 e si è fermata al rilascio. Provati anche menu contestuale, doppio clic sull’installer e rotella nel Terminale.
- **Recupero UEFI:** avviato da solo via USB, senza SATA né ISO. Due clic, su *Terminale* e *Continua*, hanno aperto la shell di recupero. Mouse relativo verificato in una VM separata.

Per installare usa un **nuovo disco SATA virtuale vuoto** (96 MiB consigliati). Al termine spegni, rimuovi la ISO e riavvia dal disco. Il semplice avvio del CD non aggiorna un’installazione 2.0 esistente. Non usare dischi fisici con dati da conservare.

**SHA-256 della ISO**, confrontato con il valore di Drive:

```text
0c48424f70ab6d385eac27d8f0e8504d9513618e69751a554a8d27481cb96422
```

[Guida completa](../docs/QUICKSTART.txt) · [Collaudo e prove ripetibili](../docs/VALIDATION.md)
