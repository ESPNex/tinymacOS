# TinymacOS: baseline senza browser e avvio del port WebKit

## ISO su Google Drive

[Scarica tinymacOS 2.4.0 — baseline senza browser](https://drive.google.com/file/d/1-h_Cc3aw2NX1ggj9_PM0BRM1oGbUeMXy/view)

Nome su Drive: `tinymacOS-2.4.0-WebKit-baseline-x86_64.iso`.

**La parola WebKit nel nome indica il punto di partenza del nuovo progetto: questa ISO NON contiene WebKit né un altro browser.**

- Dimensione: **118.239.232 byte** (circa 112,8 MiB).
- SHA-256: `be8972eb3377ef3093c91bef673be017469d3f201dfbb1c0f48ab8886366386d`.
- Upload completato; dimensione e SHA-256 restituiti da Drive coincidono con il file locale.
- File locale esterno al workspace: `/var/tmp/tmc-webkit-baseline/tinymacOS-2.4.0-x86_64.iso`.
- Nessuna ISO o ZIP creata sotto `/home/user`.
- Permessi Drive non modificati: il file è accessibile con l'account connesso, non è stato reso pubblico.

## Stato sorgenti aggiornato

Su richiesta esplicita sono stati eliminati il vecchio NetSurf, le librerie del port, il registro storico non compresso, gli oggetti precompilati e i backup vendor. Il nuovo backend non dipende più da quei percorsi. Sono stati conservati solo nove header utilizzati dal backend, con la licenza musl, in `/home/user/workspace/webkit-tmc/posix/libc-headers/`.

TinymacOS e il nuovo progetto WebKit restano in forma di file sorgente separati. Compilazione freestanding e test host del backend ripetuti con successo dopo la migrazione. L'implementazione libc completa, errno per-thread e il motore WebKit funzionante restano da realizzare.

## Verifica USB aggiuntiva

È stata costruita un'immagine GPT/FAT32 copiando i file della radice dell'ISO, equivalente al percorso di copia file della modalità ISO. Avviata su QEMU/OVMF come dispositivo USB mass-storage xHCI, senza CD collegato: raggiunta e verificata visivamente la schermata live **Crea un account**, non Recupero (`usb-iso-mode-boot.png`).

Questo non è Rufus eseguito su Windows e non è un test su hardware fisico. La causa del ripiego legata all'assenza di System.vol nella radice del supporto è corretta nell'ISO attuale; restano possibili limiti firmware/driver e file corrotti. R/F12, un flag di Recovery o il fallimento dei controlli sui payload possono ancora aprire Recupero.

## Build e verifiche

Comando eseguito dal progetto TinymacOS:

```sh
TOOLROOT=/var/tmp/tmc/tools/root \
TMC_BUILD=/var/tmp/tmc-webkit-baseline bash build.sh
```

- Build release completata: **0 errori, 7 warning** su parametri/variabili inutilizzati; log conservato in `baseline-build.log`.
- SHA-256 e PBKDF2 confrontati con hashlib: test superati.
- Bundle live: **14 app**, incluso l'installer; bundle installato: **13 app**, senza installer; **2 kext** separati in entrambi.
- Audit Mach-O: eseguibili distinti, identificatori distinti, segmenti W^X e entry point verificati.
- Audit supporto: KernelCache identico su live/installato, hash corretti; BaseSystem.dmg e payload Recovery coerenti e verificati; installer assente dall'ESP installato; nessun sorgente nei cataloghi distribuiti.
- Avvio **della ISO release effettivamente caricata** in QEMU/OVMF fino alla schermata grafica «Crea un account»: `baseline-boot.png`. La VM risultava running.
- In questa sessione NON eseguiti: installazione completa su disco, regressione completa di tutte le app, avvio Recovery interattivo, test su PC fisico o chiavetta preparata realmente da Rufus. Il test USB simulato aggiuntivo è descritto sopra.

## Rufus su Windows

**Attenzione: Rufus cancella la chiavetta selezionata. Non usare unità con dati da conservare. Anche l'installazione di questo sistema sperimentale va fatta soltanto su un disco dedicato e sacrificabile, preferibilmente prima in VM.**

1. Scarica l'ISO da Drive e controlla il digest in PowerShell:

   ```powershell
   Get-FileHash .\tinymacOS-2.4.0-WebKit-baseline-x86_64.iso -Algorithm SHA256
   ```

2. Apri Rufus e seleziona con attenzione la chiavetta corretta. Scegli **Disco o immagine ISO**, quindi il file scaricato.
3. Scegli **GPT**, destinazione **UEFI (non CSM)** e **FAT32**. Avvia la scrittura; se viene chiesto il metodo, scegli **modalità immagine ISO**, non DD.
4. Nel firmware del PC disattiva **Secure Boot** e avvia la voce **UEFI: nome chiavetta**. È un sistema x86-64: non avvia su Apple Silicon.
5. Alla schermata iniziale crea un account. Per l'installazione usa l'app Installa tinymacOS solo su un disco vuoto dedicato. **R/F12** durante la schermata di avvio apre Recupero.

Questa ISO ha boot UEFI El Torito ma **non ha una tabella disco ibrida nella System Area**: non va trattata come un'immagine USB raw da scrivere in DD. La modalità ISO copia i payload previsti anche nella radice FAT della chiavetta. Il supporto hardware rimane sperimentale e non è stato verificato con Rufus in questa sessione. Un avvio UEFI riuscito non garantisce la disponibilità di tutti i driver storage/input del PC.

Profilo VM di riferimento: Q35/OVMF x86-64, 2 vCPU, 1 GiB RAM, AHCI/SATA, xHCI con tastiera e usb-tablet, e1000 per rete. I driver generici per qualunque PC, NVMe e Wi-Fi fisico non sono garantiti.

## DNS e WebKit

Il resolver IPv4 è presente in `kernel/net.c` (`net_resolve`), esposto come `S_NET/3`; anche `S_NET/4` risolve i nomi prima di aprire TCP. Questo è un riscontro sui sorgenti, non un nuovo collaudo DNS end-to-end del guest.

Nuovo progetto: `/home/user/workspace/webkit-tmc/`.

Primo codice scritto e verificato:

- ponte syscall x86-64 nativo;
- backend DNS/TCP con gestione scritture parziali, limiti syscall, would-block ed EOF;
- clock monotono basato sui tick reali;
- API per entropia che fallisce esplicitamente con ENOSYS, senza fallback insicuri;
- test di contratto host con ASan/UBSan superati;
- compilazione freestanding dei tre file backend superata, senza link di una libc o del motore.

È stato fissato un commit WebKit upstream e scaricato **solo un piccolo sottoinsieme espanso di riferimento**, non il motore completo. Nessuna build JSC/WebCore è stata eseguita. Restano da implementare runtime C/C++, POSIX/threading/VM/fd/poll, CSPRNG utenti, TLS verificato, grafica/font, WebCore e shell del browser. Dettagli in `../webkit-tmc/README.md` e `LIMITATIONS.md`.

**Safari è un'applicazione, WebKit è il motore.** L'interfaccia può essere sviluppata con Safari come riferimento, ma non si promettono identità assoluta, integrazioni Apple/DRM o compatibilità con tutti i siti.
