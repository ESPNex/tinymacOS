# tinymacOS

Sistema operativo desktop sperimentale **x86-64**, con kernel proprio, applicazioni native e Recovery con kernel dedicato. Non è macOS e non esegue le applicazioni macOS.

**Versione attuale: 10.2.1 Tramontana · build 30B12 · pre-release.**

[Scarica ISO e sorgenti](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.1) · [Novità della 10.2.1](tinymacOS/docs/NOTE-10.2.1.md)

## Funzioni principali

- Desktop con Finder, Terminale, editor, utility e temi chiaro/scuro.
- Interfaccia Liquid Glass, Dock e Centro di Controllo.
- Tastiera italiana e input USB con controller xHCI/EHCI/UHCI; percorso canonico USB/HID Linux su xHCI/EHCI.
- Avvio ottico BIOS/UEFI, installazione su SATA e Recovery nativa offline, scura e opaca.

## Da sapere

- Progetto sperimentale: i test in macchina virtuale non garantiscono compatibilità con l'hardware reale. La verifica sulla ASRock Q1900M/J1900 resta da completare.
- La ISO **non è USB-DD ibrida**. Per le prove usare macchine virtuali e dischi vuoti dedicati; non usare dischi con dati da conservare.
- Sulle GPU senza cambio risoluzione runtime supportato, la scelta richiede un sistema installato e viene applicata al riavvio.
- Browser WebKit non incluso; compatibilità USB/HID non universale.

## Documentazione

[Compilazione e sviluppo](tinymacOS/README.md) · [Guida hardware](tinymacOS/docs/HARDWARE-TEST.md) · [USB/HID](tinymacOS/docs/USBHID-PORT.md) · [Licenze dei componenti](tinymacOS/licenses/)
