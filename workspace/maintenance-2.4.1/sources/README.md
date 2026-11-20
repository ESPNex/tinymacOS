# tinymacOS 2.4.1 · Levante

Sistema desktop sperimentale x86-64, con kernel proprio, processi in Ring 3, driver USB/SATA e tre ambienti distinti. Non usa Linux o XNU. Le app hanno un contenitore Mach-O e una **ABI nativa tinymacOS**: non c’è compatibilità generale con app per macOS o con hardware Apple.

**[Scarica la ISO 2.4.1 di manutenzione da Drive](https://drive.google.com/file/d/1AkKo1USI3fTQbA9G2OE47yolmSwo6TuG/view)** · [Test e istruzioni](../workspace/maintenance-2.4.1/CONSEGNA.md)

**Build 21B2 manutenzione:** decoder USB HID relativo report-protocol, campo password senza caret bianco, Recupero con geometria condivisa con il desktop e trascinamento reale, nessun suggerimento Recupero al boot. Nessun browser incluso. Test QEMU usb-tablet e usb-mouse passati; hardware fisico non verificato.


**Novità della build 21B1 «Levante»:** **Impostazioni completa** — lo *Scuro* ricolora finestre, barre e pulsanti (non più solo sfondo e Dock) e **ogni colore di accento ha la sua variante chiara e scura**; **sistema kext** con driver caricati da `/System/Library/Extensions` e comandi `kextstat`/`kextload`/`kextunload`; **protezione di sistema alla macOS** (`csrutil status/enable/disable`, modifiche solo dalla Shell di Recupero, percorsi di sistema protetti); **rete con lwIP 2.2.1** — driver **e1000** (DHCP e ping reali con `-netdev user -device e1000`) e **Wi‑Fi simulato** (`wifi scan/connect/status`, `ifconfig`, `ping`, MAC visibili, disclaimer onesto); **DPI costante** e leggibile a tutte le risoluzioni fino a **1920×1080**; identità **Levante** ovunque (`sw_vers` 2.4.0, build 21B1). Restano dalla 20C2 Mission Control (F10), i dodici sfondi generati e il ripristino dell’account inaccessibile in due passi dal login. Resta il supporto unico: `BaseSystem.dmg` è un **file reale dentro la ISO**, in `/Recovery/`, identico byte per byte dopo l’installazione nella partizione Recovery, e si avvia anche senza CD e senza rete — un’unica ISO per provare, installare e ripristinare. Il puntatore predefinito resta il `usb-tablet` QEMU. [Dettagli della consegna](consegna/Sirocco-2.3.0.md).

Per una prima installazione basta la ISO. Non aggiorna automaticamente un vecchio disco di sistema.

| Avvio | A cosa serve | Contenuto |
|---|---|---|
| **ISO live** | Provare le app e installare | Scrivania completa, account live, app **Installa tinymacOS**, `/Recovery/BaseSystem.dmg` |
| **Disco SATA installato** | Lavorare e conservare i dati | 12 app, documenti e account permanenti. **Nessuna app di installazione** |
| **Recupero** | Manutenzione senza rete e senza CD | Runtime UEFI contenuto nel DMG: ricostruzione del sistema, copia dati, SOS, inizializzazione, shell e disco di avvio. Nessun WindowServer, Dock o app del desktop |

## Come avviare e installare

Configura QEMU/OVMF **Q35 x86-64**, un vCPU, almeno 512 MiB di RAM, Secure Boot spento, AHCI con disco virtuale vuoto dedicato (96 MiB consigliati), xHCI con tastiera USB e puntatore assoluto QEMU `usb-tablet`. Per il mouse relativo avvia il launcher con `TMC_POINTER=mouse`. Avvia dalla ISO. **Crea un account**: nessuna password preimpostata. Per il disco permanente apri *Installa tinymacOS* dalla **scrivania live**. Controlla la destinazione e confermala. L’assistente scrive il sistema **e copia `/Recovery/BaseSystem.dmg` nella partizione Recovery**, leggendo ogni settore prima della prima scrittura; non forza mai l’installazione su un disco non vuoto.

Alla schermata **Il disco è pronto** spegni, **rimuovi la ISO**, lascia collegato il disco SATA e riaccendi. Accedi con la stessa password. Non usare dischi fisici o unici esemplari di dati importanti. Il sistema non cifra i dati. Vedi [guida di avvio](docs/QUICKSTART.txt).

## Cosa usare

- **Finder:** cartelle `/Applications`, `/System/Library`, `/Library`, `~/Library`, `/Users` e `/Volumes`. I bundle sono vere directory: clic destro → *Mostra contenuto pacchetto*. `Ctrl+Maiusc+K` apre la cartella del **KernelCache effettivamente avviato**; selezionalo e premi Spazio per leggerne i byte e il digest. La Libreria di sistema contiene anche il font e gli sfondi letti dal compositor.
- **Terminale (F2):** esegue programmi dai veri percorsi `/bin`, `/usr/bin`, `/usr/sbin`. La shell gestisce virgolette, `~`, `$PWD`/`$HOME`/`$USER`/`$?`, cronologia, completamento, `|`, `<`, `>` e `>>`. Ad esempio `echo 'Alba sul mare' > Documents/nota.txt`, poi `cat Documents/nota.txt | wc -w`. `help` mostra i comandi; `open -a 'Disk Utility'` apre l’app. È una shell locale nativa, non una copia di zsh.
- **Utility Disco:** visualizza i dispositivi reali e i volumi; SOS controlla struttura TinyFS, GPT e integrità del volume di sistema Riviera. Può ripristinare una delle GPT da una copia valida. Dal disco installato *Copia dati* e *Recupero…* accedono ai flussi di salvataggio e manutenzione. Le scritture distruttive richiedono Recupero.
- **Note e Memo:** creazione, modifica, salvataggio automatico, ricerca, eliminazione con conferma e colore per i memo. I file si trovano in `~/Library/Application Support/{Notes,Stickies}`. Sono affiancati da Finder, Impostazioni, Terminale, TextEdit, Calendario, Calcolatrice, Galleria, Monitor Attività, Info e Utility Disco.
- **Scrivania e accesso:** logo fornito dall’utente e barra all’avvio, indicatore a 12 tratti al login, menu contestuali, dodici sfondi (inclusi *Riviera* e *Levante*) e sette temi. `Ctrl+Spazio` apre Cerca, `Ctrl+Maiusc+A` la libreria app e `Ctrl+Maiusc+Q` blocca lo schermo.

## Recupero integrato, come sul Mac

Premi **R durante la mela** (oppure F12): il loader apre il file `/Recovery/BaseSystem.dmg` dal supporto che sta già avviando. Dopo l’installazione lo stesso gesto, **senza CD e senza rete**, apre il file dalla partizione Recovery del disco: è lo stesso DMG, verificato per intero con il suo SHA-256. Puoi anche scegliere **Utility Disco → Recupero…** dal sistema installato.

La partizione Recovery è un volume FAT16 da 4 MiB con un piccolo bootstrap UEFI e il file `BaseSystem.dmg` in `/Recovery/`, esattamente come il file visibile sulla ISO. Nella shell di Recupero il comando `diskutil info /` mostra il percorso, la provenienza (CD o disco interno) e i byte verificati dell’immagine. Il messaggio di rifiuto per un DMG mancante o alterato non attiva mai una reinstallazione automatica.

Il modello di riferimento è la recoveryOS degli Intel Mac, descritta da Apple in [RecoveryOS and Diagnostics Environments](https://support.apple.com/guide/security/recoveryos-and-diagnostics-environments-sec2512a0c09/web): l’immagine di recupero risiede nella partizione interna e viene verificata prima dell’avvio. Qui non c’è né firmware Apple né firma Apple: i loader di tinymacOS accettano soltanto il profilo UDIF prodotto da questa build.

La reinstallazione legge la **sorgente locale verificata** sul disco già installato; non scarica un sistema e non inizializza i documenti. Il ripristino dati richiede una copia creata in precedenza. La copia è sullo **stesso disco**, non è un backup esterno né un’implementazione di Time Machine. Per una prima installazione su disco vuoto usa la ISO.

## Compilare senza lasciare media nel workspace

**Divieto permanente: nessuna ISO o ZIP sotto `/home/user`, nemmeno temporaneamente per l’upload.** `build.sh`, `tools/iso.sh` e `tools/sourcekit.py` validano la destinazione risolta. Il percorso di build predefinito è `/var/tmp/tinymacos-<uid>/2.4.1`. Anche il `BaseSystem.dmg` intermedio viene creato in una cartella temporanea **esterna** al workspace: solo la ISO finita viene copiata come artefatto. In questo progetto restano i sorgenti; sul guest non vengono distribuiti.

Su Debian 13 amd64 servono GCC, binutils, GNU-EFI, dosfstools, mtools, xorriso e Python 3. Per l’avvio QEMU e OVMF. In alternativa `python3 tools/local-tools.py /var/tmp/tmc-tools` prepara la toolchain bloccata con SHA-256; imposta `TOOLROOT=/var/tmp/tmc-tools`. Gli asset finali sono inclusi nei sorgenti, niente download durante la build ordinaria.

```sh
export TMC_BUILD=/var/tmp/tinymacos-levante
./build.sh
STATE=/var/tmp/tinymac-vm ./run.sh
# Dopo l’installazione e lo spegnimento (solo disco, NVRAM nuova):
STATE=/var/tmp/tinymac-vm BOOT_DISK=1 ./run.sh
# Recupero dal solo disco installato: premi R durante la mela.
```

`DISK` può indicare un disco raw esistente. Per importare un vecchio volume TinyFS usa una copia offline come `LEGACY_DATA` sul CD live e una nuova destinazione vuota. `LIVE=1` non collega dischi dati; `BOOT_DISK=1` non crea un disco e non collega la ISO. La [console opzionale](tools/console/README.md) offre un unico download della ISO ed è solo un display per la VM sul computer host, non il sistema operativo.

[Architettura](docs/ARCHITECTURE.md) · [Layout del disco](docs/TINYFS.md) · [Collaudo della release](docs/VALIDATION.md). La documentazione delle release precedenti è in `docs/storico/`.

**Limiti:** un solo core, nessun driver di rete/audio, NVMe o accelerazione GPU. USB HID con mappa tasti USA; niente hot-plug o driver universali per PC/Mac. Il DMG accettato dai loader è quello costruito da questa build: non è un decodificatore UDIF generico, non monta DMG Apple e non verifica firme Apple. TinyFS offre 96 voci e fino a 8 KiB per documento. Il profilo verificato è la VM descritta sopra.

Lo sfondo Riviera è stato generato per il progetto. Logo, caratteri e altri asset: vedi `licenses/`. tinymacOS è indipendente da Apple.
