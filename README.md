# tinymacOS

Sistema operativo desktop sperimentale **x86-64**, con kernel proprio, applicazioni native e Recovery con kernel dedicato. Non è macOS e non esegue le applicazioni macOS.

**Versione attuale: 10.2.5 Tramontana · build 30B19 · pre-release.**

[Scarica l’unica ISO BIOS+UEFI](https://github.com/ESPNex/tinymacOS/releases/download/v10.2.5-pre-release/tinymacOS-10.2.5-x86_64.iso) · [Sorgenti e verifiche](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.5-pre-release) · [Novità della 10.2.5](tinymacOS/docs/NOTE-10.2.5.md)

**10.2.5:** FS2 su tutto l’extent dati, NVMe nativo e installazione/avvio UEFI 4Kn, conversione/deattivazione cifratura sullo stesso extent da Recovery; Finder Taglia/Copia/Incolla e appunti Terminale; trasferimento file drag-and-drop disabilitato dal rollback 30B19. [Accettazione e hash](tinymacOS/docs/RELEASE-READINESS-10.2.5.md).

**Storico della revisione 30B17:** font della ISO 10.2.4 ripristinato e bloccato nel packaging; gestione per-frame degli eventi mouse/drag; inventario dello spazio non allocato conservato su GPT protette, senza convertire RAW/MSR in spazio libero. [Correzioni, test e limiti](tinymacOS/docs/HOTFIX-10.2.5-30B17.md). Safari completo non è incluso.

**Rebuild 30B18:** UI dai quattro OTF forniti e vero Mono Regular per Terminale e panic; nessuna famiglia di fallback. FS2 obbligatorio nel build di produzione. Audit font nei tre ambienti e 14 suite guest della nuova ISO chiusi. [Note e limiti](tinymacOS/docs/NOTE-10.2.5-30B18.md) · [Verifica della build](tinymacOS/docs/VALIDAZIONE-10.2.5-30B18.md).

**Revisione 30B19:** rollback desktop/input esatto alla 10.2.4, drag-and-drop file disabilitato; GPT riconosciuta anche senza firma MBR e ripristino disponibile solo nell'installer con doppia GPT valida, spazio realmente non allocato e consenso esplicito. Nessuna conferma del mouse sul PC fisico. [Note e limiti](tinymacOS/docs/NOTE-10.2.5-30B19.md).

## Funzioni principali

- Un solo assistente «Installa tinymacOS Tramontana» nel desktop e «Reinstalla tinymacOS Tramontana» in Recovery.
- Installer su spazio non allocato GPT/MBR; riconoscimento FAT32, NTFS e RAW in Installer, Utility Disco e Terminale.
- Snapshot CoW: dati iniziali condivisi, vecchi blocchi conservati una sola volta e recupero dello spazio all’ultimo riferimento.
- Utility Disco offline: creazione/eliminazione e resize FAT32, NTFS nativo e formato storico; FS2 usa l’extent dati e non supporta resize.
- Desktop con Finder a doppio clic, Terminale, editor, utility e temi chiaro/scuro.
- FileVault nativo: volume dati cifrato, sblocco prima del desktop, chiave di recupero e gestione da Impostazioni/Recovery.
- Interfaccia Liquid Glass, Dock e Centro di Controllo.
- Tastiera italiana e input USB con controller xHCI/EHCI/UHCI; componenti USB/input Linux integrati e variante canonica sperimentale opzionale.
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
