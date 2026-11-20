> **Corrente: manutenzione input 2.4.5 Levante / 21B6.** [Note, checksum, prove e limiti](../workspace/release-2.4.5/RELEASE.md). Le note OC2 immediatamente sotto sono il riferimento storico dei percorsi di avvio conservati; non la versione corrente. Nessuna conferma sul PC fisico.

> **Revisione installatore OC2:** [v2.4.4-oc2](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-oc2) aggiorna la manutenzione **2.4.4 Levante / 21B5**, senza browser. Live CD BIOS+UEFI, disco installato BIOS/OpenDuet/OpenCore e UEFI/OpenCore; installazione grafica, ISO rimossa, login, riavvio, documento persistente e Recupero offline verificati in QEMU. [Note, limiti, checksum e screenshot](../workspace/release-2.4.4/installer-media/RELEASE.md).

> OC2 include anche il TLS nativo per-thread già verificato con 1/4/8 vCPU, non pthread/libc completa né WebKit/HTTPS. Le note storiche UEFI-only sotto descrivono la ISO originale `v2.4.4`, non OC2. Usare il tag OC2 per ricostruire questa revisione.

# tinymacOS 2.4.5 · Levante

Sistema desktop sperimentale x86-64 con kernel proprio, processi Ring 3, ABI nativa e runtime UEFI di Recupero. Non usa Linux o XNU e non esegue generalmente applicazioni per macOS.

## Release corrente

**Levante 2.4.5, build 21B6**. Pagina release: https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.5 . La repository e i download sono privati: occorre accesso GitHub autorizzato. Prima della pubblicazione la pagina potrebbe non essere ancora disponibile.

Questa manutenzione è stata richiesta con priorità per mouse fermo al login e lag, prima di riprendere i grandi blocchi WebKit. **Non include Safari o WebKit funzionante.** La precedente riserva del numero 2.4.5 al browser è superata. Il browser reale resta da completare: HTTPS/HTML/CSS/JavaScript, UI classica e integrazione con i temi. [Note input 2.4.5, prove e limiti](../workspace/release-2.4.5/RELEASE.md).

Novità input 2.4.5:

- Movimento al login con due rettangoli di danno del cursore, non ridisegno completo per ogni report; hover e pulsanti mantengono il ridisegno della scena.
- Accorpamento movimenti in coda con barriere per transizioni pulsanti, ruota e tasti.
- Recupero limitato degli endpoint xHCI in halt, recupero EP0 e fallback mouse Report solo con protocollo confermato e descriptor valido; buffer DMA allineati.
- F11 al login: diagnostica input nativo senza contenuto dei tasti o password. Driver nativo xHCI su porte radice; niente supporto promesso per hub o EHCI/OHCI/UHCI.

Funzionalità mantenute dalla 2.4.4:

- VM multi-CPU con invalidazione TLB e coordinamento degli accessi utente; prove VM/MMU e stress concorrente separati dai test browser.
- Verbose selezionabile con V durante la mela; `bootverbose status`, `on`, `off`, `log [pagina]` dal Terminale. La preferenza persistente richiede un sistema installato e viene letta al prossimo riavvio.
- Versione/build aggiornate nelle app, `sw_vers`, `uname`, metadati bundle, installer, Recupero e documento iniziale. Corrette anche le etichette disegnate dell’Info e dell’installer: **LEVANTE**, non RIVIERA.
- Restano puntini password compatti, gestione dei piccoli movimenti USB relativi e correzioni GOP della 2.4.3.

**La ISO OC2 e il disco installato supportano BIOS e UEFI nei profili QEMU documentati.** Il ponte MBR legge OpenDuet dallo spazio riservato prima dell’ESP; la GPT/FAT16 e le copie Recovery/sistema restano invariate. Non è una migrazione automatica dei dischi installati con versioni precedenti. Nessuna promessa di compatibilità hardware universale.

## Avvio e installazione

Usare una VM x86-64 con OVMF/UEFI, Secure Boot disattivato, Q35/AHCI, almeno 512 MiB di RAM (1 GiB consigliato), tastiera USB e mouse USB o tablet QEMU su xHCI.

```sh
python3 tools/local-tools.py /var/tmp/tmc/tools/root
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-build-245 bash build.sh
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-build-245 STATE=/var/tmp/levante-vm LIVE=1 bash run.sh
```

Per installare, collegare un **nuovo disco virtuale SATA vuoto** dedicato, almeno 96 MiB consigliati. Creare l’account sulla live, aprire *Installa tinymacOS*, controllare la destinazione e confermare. A «Il disco è pronto», spegnere, rimuovere l’ISO e riavviare dal disco con la stessa password. Non usare dischi fisici o dati importanti. Un semplice riavvio dalla nuova ISO non aggiorna un sistema già installato.

Su USB/Rufus usare modalità **ISO**, schema GPT/UEFI senza CSM, FAT32 e Secure Boot disattivato; la modalità DD non è supportata da questa ISO non ibrida. La scrittura cancella la chiavetta selezionata. Non sono stati eseguiti test Windows/Rufus o su hardware fisico.

## Desktop e Recupero

F2 apre il Terminale; F1–F9 aprono app. Ctrl+Spazio apre Cerca; Ctrl+Maiusc+Q blocca la sessione. Finder mostra i veri file/bundle di sistema; il desktop include editor, Note/Memo, Galleria, Monitor, Impostazioni e Utility Disco. I temi e gli sfondi storici chiamati Riviera/Sirocco non sono il nome della release e non vanno rinominati retroattivamente.

Premere ripetutamente **R o F12 durante la mela** per aprire `/Recovery/BaseSystem.dmg`, presente nella stessa ISO e copiato nella partizione Recovery dall’installer. Nessun messaggio Recovery viene sovrapposto al normale avvio. Recovery è un runtime UEFI distinto, non Internet Recovery o un ambiente Apple. `sw_vers` nella Shell di Recupero riporta 2.4.5 (21B6).

```text
bootverbose status
bootverbose on
bootverbose off
bootverbose log
bootverbose log 1
```

On/off non cambiano retroattivamente l’avvio corrente. Sulla live in sola lettura sono rifiutati; V consente comunque un avvio verbose. I log del loader sono visibili durante l’avvio; il comando log legge il ring di avvio kernel dopo autenticazione. Il tracing diagnostico dettagliato resta escluso dalla build retail.

## Sorgenti, backup e limiti

**Nessuna ISO o ZIP sotto `/home/user`, nemmeno temporaneamente.** Build/media/toolchain devono restare sotto `/var/tmp` o altra destinazione esterna validata. Il checkout conserva sorgenti e vendor selezionati estratti con licenze; le credenziali non sono sorgenti. GitHub conserva commit/tag verificati; la directory temporanea non è il backup.

La precedente manutenzione 2.4.3 resta congelata in `../workspace/maintenance-2.4.3/sources/` e documentata in `CONSEGNA.md` nella stessa directory. Non modificarla per simulare un aggiornamento retroattivo. Per 2.4.4 usare il tag GitHub `v2.4.4` e la manifestazione dei test della nuova release. Per stato di sviluppo e limiti browser: `../workspace/integrated-next/STATUS.md`, `../workspace/webkit-tmc/README.md` e `../workspace/webkit-tmc/release-gates.json`.
