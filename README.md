# tinymacOS

Sistema operativo desktop sperimentale **x86-64**, con kernel proprio, applicazioni native e Recovery con kernel dedicato. Non è macOS e non esegue le applicazioni macOS.

**Versione attuale: 10.2.3 Tramontana · build 30B14 · pre-release.**

[Scarica ISO e sorgenti](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.3) · [Novità della 10.2.3](tinymacOS/docs/NOTE-10.2.3.md)

**Sviluppo non rilasciato:** [Utility Disco/OpenCore: stato e limiti](tinymacOS/docs/DISK-UTILITY-DEVELOPMENT.md).

## Funzioni principali

- Installer su spazio non allocato GPT/MBR; riconoscimento FAT32, NTFS e RAW in Installer, Utility Disco e Terminale.
- Desktop con Finder a doppio clic, Terminale, editor, utility e temi chiaro/scuro.
- FileVault nativo: volume dati cifrato, sblocco prima del desktop, chiave di recupero e gestione da Impostazioni/Recovery.
- Interfaccia Liquid Glass, Dock e Centro di Controllo.
- Tastiera italiana e input USB con controller xHCI/EHCI/UHCI; percorso canonico USB/HID Linux su xHCI/EHCI.
- Avvio ottico BIOS/UEFI, installazione su SATA e Recovery nativa offline, scura e opaca.

## Da sapere

- Progetto sperimentale: i test in macchina virtuale non garantiscono compatibilità con l'hardware reale. La verifica sulla ASRock Q1900M/J1900 resta da completare.
- La ISO **non è USB-DD ibrida**. Prima provare in VM e fare un backup completo: il dual boot è sperimentale, non certificato con Windows reale. Servono 96 MiB non allocati; nessun ridimensionamento automatico.
- UEFI: può servire aggiungere dal firmware il file `\EFI\BOOT\BOOTX64.EFI` del nuovo volume; Windows Boot Manager non viene sostituito.
- Sulle GPU senza cambio risoluzione runtime supportato, la scelta richiede un sistema installato e viene applicata al riavvio.
- Browser WebKit non incluso; compatibilità USB/HID non universale.

## Documentazione

[Compilazione e sviluppo](tinymacOS/README.md) · [Guida hardware](tinymacOS/docs/HARDWARE-TEST.md) · [USB/HID](tinymacOS/docs/USBHID-PORT.md) · [Licenze dei componenti](tinymacOS/licenses/)
