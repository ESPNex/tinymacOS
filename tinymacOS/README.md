# tinymacOS 10.1.4 · Tramontana / 30B5

Sistema desktop sperimentale x86-64 con kernel proprio, processi Ring 3, ABI nativa e runtime UEFI di Recupero. Non usa Linux o XNU e non esegue generalmente applicazioni per macOS. Repository pubblico, ISO negli asset delle release, non in Git.

[Note 10.1.4, risultati e limiti](../workspace/release-10.1.4-30B5/RELEASE.md) · [Confronto sorgente Linux v6.12](../workspace/release-10.1.4-30B5/LINUX-REVIEW.md). Lo stato di pubblicazione effettivo è nella ricevuta `github-release-verification.json`, quando presente; fino alla pubblicazione resta disponibile la precedente 10.1.3.

## Input: situazione reale

**10.1.3: l’utente segnala Recovery funzionante ma movimento soltanto verticale. La correzione sul PC fisico non è confermata.** Riferimento storico esatto: 2.4.5 Levante / 21B9, Recovery funzionante e login solo verticale.

La 10.1.4 corregge il framing dei pacchetti numerati interpretati come Boot, preferisce il descriptor e il protocollo Report verificato, conservando un fallback Boot negoziato. Tre test identici sul vero codice precedente riproducono la conversione errata X=64/Y=0 → X=0/Y=64; il nuovo codice mantiene entrambi gli assi indipendenti. Questo è un difetto riprodotto, non un’acquisizione USB del mouse dell’utente.

Confronto ingegneristico con Linux v6.12: xHCI pubblica il primo TRB soltanto dopo l’intero TD, attende Status dopo Short Data e controlla la lunghezza effettiva; EHCI quiesce la schedule periodica prima delle modifiche all’overlay e lascia il toggle interrupt all’hardware. Nessun driver Linux importato. La Recovery usa EFI_USB_IO e condivide il decoder; non avvia gli HCD nativi mentre il firmware possiede i controller.

Conservati PS/2, EHCI HID, UHCI companion, xHCI con hub USB2, routing Intel, ripresa asincrona Recovery e acquisizione del solo figlio mouse. Tastiera fisica esclusivamente italiana ISO, UTF-8, accenti, euro e AltGr tra login, app e Recupero. Nessun pannello diagnostico F11 al login; F11 sul desktop live apre l’installer.

**I PASS QEMU e ASan/UBSan non provano il funzionamento sull’ASRock Q1900M/J1900.** Nessuna regola riservata a quella scheda; niente promessa di compatibilità universale. Non implementati OHCI, hotplug generale, hub SuperSpeed o HID universale.

## Centro di Controllo e artwork

13 icone originali in `assets/control-center/*.svg`, sorgenti autorevoli. `tools/control-svg.py` le compila deterministicamente in maschere alpha antialias a 64 px, tinteggiate dal renderer nativo; non è un interprete SVG/XML a runtime. Gli SVG e il manifest sono installati nel filesystem. Il codice geometrico C precedente delle icone del pannello è rimosso.

Pannello di vetro conservato; **un solo Sfondo e un solo accesso alle Impostazioni**. Contratti su hit-test e azioni effettive, preview tratta dalla vera ISO, non un mockup.

Icone importate soltanto per le applicazioni; cartelle, dischi, Libreria e categorie Impostazioni rimangono procedurali. Recovery procedurale salvo Terminale; installer con artwork procedurale dedicato Tramontana. PNG applicazioni dall’archivio fornito, scelta della variante datata più recente per nome esatto, provenienza/SHA in `assets/icons/manifest.json`, taglie native 16/24/32/48/64/128 px realmente nei bundle.

Sfondo Tramontana non procedurale generato con immagini native; sorgente `assets/tramontana-generated.jpg`. Tema e palette distinti, incluse varianti chiare/scure Blu, Azzurro e Riviera. Teal resta distinto da Tramontana; Blu → Prisma, Azzurro → Brezza, Tramontana → Tramontana. Wordmark Informazioni: “tiny” regolare conservato, “macOS” con faccia autentica semibold 600, licenza OFL inclusa.

**Safari/WebKit reale non incluso**: prove backend/runtime non equivalgono a un browser funzionante.

## Build e avvio

Toolchain e output voluminosi fuori dal workspace. Dalla directory `tinymacOS`:

```sh
python3 tools/local-tools.py /var/tmp/tmc/tools/root
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc1014 bash build.sh
mkdir -p /var/tmp/tmc1014-optical
TOOLROOT=/var/tmp/tmc/tools/root python3 tools/dualboot-iso.py \
  /var/tmp/tmc1014/tinymacOS-10.1.4-x86_64.iso \
  /var/tmp/tmc1014-optical/tinymacOS-10.1.4-x86_64.iso
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc1014 \
  STATE=/var/tmp/tramontana-vm LIVE=1 bash run.sh
```

Richiesti Python/Pillow e gli strumenti dei lock. Font già rasterizzato nella normale build; `tools/tramontana-font.py` rigenera la faccia semibold dal TTF incluso. Il compilatore delle icone SVG non scarica risorse o librerie di rete.

ISO ottica UEFI nativa + CD BIOS ISOLINUX/MEMDISK → OpenDuet/OpenCore → loader nativo. **Non è un’immagine USB-DD ibrida.** Sistemi installati GPT/FAT16 con ponte MBR nello spazio riservato, collaudati BIOS/UEFI in VM. Avviare la nuova ISO non aggiorna il vecchio sistema installato.

VM Q35/AHCI x86-64, Secure Boot disattivato, almeno 512 MiB RAM (1 GiB consigliato). Installare soltanto su **nuovo disco virtuale SATA vuoto dedicato**, almeno 96 MiB consigliati. Creare account live, aprire Installa tinymacOS, controllare e confermare la destinazione. Dopo “Il disco è pronto”, spegnere, rimuovere ISO e avviare da disco con la stessa password. Non usare dischi fisici o dati importanti.

## Desktop e Recupero

F1–F9 app; F2 Terminale; Ctrl+Spazio Cerca; Ctrl+Maiusc+Q blocco sessione. Finder mostra i bundle e file reali. Temi e wallpaper storici non identificano la release attuale.

Premere ripetutamente R o F12 durante la mela per Recovery ottica. L’installer copia BaseSystem nella partizione Recovery per recupero locale senza ISO. `sw_vers`: `tinymacOS Recovery 10.1.4 (30B5)`. Runtime distinto da macOS/Internet Recovery Apple.

V abilita verbose; `bootverbose status`, `on`, `off`, `log [pagina]`. Persistenza richiede sistema installato. VM/MMU multi-CPU, TLS e sincronizzazione nativa sperimentali, non libc/POSIX/pthread completa.

## Prove e storico

- [`workspace/release-10.1.4-30B5`](../workspace/release-10.1.4-30B5/): nuove ricevute, confronto Linux, controlli negativi e pixel reali.
- [`workspace/release-10.1.3-30B4`](../workspace/release-10.1.3-30B4/): release precedente; i suoi PASS non annullano il riscontro fisico negativo.
- [`workspace/release-2.4.5-input4`](../workspace/release-2.4.5-input4/): riferimento 21B9.
- [`workspace/webkit-tmc`](../workspace/webkit-tmc/): regressioni kernel/runtime e sonde QA separate, non da pubblicare.
- `docs/` e `docs/storico/`: documenti storici, non identità della release corrente.
