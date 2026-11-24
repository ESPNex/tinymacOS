# tinymacOS

Sistema desktop sperimentale x86-64 con kernel proprio, applicazioni native e ambiente di Recupero UEFI. Non è basato su Linux o XNU e non offre compatibilità generale con le applicazioni macOS.

**Versione pubblicata: 10.1.6 Tramontana · build 30B7.**

La **10.1.7 / 30B8 è in sviluppo e non pubblicata**. I controller UHCI, EHCI e xHCI eseguono ora i driver originali Linux 6.12 su runtime privati a polling. La modalità sperimentale `TMC_USB_CANONICAL=1` collega davvero **xHCI/EHCI → core USB/HCD → hub Linux → driver Boot mouse/tastiera**, con un unico modello canonico USB e confini a handle opachi. Sullo stesso ISO sperimentale passano **6/6 casi QEMU BIOS/UEFI**: login e Terminale via tastiera USB, cursore verificato a pixel, hub esterno, scollegamento con URB attive e ricollegamento. Il vecchio percorso nativo di enumerazione/classe non prende in carico dispositivi in questa modalità.

Questo **non è ancora il port completo HID-core/usbhid**: il percorso canonico usa `usbkbd`/`usbmouse`, non HID universale, e resta opt-in. Il percorso predefinito mantiene l'adattatore di enumerazione/classe precedente, con i moduli HID generici già integrati sul loro ABI separato. PS/2 è escluso dal lavoro residuo richiesto. OpenCore, input firmware di Recupero, layout italiano, cancellazione e identità 10.1.7 / 30B8 hanno prove proprie: i loro risultati storici non costituiscono il gate finale del nuovo ISO. Nessuna nuova release o prova fisica ASRock viene dichiarata.

[Download e note di rilascio](https://github.com/ESPNex/tinymacOS/releases/latest)

## Il progetto

- Desktop con Finder, Terminale, editor, utility e Centro di Controllo in vetro.
- Temi chiari/scuri, sfondi e icone SVG originali per il Centro di Controllo.
- Tastiera italiana ISO, input PS/2 e driver USB xHCI, EHCI e UHCI.
- Avvio ottico BIOS/UEFI, installazione su SATA e ripristino locale offline.

## Stato e limiti

L'utente conferma avvio e USB funzionanti in VirtualBox con la 10.1.5. La 10.1.6 mantiene gli stessi driver, ma non è ancora verificata sulla ASRock Q1900M/J1900 fisica: la 10.1.4 lì aveva il mouse immobile al login e in Recovery. Il successo in macchina virtuale non garantisce quello sull'hardware reale.

Safari/WebKit non è incluso. Non sono supportati HID/USB universale o OHCI; il test hotplug della modalità canonica riguarda soltanto le topologie documentate, non il supporto generale o una certificazione degli hub SuperSpeed. La ISO non è un'immagine USB-DD ibrida. Per le prove d'installazione usare soltanto dischi virtuali vuoti dedicati, senza dati da conservare.

### Prova su hardware reale

È possibile tentare un **avvio live sperimentale su un computer non critico**, ma compatibilità e affidabilità non sono ancora dimostrate. Usare il supporto **ottico BIOS/UEFI** indicato nella [guida hardware](tinymacOS/docs/HARDWARE-TEST.md): la ISO **non** è USB-DD ibrida e non si promette l'avvio da Rufus, Ventoy o copia su chiavetta. Scollegare i dischi con dati, non avviare l'installer né utility distruttive, usare mouse e tastiera USB Boot semplici. Il test non modifica lo stato della release stabile.

## Sorgenti

| Percorso | Contenuto |
| --- | --- |
| `tinymacOS/` | Sistema operativo, risorse, build e test |
| `workspace/webkit-tmc/` | Backend sperimentale per il futuro port WebKit; non un browser |
| `workspace/release-*/` | Note specifiche, script e ricevute delle build pubblicate o sostituite |
| `workspace/maintenance-*/`, `workspace/integrated-next/` | Revisioni precedenti, mirror di sviluppo ed evidenze |

Le ISO e i checksum sono negli asset delle release. Le versioni precedenti restano pre-release; i loro tag e file sono conservati.

[Build e guida tecnica](tinymacOS/README.md) · [Contribuire](CONTRIBUTING.md)
