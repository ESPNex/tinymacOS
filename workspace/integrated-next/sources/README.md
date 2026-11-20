> **Redeploy ottico:** [v2.4.4-oc1 OpenDuet/OpenCore + UEFI](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-oc1) è ora pubblicato e verificato da CD su SeaBIOS/OVMF. È una revisione del supporto 2.4.4, non una nuova versione OS: il vecchio limite UEFI-only delle note 2.4.4 sottostanti vale per la ISO originale, non per l'avvio ottico della nuova revisione. L'installer rimane UEFI. [Note complete](../workspace/release-2.4.4/opencore-media/RELEASE.md).

> **Sorgenti main dopo il tag:** includono il primo TLS per-thread FS nativo, verificato con 1/4/8 vCPU; è sviluppo WebKit non ancora distribuito, non pthread/libc completa o HTTPS. Per ricostruire esattamente la ISO OpenCore usare il tag `v2.4.4-oc1`, non main.

# tinymacOS 2.4.4 · Levante

Sistema desktop sperimentale x86-64 con kernel proprio, processi Ring 3, ABI nativa e runtime UEFI di Recupero. Non usa Linux o XNU e non esegue generalmente applicazioni per macOS.

## Release corrente

**Levante 2.4.4, build 21B5**. Pagina release: https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4 . La repository e i download sono privati: occorre accesso GitHub autorizzato. Prima della pubblicazione la pagina potrebbe non essere ancora disponibile.

Questa ISO intermedia è stata esplicitamente richiesta dopo il backup GitHub. **Non include Safari o WebKit funzionante.** Prossima patch prevista: **2.4.5 Levante**, solo dopo motore browser reale, HTTPS/HTML/CSS/JavaScript, UI classica e integrazione verificata con tutti i temi. Il nome resta Levante.

Novità rispetto alla manutenzione 2.4.3:

- VM multi-CPU con invalidazione TLB e coordinamento degli accessi utente; prove VM/MMU e stress concorrente separati dai test browser.
- Verbose selezionabile con V durante la mela; `bootverbose status`, `on`, `off`, `log [pagina]` dal Terminale. La preferenza persistente richiede un sistema installato e viene letta al prossimo riavvio.
- Versione/build aggiornate nelle app, `sw_vers`, `uname`, metadati bundle, installer, Recupero e documento iniziale. Corrette anche le etichette disegnate dell’Info e dell’installer: **LEVANTE**, non RIVIERA.
- Restano puntini password compatti, gestione dei piccoli movimenti USB relativi e correzioni GOP della 2.4.3.

**La ISO e l’installer rimangono UEFI.** Il bootstrap MBR/FAT32 SeaBIOS → OpenDuet/OpenCore → sistema nativo è stato collaudato su un disco QA separato. Non è integrato come boot BIOS dell’ISO e non rende BIOS l’installazione GPT/FAT16. Nessuna promessa di compatibilità hardware universale.

## Avvio e installazione

Usare una VM x86-64 con OVMF/UEFI, Secure Boot disattivato, Q35/AHCI, almeno 512 MiB di RAM (1 GiB consigliato), tastiera USB e mouse USB o tablet QEMU su xHCI.

```sh
python3 tools/local-tools.py /var/tmp/tmc/tools/root
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-build-244 bash build.sh
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-build-244 STATE=/var/tmp/levante-vm LIVE=1 bash run.sh
```

Per installare, collegare un **nuovo disco virtuale SATA vuoto** dedicato, almeno 96 MiB consigliati. Creare l’account sulla live, aprire *Installa tinymacOS*, controllare la destinazione e confermare. A «Il disco è pronto», spegnere, rimuovere l’ISO e riavviare dal disco con la stessa password. Non usare dischi fisici o dati importanti. Un semplice riavvio dalla nuova ISO non aggiorna un sistema già installato.

Su USB/Rufus usare modalità **ISO**, schema GPT/UEFI senza CSM, FAT32 e Secure Boot disattivato; la modalità DD non è supportata da questa ISO non ibrida. La scrittura cancella la chiavetta selezionata. Non sono stati eseguiti test Windows/Rufus o su hardware fisico.

## Desktop e Recupero

F2 apre il Terminale; F1–F9 aprono app. Ctrl+Spazio apre Cerca; Ctrl+Maiusc+Q blocca la sessione. Finder mostra i veri file/bundle di sistema; il desktop include editor, Note/Memo, Galleria, Monitor, Impostazioni e Utility Disco. I temi e gli sfondi storici chiamati Riviera/Sirocco non sono il nome della release e non vanno rinominati retroattivamente.

Premere ripetutamente **R o F12 durante la mela** per aprire `/Recovery/BaseSystem.dmg`, presente nella stessa ISO e copiato nella partizione Recovery dall’installer. Nessun messaggio Recovery viene sovrapposto al normale avvio. Recovery è un runtime UEFI distinto, non Internet Recovery o un ambiente Apple. `sw_vers` nella Shell di Recupero riporta 2.4.4 (21B5).

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
