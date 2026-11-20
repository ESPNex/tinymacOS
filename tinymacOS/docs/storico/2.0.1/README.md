# tinymacOS 2.0.1 · Riviera

Sistema desktop sperimentale x86-64, con kernel proprio, processi in Ring 3, driver USB/SATA e tre ambienti distinti. Non usa Linux o XNU. Le app hanno un contenitore Mach-O e una **ABI nativa tinymacOS**: non c’è compatibilità generale con app per macOS o con hardware Apple.

**[Scarica la ISO 2.0.1 da Drive](https://drive.google.com/uc?id=1-Ja9qcEr34D8eSdNr9vsUULIwaleUHoI&export=download)** · [Solo BaseSystem.img](https://drive.google.com/uc?id=12HFtgI4Ma8_AkWs44Cg-UzTyC3ZK0QEg&export=download) · [Verifica SHA-256](consegna/tinymacOS-2.0.1-SHA256.txt)

**Novità della build 20A2:** `usb-tablet` è il puntatore predefinito in QEMU. La freccia raggiunge la posizione indicata dall’host invece di sommare movimenti relativi. Funziona dal setup e dal login fino alla scrivania e al Recupero offline. Il profilo `usb-mouse` resta disponibile. [Configurazione e collaudo del puntatore](consegna/Riviera-2.0.1.md).

Per una prima installazione basta la ISO: contiene già l’immagine di recupero. Non aggiorna automaticamente un vecchio disco di sistema.

| Avvio | A cosa serve | Contenuto |
|---|---|---|
| **ISO live** | Provare le app e installare | Scrivania completa, account live, app **Installa tinymacOS** |
| **Disco SATA installato** | Lavorare e conservare i dati | 12 app, documenti e account permanenti. **Nessuna app di installazione** |
| **Recupero** | Manutenzione senza rete e senza CD | Runtime UEFI autonomo: ricostruzione del sistema, copia dati, SOS, inizializzazione, shell e disco di avvio. Nessun WindowServer, Dock o app del desktop |

## Come avviare e installare

Configura QEMU/OVMF **Q35 x86-64**, un vCPU, almeno 512 MiB di RAM, Secure Boot spento, AHCI con disco virtuale vuoto dedicato (96 MiB consigliati), xHCI con tastiera USB e puntatore assoluto QEMU `usb-tablet`. Per il mouse relativo avvia il launcher con `TMC_POINTER=mouse`. Avvia dalla ISO. **Crea un account**: nessuna password preimpostata. Per il disco permanente apri *Installa tinymacOS* dalla **scrivania live**. Controlla la destinazione e confermala. L’assistente legge l’intero disco prima della prima scrittura; non forza mai l’installazione su un disco non vuoto.

Alla schermata **Il disco è pronto** spegni, **rimuovi la ISO**, lascia collegato il disco SATA e riaccendi. Accedi con la stessa password. Non usare dischi fisici o unici esemplari di dati importanti. Il sistema non cifra i dati. Vedi [guida di avvio](docs/QUICKSTART.txt).

## Cosa usare

- **Finder:** cartelle `/Applications`, `/System/Library`, `/Library`, `~/Library`, `/Users` e `/Volumes`. I bundle sono vere directory: clic destro → *Mostra contenuto pacchetto*. `Ctrl+Maiusc+K` apre la cartella del **KernelCache effettivamente avviato**; selezionalo e premi Spazio per leggerne i byte e il digest. La Libreria di sistema contiene anche il font e gli sfondi letti dal compositor.
- **Terminale (F2):** esegue programmi dai veri percorsi `/bin`, `/usr/bin`, `/usr/sbin`. La shell gestisce virgolette, `~`, `$PWD`/`$HOME`/`$USER`/`$?`, cronologia, completamento, `|`, `<`, `>` e `>>`. Ad esempio `echo 'Alba sul mare' > Documents/nota.txt`, poi `cat Documents/nota.txt | wc -w`. `help` mostra i comandi; `open -a 'Disk Utility'` apre l’app. È una shell locale nativa, non una copia di zsh.
- **Utility Disco:** visualizza i dispositivi reali e i volumi; SOS controlla struttura TinyFS, GPT e integrità del volume di sistema Riviera. Può ripristinare una delle GPT da una copia valida. Dal disco installato *Copia dati* e *Recupero…* accedono ai flussi di salvataggio e manutenzione. Le scritture distruttive richiedono Recupero.
- **Note e Memo:** creazione, modifica, salvataggio automatico, ricerca, eliminazione con conferma e colore per i memo. I file si trovano in `~/Library/Application Support/{Notes,Stickies}`. Sono affiancati da Finder, Impostazioni, Terminale, TextEdit, Calendario, Calcolatrice, Galleria, Monitor Attività, Info e Utility Disco.
- **Scrivania e accesso:** logo fornito dall’utente e barra all’avvio, indicatore a 12 tratti al login, menu contestuali, otto sfondi (incluso il nuovo *Riviera*) e cinque temi. `Ctrl+Spazio` apre Cerca, `Ctrl+Maiusc+A` la libreria app e `Ctrl+Maiusc+Q` blocca lo schermo.

## Recupero offline

Premi **R durante la mela** o, dal disco installato, scegli **Utility Disco → Recupero…**. La partizione di recupero contiene la stessa immagine FAT16 da 4 MiB fornita sulla ISO come `/Recovery/BaseSystem.img`; si può anche avviare l’immagine come supporto USB indipendente.

La reinstallazione legge la **sorgente locale verificata** sul disco già installato; non scarica un sistema e non inizializza i documenti. Il ripristino dati richiede una copia creata in precedenza. La copia è sullo **stesso disco**, non è un backup esterno né un’implementazione di Time Machine. Senza dischi l’immagine di recupero si avvia comunque ma non finge di avere una destinazione. Per una prima installazione su disco vuoto usa la ISO.

## Compilare senza lasciare media nel workspace

**Divieto permanente: nessuna ISO o ZIP sotto `/home/user`, nemmeno temporaneamente per l’upload.** `build.sh`, `tools/iso.sh` e `tools/sourcekit.py` validano la destinazione risolta. Il percorso di build predefinito è `/var/tmp/tinymacos-<uid>/2.0.1`. In questo progetto restano i sorgenti; sul guest non vengono distribuiti.

Su Debian 13 amd64 servono GCC, binutils, GNU-EFI, dosfstools, mtools, xorriso e Python 3. Per l’avvio QEMU e OVMF. In alternativa `python3 tools/local-tools.py /var/tmp/tmc-tools` prepara la toolchain bloccata con SHA-256; imposta `TOOLROOT=/var/tmp/tmc-tools`. Gli asset finali sono inclusi nei sorgenti, niente download durante la build ordinaria.

```sh
export TMC_BUILD=/var/tmp/tinymac-riviera
./build.sh
STATE=/var/tmp/tinymac-vm ./run.sh
# Dopo l’installazione e lo spegnimento:
STATE=/var/tmp/tinymac-vm BOOT_DISK=1 ./run.sh
# Recupero standalone senza disco o ISO:
RECOVERY_IMG="$TMC_BUILD/BaseSystem.img" LIVE=1 STATE=/var/tmp/rec-vm ./run.sh
```

`DISK` può indicare un disco raw esistente. Per importare un vecchio volume TinyFS usa una copia offline come `LEGACY_DATA` sul CD live e una nuova destinazione vuota. `LIVE=1` non collega dischi dati; `BOOT_DISK=1` non crea un disco e non collega la ISO. La [console opzionale](tools/console/README.md) è solo un display per la VM sul computer host, non il sistema operativo.

[Architettura](docs/ARCHITECTURE.md) · [Layout del disco](docs/TINYFS.md) · [Collaudo della release](docs/VALIDATION.md). La documentazione delle release precedenti è in `docs/storico/`.

**Limiti:** un solo core, nessun driver di rete/audio, NVMe o accelerazione GPU. USB HID con mappa tasti USA; niente hot-plug o driver universali per PC/Mac. L’avvio di BaseSystem via USB usa il firmware, non un driver desktop di archiviazione USB. TinyFS offre 96 voci e fino a 8 KiB per documento. Il profilo verificato è la VM descritta sopra.

Lo sfondo Riviera è stato generato per il progetto. Logo, caratteri e altri asset: vedi `licenses/`. tinymacOS è indipendente da Apple.
