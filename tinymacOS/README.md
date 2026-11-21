# tinymacOS 10.1.5 · Tramontana / 30B6

Sistema desktop sperimentale x86-64 con kernel proprio, processi Ring 3, ABI nativa e runtime UEFI di Recupero. Non usa Linux o XNU e non esegue generalmente applicazioni per macOS. Repository pubblico, ISO negli asset delle release, non in Git.

[Note 10.1.5, risultati e limiti](../workspace/release-10.1.5-30B6/RELEASE.md) · [Confronto sorgente Linux v6.12](../workspace/release-10.1.5-30B6/LINUX-REVIEW.md). Lo stato di pubblicazione effettivo è nella ricevuta `github-release-verification.json`, quando presente; fino alla pubblicazione resta disponibile la precedente 10.1.3.

## Input: situazione reale

**10.1.4: sul PC dell’utente il mouse è fermo sia al login sia in Recovery. Regressione fisica confermata dall’utente; la 10.1.5 non è ancora verificata su quel PC.** La 10.1.3 muoveva soltanto in verticale in Recovery; il riferimento 2.4.5 Levante / 21B9 aveva Recovery funzionante e login verticale.

La 10.1.5 interviene sui seguenti difetti di compatibilità introdotte o aggravate dalla 10.1.4:

- Mouse Boot: negozia prima Boot, senza forzare Report solo perché il descriptor è leggibile e SET/GET_PROTOCOL risponde. Report rimane il percorso per dispositivi non-Boot o quando Boot è rifiutato e Report è verificato. Il descriptor relativo rimane disponibile per i pacchetti numerati già coperti dai test X/Y.
- I frame Boot di 8 byte con padding riservato zero e pulsanti validi non vengono più scartati tutti; i report numerati completi con padding zero sono accettati. ID estranei e frame incompleti restano esclusi fuori da queste forme esplicite. Non si scambiano gli assi e non si sceglie il formato dal verso del movimento. In caso di byte intrinsecamente ambigui tra Boot e Report la compatibilità universale non è garantita.
- Recovery: non disconnette più il driver firmware che possiede il mouse. Conserva SimplePointer e usa il trasporto diretto soltanto sulle interfacce USB non reclamate, ripristinando la proprietà non invasiva della 21B9. Poll di tutti i puntatori e ripresa asincrona diretta conservati.
- EHCI: osserva prima le TD; ferma la schedule periodica soltanto se occorre davvero modificare un QH o recuperare un errore, non ad ogni poll a vuoto. Conservati toggle hardware e protezione degli overlay introdotti dopo la revisione Linux.

Si tratta di difetti e casi di compatibilità software riproducibili, **non di un’acquisizione del traffico del mouse fisico né di una garanzia di soluzione per l’ASRock**. La preferenza Report della 10.1.4 non era stata giustificata da una prova fisica e viene ritirata. Restano i test sul vero codice precedente, non semplici PASS della stessa VM.

PS/2, EHCI, UHCI companion, xHCI, routing Intel e tastiera italiana ISO conservati. Nessun nuovo pannello diagnostico F11. I test ASan/UBSan e QEMU non verificano il PC ASRock Q1900M/J1900; non implementati OHCI, hotplug generale, hub SuperSpeed o HID universale.

## Centro di Controllo e artwork

Conservate dalla 10.1.4: 13 icone originali in `assets/control-center/*.svg`, sorgenti autorevoli. `tools/control-svg.py` le compila deterministicamente in maschere alpha antialias a 64 px, tinteggiate dal renderer nativo; non è un interprete SVG/XML a runtime. Gli SVG e il manifest sono installati nel filesystem. Il codice geometrico C precedente delle icone del pannello è rimosso.

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
  /var/tmp/tmc1014/tinymacOS-10.1.5-x86_64.iso \
  /var/tmp/tmc1014-optical/tinymacOS-10.1.5-x86_64.iso
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc1014 \
  STATE=/var/tmp/tramontana-vm LIVE=1 bash run.sh
```

Richiesti Python/Pillow e gli strumenti dei lock. Font già rasterizzato nella normale build; `tools/tramontana-font.py` rigenera la faccia semibold dal TTF incluso. Il compilatore delle icone SVG non scarica risorse o librerie di rete.

ISO ottica UEFI nativa + CD BIOS ISOLINUX/MEMDISK → OpenDuet/OpenCore → loader nativo. **Non è un’immagine USB-DD ibrida.** Sistemi installati GPT/FAT16 con ponte MBR nello spazio riservato, collaudati BIOS/UEFI in VM. Avviare la nuova ISO non aggiorna il vecchio sistema installato.

VM Q35/AHCI x86-64, Secure Boot disattivato, almeno 512 MiB RAM (1 GiB consigliato). Installare soltanto su **nuovo disco virtuale SATA vuoto dedicato**, almeno 96 MiB consigliati. Creare account live, aprire Installa tinymacOS, controllare e confermare la destinazione. Dopo “Il disco è pronto”, spegnere, rimuovere ISO e avviare da disco con la stessa password. Non usare dischi fisici o dati importanti.

## Desktop e Recupero

F1–F9 app; F2 Terminale; Ctrl+Spazio Cerca; Ctrl+Maiusc+Q blocco sessione. Finder mostra i bundle e file reali. Temi e wallpaper storici non identificano la release attuale.

Premere ripetutamente R o F12 durante la mela per Recovery ottica. L’installer copia BaseSystem nella partizione Recovery per recupero locale senza ISO. `sw_vers`: `tinymacOS Recovery 10.1.5 (30B6)`. Runtime distinto da macOS/Internet Recovery Apple.

V abilita verbose; `bootverbose status`, `on`, `off`, `log [pagina]`. Persistenza richiede sistema installato. VM/MMU multi-CPU, TLS e sincronizzazione nativa sperimentali, non libc/POSIX/pthread completa.

## Prove e storico

- [`workspace/release-10.1.5-30B6`](../workspace/release-10.1.5-30B6/): nuove ricevute, confronto Linux, controlli negativi e pixel reali.
- [`workspace/release-10.1.3-30B4`](../workspace/release-10.1.3-30B4/): release precedente; i suoi PASS non annullano il riscontro fisico negativo.
- [`workspace/release-2.4.5-input4`](../workspace/release-2.4.5-input4/): riferimento 21B9.
- [`workspace/webkit-tmc`](../workspace/webkit-tmc/): regressioni kernel/runtime e sonde QA separate, non da pubblicare.
- `docs/` e `docs/storico/`: documenti storici, non identità della release corrente.
