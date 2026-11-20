> **Archivio 1.3.** I percorsi nei comandi si riferiscono all’albero sorgente originale della release. Verbali e schermate: `evidence/`. Nel checkout 2.0 il vecchio runner si chiama `tests/legacy_1_3.py`; non è il collaudo di Riviera.

# tinymacOS 1.3 · Orizzonte

Sistema desktop sperimentale **x86-64 nativo**. Il CD contiene un loader UEFI, il kernel e applicazioni Ring 3, non Linux o una pagina web. Può essere avviato live oppure **installato dalla scrivania e avviato a macchina spenta senza CD**. Riprende convenzioni di macOS ma non è Apple macOS, non esegue software per Mac e non è destinato ai Mac fisici.

[**Scarica la ISO 1.3 da Google Drive**](https://drive.google.com/uc?id=16fx2iSZpYGB7Zflvncaq726rc-CMmzTb&export=download) · [SHA-256 e collaudo](docs/VALIDATION.md)

## Installazione in una VM

1. Crea una VM UEFI x86-64 con **Secure Boot disattivato**, 512 MiB di RAM, **SATA AHCI**, **USB xHCI** con tastiera e mouse USB HID relativo. Collega la ISO e un **disco virtuale vuoto dedicato** (96 MiB consigliati). Il riferimento è QEMU Q35 + OVMF; PS/2, IDE legacy e tablet assoluto non sono driver alternativi.
2. Avvia dal CD e crea l’account live. Dal desktop fai doppio clic su **Installa tinymacOS**, oppure usa F11, Cerca o la Libreria app. Scegli il disco, verifica modello e capacità, spunta la conferma e premi *Installa*. Non selezionare un disco fisico.
3. L’assistente controlla che **tutti** i settori siano vuoti, copia il sistema e un’istantanea di account, documenti e preferenze; rilegge i dati prima di completare la GPT. Non inizializza dischi sconosciuti all’avvio e non offre la cancellazione forzata. Un errore interrompe la procedura. Non spegnere durante la copia.
4. Alla schermata **Il disco è pronto**, premi *Spegni*. **Rimuovi solo la ISO**, lascia collegato il disco SATA e riaccendi la VM. Usa la stessa password. Le modifiche successive nella vecchia sessione CD non vengono replicate sul disco.

**Se provieni dalla 1.2.x:** conserva un backup offline di `user.img`. Al boot da CD è possibile collegarlo come *secondo disco SATA*, accedere con la vecchia password e installare su un nuovo disco vuoto. Il vecchio TinyFS viene letto in RAM prima della migrazione: il disco di origine non è la destinazione. Non serve formattarlo.

### Launcher e build su Linux

**Regola del workspace:** la build e il packaging rifiutano di creare ISO o ZIP sotto `/home/user` o dentro il progetto, anche solo come staging. Per i media usa una directory esterna, ad esempio `/var/tmp`.

```sh
./build.sh                      # scrive solo fuori dall'albero del progetto
STATE="$HOME/tinymac-stato" ./run.sh
# Dopo Installazione → Spegni:
STATE="$HOME/tinymac-stato" BOOT_DISK=1 ./run.sh
```

`system.raw` viene creato vuoto **con creazione esclusiva** se manca; `vars.fd` conserva le variabili firmware. `BOOT_DISK=1` non collega il lettore, non controlla la presenza della ISO e richiede il disco esistente. `DISK=/percorso/disco.raw` seleziona un’immagine raw esistente. `LIVE=1` esclude tutti i dischi. `LEGACY_DATA=/percorso/user.img` collega un’immagine dati 1.x in modalità snapshot; senza opzione viene cercata in `STATE/user.img`. Il launcher non sostituisce o ridimensiona un’immagine preesistente.

Su Debian 13 amd64: GCC, binutils, GNU-EFI, dosfstools, mtools, xorriso, Python 3; per il guest QEMU x86-64 e OVMF. Le immagini, i font e le icone sono già pronti: la normale build non usa la rete, Pillow o noVNC. In alternativa `python3 tools/local-tools.py "$HOME/.local/tinymacos-tools"` prepara gli strumenti bloccati e verificati con SHA-256: `export TOOLROOT="$HOME/.local/tinymacos-tools"`. Per firmware in altre posizioni: `OVMF_CODE` e `OVMF_VARS`. `TMC_BUILD` può puntare a una directory assoluta esterna permanente; il default è `/tmp/tinymacos-<uid>/1.3`. Con un CD scaricato: `ISO=/percorso/alla.iso ./run.sh`.

## Scrivania, temi e Finder

- **Aspetto:** Chiaro, Scuro, **Prisma glaciale**, **Aurora boreale**, **Rame vespertino**. I tre nuovi temi cambiano i colori effettivi dei controlli, dei testi, delle selezioni e dei materiali, e scelgono il proprio sfondo. Le sette immagini restano selezionabili indipendentemente. Le preferenze sono persistenti sul sistema installato.
- **Menu contestuali:** clic destro su sfondo o icona della scrivania, Dock, cornice di una finestra, contenuto di TextEdit, elemento o spazio vuoto del Finder. Il Finder distingue scrivibile e sola lettura e seleziona l’elemento colpito; l’eliminazione passa da una conferma. Esc o un clic fuori chiudono il menu. Le frecce e Invio permettono di scegliere senza inseguire il puntatore. I documenti della scrivania corrispondono a `/Users/User/Desktop`.
- **File del sistema:** Finder → Posizioni → **Kernel avviato** (`Ctrl+Maiusc+K`), oppure Disco → System → Kernel. `kernel.elf` è l’ELF64 intero letto dal loader per *questo* avvio; `Avvio/BOOTX64.EFI` espone i byte del medesimo eseguibile UEFI caricato dal volume. La destinazione su FAT è `/EFI/BOOT/BOOTX64.EFI`. Nessun `Sorgenti/`, sorgente C/assembly o mappa del linker nel filesystem del guest. Spazio o doppio clic: byte, lunghezza, paginazione e SHA-256 calcolato leggendo l’intero file. Anche `shasum /System/Kernel/kernel.elf` funziona nel Terminale. I sorgenti per sviluppatori restano nel progetto e nel kit separato.
- **Avvio e login:** la mela fornita dall’utente mantiene la sua sagoma. Al boot appare su nero con barra **sotto**; il menu usa un raster autonomo di 16 px di altezza in una barra di 34 px. Il login usa invece un indicatore monocromatico a 12 tacche (fisso con movimento ridotto). La sessione si blocca con `Ctrl+Maiusc+Q`.
- **App:** F1 Finder, F2 Terminale, F3 TextEdit, F4 Impostazioni, F5 Info, F6 Calcolatrice, F7 Calendario, F8 Monitor Attività, F9 Galleria, **F11 Installa tinymacOS**. F10: panoramica; Ctrl+Spazio: cerca; Ctrl+Maiusc+A: libreria. Dopo l’avvio da disco, l’assistente informa che il sistema è già installato e non riscrive il disco in uso.

## Limiti e verifica

Una vCPU, GOP software, niente rete/audio nel guest, hub USB, hot-plug, NVMe, USB mass-storage, BIOS legacy, SMP, Secure Boot o cifratura. Tastiera USB con mappa USA ASCII; mouse relativo. TinyFS è conservativo: 96 voci e 8 KiB per documento, non un filesystem general-purpose. Gli eseguibili di sistema non sono limitati a 8 KiB. Destinazioni supportate dall’assistente: **64 MiB–128 GiB, settori logici da 512 byte**. Il controllo completo è più lento per immagini grandi. Non usare mai l’unica copia di dati importanti.

Il primo account usa PBKDF2-HMAC-SHA256 (120.000 iterazioni). Se il firmware/CPU non offre un RNG si prova la sorgente jitter con verifiche di salute. Solo se anche questa manca, al primo setup genera sull’host un codice con `openssl rand -hex 16`, oppure Python `secrets.token_hex(16)`. È un sale iniziale pubblico, non una password o una chiave di recupero. Il login esistente non richiede nuovo materiale casuale.

- [Avvio rapido](docs/QUICKSTART.txt) · [driver e ABI](docs/ARCHITECTURE.md) · [layout e protezioni del disco](docs/TINYFS.md) · [collaudo](docs/VALIDATION.md).
- `tests/crypto.py`, `tests/integration.py`, `tests/disk_guard.py`, `tests/desktop_extra.py`, `tests/salt_compat.py`: collaudo solo su VM e dischi temporanei. Impostare `TOOLROOT` se necessario; le suite VM richiedono Pillow. La procedura opzionale di migrazione 1.2.2 usa il launcher della release precedente.
- [Console host facoltativa](tools/console/README.md): trasmette il vero framebuffer del guest; ogni connessione ha un disco **usa e getta**. La persistenza tra spegnimenti richiede il launcher locale o la propria VM. Nessun frontend web nel CD.
- `licenses/` riporta attribuzioni per artwork, font, componenti GNU-EFI e CPU Jitter RNG. `tools/art.py` è solo per rigenerare le risorse (Pillow, NumPy, DejaVu, librsvg).
