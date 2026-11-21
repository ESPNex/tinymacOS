# tinymacOS 10 · Tramontana

Sistema desktop sperimentale x86-64 con kernel proprio, processi Ring 3, ABI nativa e runtime UEFI di Recupero. Non usa Linux o XNU e non esegue generalmente applicazioni per macOS.

**[Versione 10, build 30A2 — manutenzione della stessa v10](https://github.com/ESPNex/tinymacOS/releases/tag/v10): Release normale e Latest; tutte le precedenti Pre-release.** Ricevuta e collaudi in [`workspace/release-10-30A2`](../workspace/release-10-30A2/). Ultima ISO pubblicata precedente: [2.4.5 / 21B9](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.5), con [prove e ricevuta](../workspace/release-2.4.5-input4/RELEASE.md). I download sono privati e richiedono accesso GitHub autorizzato.

## Tramontana

- Nuovo sfondo non procedurale realizzato con generazione nativa di immagini; sorgente conservata in `assets/tramontana-generated.jpg`. Wallpaper QOI 1280×800 installato nel filesystem.
- Icone PNG dall'archivio fornito dall'utente: variante datata più recente per ciascun nome esatto. Provenienza e SHA-256 in `assets/icons/manifest.json`.
- Atlas applicazioni a 16/24/32/48/64/128 px; originali PNG e varianti realmente installati nei bundle. Cartelle, documenti, dischi, Libreria, controlli e categorie Impostazioni/Centro di Controllo usano i simboli procedurali originali; nessun App Store sostitutivo. Anche Recovery è procedurale, tranne Terminale.
- Eccezione esplicita: icona installer originale procedurale dedicata a Tramontana, rigenerata deterministicamente dal packer.
- Tema Tramontana e tredicesimo sfondo predefiniti. Teal verde e Tramontana blu ardesia distinti anche nei selettori; Blu, Azzurro e Riviera mantengono palette complete distinte. Associazioni esplicite Blu → Prisma, Azzurro → Brezza, Tramontana → Tramontana.
- Informazioni: “tiny” conserva integralmente il vecchio carattere regolare; “macOS” usa una faccia autentica di peso 600, non un grassetto simulato. Licenza OFL inclusa. Le dieci vecchie facce sono conservate bit per bit.
- Recupero: mela nativa 24 px allineata al testo della barra e titolo finestra più piccolo.
- Identità 10 / 30A2 in app, metadati, shell, installer, Recupero e documenti iniziali.

**Safari/WebKit reale resta sospeso e non è incluso.** I test backend/runtime non costituiscono prove di un browser funzionante.

## Input e limiti

Manutenzione 30A2: routing Intel USB2 del firmware mantenuto se è presente EHCI; `SET_PROTOCOL` STALL gestito tramite conferma `GET_PROTOCOL` anche in Recovery e ripiego Boot verificato. **Sul PC ASRock il mouse è stato segnalato fermo sia nel live sia in Recovery: causa fisica non accertata e soluzione non confermata.**

Conservati i decoder della 21B9: tastiera fisica esclusivamente italiana ISO, UTF-8, accenti, euro e AltGr fra login, app e Recupero; PS/2, EHCI HID, UHCI companion e xHCI con hub USB2. Gli assi mouse sono estratti dal descrittore validato e gestiti indipendentemente. Nessun pannello diagnostico F11 al login. F11 sul desktop live apre l'installer, come il collegamento sulla scrivania.

I test QEMU non verificano il PC fisico ASRock Q1900M/J1900 né garantiscono compatibilità universale. Non sono implementati OHCI, hotplug generale, hub SuperSpeed o HID universale. Nessuna nuova conferma fisica è implicita nei PASS. Il supporto conserva gli standard/controller generali, non una soluzione riservata a una scheda.

## Build e avvio

Output, ISO, toolchain e archivi voluminosi vanno fuori dal workspace. Esempio dalla directory `tinymacOS`:

```sh
python3 tools/local-tools.py /var/tmp/tmc/tools/root
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc10 bash build.sh
mkdir -p /var/tmp/tmc10-optical
TOOLROOT=/var/tmp/tmc/tools/root python3 tools/dualboot-iso.py \
  /var/tmp/tmc10/tinymacOS-10-x86_64.iso \
  /var/tmp/tmc10-optical/tinymacOS-10-x86_64.iso
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc10 STATE=/var/tmp/tramontana-vm LIVE=1 bash run.sh
```

Richiesti Python/Pillow e gli strumenti descritti dai lock. La normale build usa il font già rasterizzato; `tools/tramontana-font.py` serve a rigenerare la faccia semibold con la sorgente TTF inclusa. Non occorre riscaricare lo ZIP delle icone.

Il media distribuito aggiunge al percorso UEFI nativo un avvio CD BIOS via ISOLINUX/MEMDISK → OpenDuet/OpenCore → loader nativo. I dischi installati usano GPT/FAT16 con ponte MBR nello spazio riservato e avviano sia in BIOS sia in UEFI nei profili collaudati. Il media non è un'immagine USB DD ibrida.

Usare una VM Q35/AHCI x86-64, Secure Boot spento e almeno 512 MiB RAM (1 GiB consigliato). Per installare collegare un **nuovo disco virtuale SATA vuoto dedicato**, almeno 96 MiB consigliati. Creare l'account live, aprire *Installa tinymacOS*, controllare e confermare la destinazione. Dopo “Il disco è pronto”, spegnere, rimuovere l'ISO e avviare dal disco con la stessa password. Non usare dischi fisici o dati importanti. Avviare la nuova ISO non aggiorna un vecchio sistema installato.

## Desktop e Recupero

F1–F9 aprono le app; F2 apre Terminale. Ctrl+Spazio apre Cerca; Ctrl+Maiusc+Q blocca la sessione. Finder mostra i file e i bundle reali. I temi e sfondi storici conservano il loro nome e non identificano la release attuale.

Premere ripetutamente R o F12 durante la mela per il Recupero UEFI ottico. L'installer copia anche BaseSystem nella partizione Recovery: il recupero locale può partire senza ISO. Il Recupero è un runtime distinto, non Internet Recovery né un ambiente Apple; `sw_vers` riporta `tinymacOS Recovery 10 (30A2)`.

Verbose durante l'avvio con V; comandi `bootverbose status`, `on`, `off`, `log [pagina]`. La preferenza persistente richiede un sistema installato. VM/MMU multi-CPU, TLS per-thread e sincronizzazione nativa restano funzionalità sperimentali, non una libc/POSIX/pthread completa.

## Prove e storico

- Nuove ricevute: [`workspace/release-10-30A2`](../workspace/release-10-30A2/).
- Precedente 21B9: [`workspace/release-2.4.5-input4`](../workspace/release-2.4.5-input4/).
- Prove runtime/kernel: [`workspace/webkit-tmc`](../workspace/webkit-tmc/), con limiti espliciti e kernel delle sonde confrontato con la produzione.
- Storia architetturale: `docs/` e `docs/storico/`; i numeri di versione nei documenti storici non sono l'identità corrente.
