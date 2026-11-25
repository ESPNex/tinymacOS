# tinymacOS

Sistema operativo desktop sperimentale **x86-64**, con kernel proprio, applicazioni native e Recovery con kernel dedicato. Non è macOS e non esegue le applicazioni macOS.

**Versione attuale: 10.2.4 Tramontana · build 30B15 · pre-release.**

[Scarica ISO e sorgenti](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.4) · [Novità della 10.2.4](tinymacOS/docs/NOTE-10.2.4.md)

**Revisione 10.2.4:** Recovery senza residui della barra; dischi SATA/AHCI da 512 B per settore fino a **1 TiB**, inclusi i dischi commerciali da 1 TB. Conservati snapshot CoW reali, resize nativo e motore NTFS freestanding. Il volume dati nativo resta da 8–32 MiB. [Dettagli e limiti](tinymacOS/docs/DISK-UTILITY-DEVELOPMENT.md).

## Funzioni principali

- Un solo assistente «Installa tinymacOS Tramontana» nel desktop e «Reinstalla tinymacOS Tramontana» in Recovery.
- Installer su spazio non allocato GPT/MBR; riconoscimento FAT32, NTFS e RAW in Installer, Utility Disco e Terminale.
- Snapshot CoW: dati iniziali condivisi, vecchi blocchi conservati una sola volta e recupero dello spazio all’ultimo riferimento.
- Utility Disco offline: creazione/eliminazione e resize FAT32, NTFS nativo e formato proprietario a capacità variabile 8–32 MiB.
- Desktop con Finder a doppio clic, Terminale, editor, utility e temi chiaro/scuro.
- FileVault nativo: volume dati cifrato, sblocco prima del desktop, chiave di recupero e gestione da Impostazioni/Recovery.
- Interfaccia Liquid Glass, Dock e Centro di Controllo.
- Tastiera italiana e input USB con controller xHCI/EHCI/UHCI; percorso canonico USB/HID Linux su xHCI/EHCI.
- Avvio ottico BIOS/UEFI, installazione su SATA e Recovery nativa offline, scura e opaca.

## Da sapere

- Progetto sperimentale: i test in macchina virtuale non garantiscono compatibilità con l'hardware reale. La verifica sulla ASRock Q1900M/J1900 resta da completare.
- La ISO **non è USB-DD ibrida**. Prima provare in VM e fare un backup completo: il dual boot è sperimentale, non certificato con Windows reale. Servono 96 MiB non allocati; nessun ridimensionamento automatico.
- UEFI: il primo avvio verificato registra OpenCore e conserva il precedente BootOrder, senza sostituire Windows Boot Manager. Per raggiungerlo può ancora servire scegliere il nuovo volume dal firmware.
- Formato nativo proprietario: massimo 384 voci, 8 KiB/file e 8 snapshot; non Apple APFS o Btrfs Linux. Resize cifrato non supportato; dopo il resize NTFS serve il controllo Windows.
- Sulle GPU senza cambio risoluzione runtime supportato, la scelta richiede un sistema installato e viene applicata al riavvio.
- Browser WebKit non incluso; compatibilità USB/HID non universale.

## Documentazione

[Compilazione e sviluppo](tinymacOS/README.md) · [Guida hardware](tinymacOS/docs/HARDWARE-TEST.md) · [USB/HID](tinymacOS/docs/USBHID-PORT.md) · [Licenze dei componenti](tinymacOS/licenses/)
