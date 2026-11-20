# Riviera 2.1.0 · collaudo della build 20B1

**18 novembre 2026.** Le prove qui elencate usano la nuova ISO o dischi installati da essa. Le prove 20A1/20A2 restano [nell’archivio](storico/) e non sono conteggiate. Nessun account o programma di test è stato inserito nei media.

## Media verificati

| File su Drive | Byte | SHA-256 locale e remoto |
|---|---:|---|
| [ISO UEFI con Recupero integrato](https://drive.google.com/uc?id=1KMBbKetxhhZgVnsp7bL4GCBTDI9vrSCy&export=download) | 54.149.120 | `26d159bb37587c11eb6fec9a173730a6519e7af2b546b7ca7fc733b1987d3c74` |

**Un solo oggetto su Drive per la 2.1.0**: nessun DMG/IMG BaseSystem separato. La ricerca per nome restituisce esattamente un file; dimensione, MD5 e SHA-256 confrontati con la copia locale, `can_download=true`, nessuna ACL modificata. Due compilazioni in directory esterne hanno prodotto **byte identici**: ISO, `BaseSystem.dmg`, ESP installata, due cataloghi, KernelCache, tre eseguibili EFI e manifest. [Build](../tests/evidence/riviera-2.1.0/build.json) · [Drive](../tests/evidence/riviera-2.1.0/drive.json).

## BaseSystem dentro la ISO e sul disco

Questo è il requisito centrale della 20B1; ogni affermazione qui sotto ha la propria evidenza.

| Verifica | Risultato osservato |
|---|---|
| File reale nel supporto | `/Recovery/BaseSystem.dmg` estraibile dalla ISO (ISO9660/Rock Ridge e FAT di avvio El Torito): 3.147.889 byte, UDIF v4 non compresso. SHA-256 `a3abd61b2b7d76216b9386eac207be24cd2790ffe0d4a69da230c9814cd22c6f`. |
| Decodificatore indipendente | `dmg2img` (pacchetto Debian verificato via SHA-256) ha decodificato il file: volume FAT16 identico settore per settore all’estrazione della mappa `blkx`/`mish` interna. La property list, i CRC32 e il trailer `koly` sono consistenti. |
| Contenuto | Il volume contiene **solo** `/EFI/BOOT/BOOTX64.EFI`, il runtime di Recupero nativo. Niente bundle installer, WindowServer o app desktop dentro il DMG. |
| Copia sull’hard disk | Due estrazioni indipendenti — xorriso dalla ISO e mcopy dalla partizione GPT 3 del disco installato — producono due file **identici byte per byte**. Il disco conserva `/Recovery/BaseSystem.dmg` come file nel proprio volume FAT16, non un disco grezzo. [contained.json](../tests/evidence/riviera-2.1.0/contained.json) |
| Avvio dal disco, CD scollegato | NVRAM nuova, **nessuna ISO**, tastiera e tablet: R alla mela apre il DMG locale; due clic aprono la shell. `sw_vers` → 2.1.0; `diskutil info /` dichiara origine «disco interno, partizione Recovery» e «3147889 byte verificati» — valori ricavati dal device path UEFI del volume letto. [screenshot](../tests/evidence/riviera-2.1.0/recupero-disco-locale.png) |
| Avvio dalla sola ISO | VM con **un solo dispositivo di archiviazione** (il CD): nessun disco SATA, nessuna USB storage, nessuna immagine esterna. R → Recupero → shell, origine «ISO di installazione». [screenshot](../tests/evidence/riviera-2.1.0/shell-recupero-cd.png) |
| DMG alterato | Su un clone, cambiato **un solo spazio** nel metadato XML (né settore FAT né eseguibile): il loader rifiuta il Recupero e torna al firmware senza scrivere nulla; SHA-256 del disco invariato. Nessun secondo runtime viene provato. [screenshot](../tests/evidence/riviera-2.1.0/dmg-ricusato.png) |
| Manifest di installazione | Il loader della ISO verifica SHA-256 dell’intera Recovery HD (prefisso FAT + DMG) prima del passaggio al kernel; l’installatore scrive e rilegge ogni blocco. Gli stessi SHA-256 del DMG sono incorporati nei loader **dopo** la costruzione del runtime. |
| Console | Un solo endpoint di download (`/cd-di-avvio`); `/base-system` risponde 404. Il tasto *Invia R* durante la mela apre il Recupero dalla stessa VM. |

Il confronto è con la recoveryOS degli Intel Mac (`BaseSystem.dmg` nella partizione interna, verificata prima dell’avvio): [Apple Platform Security](https://support.apple.com/guide/security/recoveryos-and-diagnostics-environments-sec2512a0c09/web). tinymacOS non usa firme, firmware o immagini Apple.

## Prove specifiche del puntatore

QEMU 10.0.13, x86-64 Q35, OVMF con Secure Boot spento, TCG, un vCPU, 512 MiB, GOP 1280×800, xHCI. i8042 disabilitato, niente rete o secondo puntatore. Il test controlla **`query-mice`**, `info usb` e il framebuffer di ciascuna VM.

| Percorso | Risultato osservato |
|---|---|
| Coordinate tablet | 8 posizionamenti sulla schermata di setup, compresi tutti e quattro gli angoli e ritorno alle stesse coordinate dopo un salto. Confronto dello sprite composto con i pixel GOP. Ripetuto in due punti sulla scrivania e nel Recupero. |
| Pulsanti | Campi dell’account e conferme col clic; Finder spostato di **(+87,+41)** pixel misurati; menu destro; doppio clic sull’installer; rotella nella cronologia del Terminale. |
| Disco e Recupero | Accesso col pulsante freccia sul disco installato senza CD. In Recupero **avviato solo dalla ISO**: selezione *Terminale* + *Continua* con due clic, `sw_vers` → 2.1.0. Nessun SATA o USB storage presenti. |
| Regressione mouse | Avvii distinti con **solo** `usb-mouse`: pixel del cursore, account, drag misurato, menu destro, login da disco e shell di Recupero. |
| Display remoto | Via WebSocket e RFB 3.8: clic assoluto su *Crea account*; in Recupero, R inviato durante la mela → apertura della shell. Download HTTP identico alla ISO di build. |

**12 gruppi puntatore** (6 tablet + 6 mouse); [pointer.json](../tests/evidence/riviera-2.1.0/pointer.json) · [console](../tests/evidence/riviera-2.1.0/console.json). Il test C verifica inoltre 160 combinazioni angoli/pulsanti su cinque geometrie, segno della rotella, ritaglio, report ID e rifiuto di descrittori non supportati. Non è stato provato hardware fisico né un tablet con penna/pressione.

## Regressioni del sistema nativo

**39 gruppi** eseguiti sulla 20B1: [14 media/live/installazione](../tests/evidence/riviera-2.1.0/live.json), [9 disco/Recupero](../tests/evidence/riviera-2.1.0/disk.json), [3 sicurezza e avvii autonomi](../tests/evidence/riviera-2.1.0/guard.json), [3 integrazione BaseSystem](../tests/evidence/riviera-2.1.0/contained.json), [5 app](../tests/evidence/riviera-2.1.0/apps.json), [1 integrità copie](../tests/evidence/riviera-2.1.0/integrity.json), [4 runtime](../tests/evidence/riviera-2.1.0/runtime.json). Separate: [3 campioni CPU](../tests/evidence/riviera-2.1.0/cpu-login.json) e [4 verifiche console](../tests/evidence/riviera-2.1.0/console.json).

- ISO → installazione su SATA vuoto → riavvio **solo disco** con NVRAM nuova. GPT primaria/secondaria valide, ESP installata identica alla distribuzione, partizione Recovery con il file DMG identico alla sorgente, installer assente dal disco installato. Terminale, documenti, Note/Memo e persistenza controllati con dati reali.
- Recupero sullo stesso disco: copia/ripristino dati, SOS della GPT, **reinstallazione offline con dati intatti**, disco di avvio, inizializzazione con parola di conferma. Negative su cloni: disco non vuoto, sorgenti e copie alterate rifiutati senza scrivere (SHA-256 del disco invariato).
- R sulla ISO senza alcun altro supporto di archiviazione; `diskutil info /` riporta origine e misura del DMG effettivamente letto.
- CPU: 519 campioni atomici della vCPU, Ring 0 e Ring 3, tre CR3 utente, CR0.WP ed EFER.NXE. Logo del boot col raster e 12 fasi di login. SHA-256 e PBKDF2 confrontati con `hashlib`.

## Ripetere le prove

`./build.sh` con `TMC_BUILD` **esterno al workspace**. `python3 tests/integration.py --work /var/tmp/qa-riviera-20B1` usa nuovi dischi e cloni; impostare `TOOLROOT` se necessario. Il confronto ISO↔partizione e l’avvio locale sono in `tests/riviera_contained.py` (usa il fixture di `riviera_live.py`); per i soli puntatori `tests/riviera_pointer.py` (`--disk` opzionale); per la console `tests/riviera_console.py URL_CONSOLE "$TMC_BUILD" /var/tmp/qa-console-20B1`. In `tests/riviera_guard.py` l’avvio ottico non allega alcun disco né USB storage. `RECOVERY_IMG` con la 2.1.0 viene rifiutato dal launcher: il Recupero è già nella ISO.
