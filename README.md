# tinymacOS

Sistema desktop sperimentale x86-64 con kernel proprio, applicazioni native e ambiente di Recupero UEFI. Non è basato su Linux o XNU e non offre compatibilità generale con le applicazioni macOS.

**Versione pubblicata: 10.1.6 Tramontana · build 30B7.**

La **10.1.7 / 30B8** ha ora una variante canonica con il collegamento **xHCI/EHCI → USB core/HCD → hub → HID-core/usbhid → hid-input/hid-generic originali Linux v6.12**. Non usa più le classi Boot nel percorso canonico. Tastiera, mouse relativo e tablet assoluto passano attraverso il medesimo modello USB, con handle opachi verso i controller e worker kernel dedicato, pinning CPU0, fuori dall’IRQ. Sono implementati control, interrupt-IN e interrupt-OUT. Il percorso precedente resta disponibile per confronto; selezionare `TMC_USB_CANONICAL=1` in compilazione.

[ISO candidata USB/HID e sorgenti](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.7-usbhid-candidate). Le prove e i limiti correnti sono in [USBHID-PORT.md](tinymacOS/docs/USBHID-PORT.md). La release stabile 10.1.6 non viene sostituita dalla candidata USB. **QEMU non certifica hardware fisico**, IRQ/IOMMU/PM o compatibilità HID universale; PS/2 è escluso dal nuovo percorso. Recovery usa separatamente i driver firmware OpenCore.

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
