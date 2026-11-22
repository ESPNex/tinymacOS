# tinymacOS

Sistema desktop sperimentale x86-64 con kernel proprio, applicazioni native e ambiente di Recupero UEFI. Non è basato su Linux o XNU e non offre compatibilità generale con le applicazioni macOS.

**Versione pubblicata: 10.1.5 Tramontana · build 30B6.**

[Download e note di rilascio](https://github.com/ESPNex/tinymacOS/releases/latest)

## Il progetto

- Desktop con Finder, Terminale, editor, utility e Centro di Controllo in vetro.
- Temi chiari/scuri, sfondi e icone SVG originali per il Centro di Controllo.
- Tastiera italiana ISO, input PS/2 e driver USB xHCI, EHCI e UHCI.
- Avvio ottico BIOS/UEFI, installazione su SATA e ripristino locale offline.

## Stato e limiti

Il mouse sulla ASRock Q1900M/J1900 resta un problema aperto: la 10.1.4 è risultata immobile al login e in Recovery; la 10.1.5 non è ancora stata verificata su quella macchina. I test QEMU non costituiscono una verifica hardware.

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
