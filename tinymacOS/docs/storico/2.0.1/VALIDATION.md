# Riviera 2.0.1 · collaudo della build 20A2

**18 novembre 2026.** Le prove qui elencate usano la nuova ISO o dischi installati da essa. Le prove 20A1 rimangono [nell’archivio](storico/2.0/VALIDATION.md) e non sono conteggiate. Nessun account o programma di test è stato inserito nei media.

## Media verificati

| File su Drive | Byte | SHA-256 locale e remoto |
|---|---:|---|
| [ISO UEFI live/installazione](https://drive.google.com/uc?id=1-Ja9qcEr34D8eSdNr9vsUULIwaleUHoI&export=download) | 55.191.552 | `0c48424f70ab6d385eac27d8f0e8504d9513618e69751a554a8d27481cb96422` |
| [Recupero USB](https://drive.google.com/uc?id=12HFtgI4Ma8_AkWs44Cg-UzTyC3ZK0QEg&export=download) | 4.194.304 | `19dbb3fe57bd8f74f29ddb3df84b7fd126836e0d5903aa68a9b7e0ebf2ed7ceb` |

Due compilazioni in directory esterne hanno prodotto byte identici per entrambi i media, ESP installata, due cataloghi, KernelCache, due eseguibili EFI e manifest. Il Recupero su Drive è identico a quello estratto dalla ISO e alla partizione sul disco installato. Un solo oggetto per ciascun nuovo nome in Drive, dimensioni e checksum confrontati; scaricabili dall’account collegato. Nessuna ACL modificata. [Build](../tests/evidence/riviera-2.0.1/build.json) · [Drive](../tests/evidence/riviera-2.0.1/drive.json).

## Prove specifiche del puntatore

QEMU 10.0.13, x86-64 Q35, OVMF con Secure Boot spento, TCG, un vCPU, 512 MiB, GOP 1280×800, xHCI. i8042 disabilitato, niente rete o secondo puntatore. Il test controlla **`query-mice`**, `info usb` e il framebuffer di ciascuna VM: vedere un dispositivo nell’elenco non costituisce il risultato del test.

| Percorso | Risultato osservato |
|---|---|
| Coordinate tablet | 8 posizionamenti sulla schermata di setup, compresi `(0,0)`, `(1279,799)`, `(1279,0)`, `(0,799)` e ritorno alle stesse coordinate dopo un salto. Confronto dello sprite composto con i pixel GOP, inclusa la punta ritagliata ai bordi. Ripetuto in due punti sulla scrivania e in tre sul Recupero autonomo. |
| Pulsanti | Tutti e tre i campi della creazione account e la conferma selezionati col clic. Finder spostato di **(+87,+41)** pixel misurati e fermo dopo il rilascio; clic destro sullo sfondo; doppio clic sull’icona dell’installer. Rotella su/giù: Terminale passa a *Cronologia* e torna a *Sessione locale*. |
| Disco e Recupero | Accesso col pulsante freccia sul clone del disco installato, senza CD. Su supporto USB Recovery con sola tastiera e tablet: selezione *Terminale* + *Continua*, quindi `sw_vers` mostra 2.0.1. Nessun SATA o rete necessario. |
| Regressione mouse | Avvii distinti con **solo** `usb-mouse`; QMP lo segnala relativo. Pixel del cursore, creazione account, drag misurato/rilascio, menu destro, login da disco e navigazione di Recupero controllati nuovamente. |
| Display remoto | Via WebSocket e RFB 3.8: clic assoluto su *Crea account* con nome vuoto → messaggio di validazione; in Recupero → apertura della shell. I due download HTTP sono identici ai file di build. |

**12 gruppi puntatore** (6 tablet + 6 mouse); [tablet](../tests/evidence/riviera-2.0.1/tablet.json) · [mouse](../tests/evidence/riviera-2.0.1/mouse.json) · [console](../tests/evidence/riviera-2.0.1/console.json). Il test C del decodificatore verifica inoltre 160 combinazioni angoli/pulsanti su cinque geometrie, segno della rotella, ritaglio, report ID e rifiuto di report corti o di descrittori non supportati. Le altre geometrie sono **prove del decodificatore**, non dichiarazioni di test grafici in VM. Non è stato provato hardware fisico o un tablet con penna/pressione.

## Regressioni del sistema nativo

Rieseguiti **36 gruppi** sulla 20A2: [13 media/live/installazione](../tests/evidence/riviera-2.0.1/live.json), [9 disco/Recupero](../tests/evidence/riviera-2.0.1/disk.json), [4 sicurezza installazione e avvii autonomi](../tests/evidence/riviera-2.0.1/guard.json), [5 app](../tests/evidence/riviera-2.0.1/apps.json), [1 integrità copie](../tests/evidence/riviera-2.0.1/integrity.json), [4 runtime](../tests/evidence/riviera-2.0.1/runtime.json). Le quattro verifiche della console sono separate.

- ISO → installazione su SATA vuoto → riavvio solo disco con NVRAM nuova, login con l’account creato sulla live. KernelCache ELF64 verificato, cataloghi con 13 bundle live / 12 installati e 26 percorsi Unix. Installer assente dal disco installato. Terminale, documenti, Note/Memo, verifica digest del Finder e persistenza controllati con dati reali.
- Recupero sullo stesso disco: copia/ripristino dati, riparazione della GPT da copia valida, reinstallazione offline con dati intatti, avvio immediato dal disco, inizializzazione con parola di conferma e nuovo account. Negative su cloni: disco non vuoto, sorgente di sistema e copia dati alterate vengono rifiutati senza scrivere.
- 519 campioni atomici della vCPU: Ring 0 e Ring 3, tre CR3 utente distinti, CR0.WP ed EFER.NXE attivi. Logo del boot confrontato col raster e tutte e 12 le fasi radiali viste al login. SHA-256 e PBKDF2 confrontati con `hashlib`.

## Ripetere le prove

`./build.sh` con `TMC_BUILD` **esterno al workspace**. `python3 tests/integration.py --work /var/tmp/qa-riviera-nuova` usa nuovi dischi e cloni; impostare `TOOLROOT` se necessario. Host QA: Python, NumPy, OpenCV, Pillow; Tesseract facoltativo per messaggi d’errore. Per i soli puntatori: `python3 tests/riviera_pointer.py /var/tmp/qa-puntatori-nuova` (facoltativo `--disk` sul solo fixture di test prodotto da `riviera_live.py`). Per la console: `python3 tests/riviera_console.py URL_CONSOLE "$TMC_BUILD" /var/tmp/qa-console-nuova`.
