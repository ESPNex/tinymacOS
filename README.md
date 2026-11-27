# tinymacOS

Sistema operativo desktop sperimentale **x86-64**, con kernel proprio, applicazioni native e Recovery con kernel dedicato. Non è macOS e non esegue le applicazioni macOS.

**Versione attuale: 10.2.6 Tramontana · build 30C1 · pre-release.**

[Scarica l’unica ISO BIOS+UEFI](https://github.com/ESPNex/tinymacOS/releases/download/v10.2.6-pre-release/tinymacOS-10.2.6-x86_64.iso) · [Sorgenti e verifiche](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.6-pre-release) · [Note](tinymacOS/docs/NOTE-10.2.6.md) · [Validazione e hash](tinymacOS/docs/VALIDAZIONE-10.2.6.md)

**10.2.6:** solo stack USB/HID canonical, base 30B21 confermata dall'utente sul PC fisico; vecchio adattatore alternativo rimosso. Drag-and-drop ripristinato, Finder Taglia/Copia/Incolla e appunti Terminale conservati. Ripristino della copertura PMBR soltanto con GPT identiche/valide e consenso; nessun bypass delle protezioni Windows. Ridotto il lavoro di composizione con molte finestre. Conservati font UI/Mono forniti, FS2, installazione/avvio 4Kn e conversione/disattivazione cifratura in-place. Circa 34 MB di artefatti storici/temporanei rimossi dal workspace.

**Verifica:** 9 suite host, 17 suite guest della stessa ISO finale e confronto con 18 finestre sulla 30B21. La nuova 10.2.6 resta da confermare sul PC fisico; il PMBR reale dell'utente non è stato acquisito. Safari/WebKit completo non è incluso. [Storico del ripristino mouse 30B21](tinymacOS/docs/NOTE-10.2.5-30B21.md).

## Funzioni principali

- Un solo assistente «Installa tinymacOS Tramontana» nel desktop e «Reinstalla tinymacOS Tramontana» in Recovery.
- Installer su spazio non allocato GPT/MBR; riconoscimento FAT32, NTFS e RAW in Installer, Utility Disco e Terminale.
- Snapshot CoW: dati iniziali condivisi, vecchi blocchi conservati una sola volta e recupero dello spazio all’ultimo riferimento.
- Utility Disco offline: creazione/eliminazione e resize FAT32, NTFS nativo e formato storico; FS2 usa l’extent dati e non supporta resize.
- Desktop con Finder a doppio clic, Terminale, editor, utility e temi chiaro/scuro.
- FileVault nativo: volume dati cifrato, sblocco prima del desktop, chiave di recupero e gestione da Impostazioni/Recovery.
- Interfaccia Liquid Glass, Dock e Centro di Controllo.
- Tastiera italiana e unico stack USB core/HID canonical, con controller originali xHCI/EHCI. Nessun selettore per il percorso USB nativo ritirato.
- Avvio ottico BIOS/UEFI, installazione su SATA/NVMe e Recovery nativa offline, scura e opaca.

## Da sapere

- Progetto sperimentale: i test in macchina virtuale non garantiscono compatibilità con l'hardware reale. La verifica sulla ASRock Q1900M/J1900 resta da completare.
- La ISO **non è USB-DD ibrida**. Prima provare in VM e fare un backup completo: il dual boot è sperimentale, non certificato con Windows reale. Servono 96 MiB non allocati; nessun ridimensionamento automatico.
- UEFI: il primo avvio verificato registra OpenCore e conserva il precedente BootOrder, senza sostituire Windows Boot Manager. Per raggiungerlo può ancora servire scegliere il nuovo volume dal firmware.
- FS2 proprietario: directory/cataloghi dinamici e snapshot CoW; non Apple APFS o Btrfs Linux. Resize cifrato non supportato; dopo il resize NTFS serve il controllo Windows.
- Sulle GPU senza cambio risoluzione runtime supportato, la scelta richiede un sistema installato e viene applicata al riavvio.
- Browser WebKit non incluso; compatibilità USB/HID non universale.

## Documentazione

[Compilazione e sviluppo](tinymacOS/README.md) · [Guida hardware](tinymacOS/docs/HARDWARE-TEST.md) · [USB/HID](tinymacOS/docs/USBHID-PORT.md) · [Licenze dei componenti](tinymacOS/licenses/)
