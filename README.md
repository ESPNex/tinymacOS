# tinymacOS

Sistema desktop sperimentale x86-64 con kernel proprio, applicazioni native e ambiente di Recupero UEFI. Non è basato su Linux o XNU e non offre compatibilità generale con le applicazioni macOS.

**Versione pubblicata: 10.1.6 Tramontana · build 30B7.**

La 10.1.7 / 30B8 è in sviluppo: OpenCore unificato, cancellazione delle cartelle e pulizia UI. Sono integrati i driver Boot mouse/tastiera, i moduli core URB e configurazione USB, la funzione DMA qTD EHCI e la libreria di protocollo PS/2 originali di Linux 6.12, oltre alle funzioni originali `psmouse` per pacchetti e rilevamento IntelliMouse/Explorer. I file originali completi `hid-generic` e `hid-input` gestiscono ora anche report USB reali tramite i controller nativi, entro limiti espliciti; verificati 10 profili QEMU BIOS/UEFI. Sono attive anche le funzioni originali HCD per code/completamenti URB e hub USB2 per debounce/reset. Controller Linux, core/hub USB e restante core HID non sono ancora completi; i driver di classe PS/2 sono rimandati alla fine. La tastiera Recovery usa ora i driver firmware OpenCore, con mappatura fisica italiana verificata su UEFI e BIOS. Anche Info riporta ora 10.1.7 / 30B8. Il port Linux completo resta da terminare: nessuna nuova ISO è stata pubblicata.

[Download e note di rilascio](https://github.com/ESPNex/tinymacOS/releases/latest)

## Il progetto

- Desktop con Finder, Terminale, editor, utility e Centro di Controllo in vetro.
- Temi chiari/scuri, sfondi e icone SVG originali per il Centro di Controllo.
- Tastiera italiana ISO, input PS/2 e driver USB xHCI, EHCI e UHCI.
- Avvio ottico BIOS/UEFI, installazione su SATA e ripristino locale offline.

## Stato e limiti

L'utente conferma avvio e USB funzionanti in VirtualBox con la 10.1.5. La 10.1.6 mantiene gli stessi driver, ma non è ancora verificata sulla ASRock Q1900M/J1900 fisica: la 10.1.4 lì aveva il mouse immobile al login e in Recovery. Il successo in macchina virtuale non garantisce quello sull'hardware reale.

Safari/WebKit non è incluso. Non sono supportati HID/USB universale, OHCI, hotplug generale o hub SuperSpeed. La ISO non è un'immagine USB-DD ibrida. Per le prove d'installazione usare soltanto dischi virtuali vuoti dedicati, senza dati da conservare.

## Sorgenti

| Percorso | Contenuto |
| --- | --- |
| `tinymacOS/` | Sistema operativo, risorse, build e test |
| `workspace/webkit-tmc/` | Backend sperimentale per il futuro port WebKit; non un browser |
| `workspace/release-*/` | Note specifiche, script e ricevute delle build pubblicate o sostituite |
| `workspace/maintenance-*/`, `workspace/integrated-next/` | Revisioni precedenti, mirror di sviluppo ed evidenze |

Le ISO e i checksum sono negli asset delle release. Le versioni precedenti restano pre-release; i loro tag e file sono conservati.

[Build e guida tecnica](tinymacOS/README.md) · [Contribuire](CONTRIBUTING.md)
