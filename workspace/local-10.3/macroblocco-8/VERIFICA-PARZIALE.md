# M8 — checkpoint UI, non consegna finale

> **STORICO:** stato precedente alla richiesta di revert accento/sfondo. Per lo stato corrente, i fix grafici, gli screenshot Drive e mount/unmount verificato, vedere `PRIORITA-E-MOUNT.md` e `NEXT.md`. I numeri prestazionali sotto non sono misure della build corrente.

**Macroblocco incompleto. Nessun commit/push/tag/release/video M8 pubblicato.** Il main pubblicato rimane `3dab9e79cdfbc1b0e1b9b623de197f9ab415e1ae` e la versione pubblica non cambia in questo checkpoint. Nessun processo di lavoro continua automaticamente dopo questo checkpoint.

## Implementato e verificato localmente

- Spotlight: font normale F_BODY al posto del run C_MONO, metriche proporzionali del font sigillato, viewport/caret/clic coerenti e UTF-8. Non modifica il campo della nuova cartella o l’input hardware.
- Aiuto → Scorciatoie: legenda Command/Control/Option/Maiusc con equivalenti Windows; otto azioni verticali. Command usa il glifo esistente; gli altri simboli sono geometria UI originale, senza alterare font bloccati. Il pulsante Fine ha il proprio rettangolo cliccabile, non il vecchio hitbox esteso.
- Aspetto: il cambio del solo accento aggiorna la palette senza ricopiare/ridisegnare lo sfondo o riavviare una dissolvenza inutile. Scaling sfondi: tabella X per colonna anziché divisione per pixel; risultato confrontato pixel per pixel con la formula originale a 640/1280/1920, in chiaro/scuro.
- L’intera build live/Recovery produce l’ISO di verifica. Non è ancora il prodotto finale 10.3.

### Test superati

- `bash tests/test-appearance8.sh`: normale + ASan/UBSan. Font realmente distribuito; hit/caret/viewport UTF-8; clipping e ripristino clip; equivalenza del wallpaper con riferimento indipendente.
- `bash tests/test-cursor7.sh`: guard autenticazione/S_ART, risorse CUR/ANI, contesti e pixel/cursore, resize queue; normale + sanitizzato.
- `bash tests/test-animation-performance.sh`: clock, surface cache, glass reference; normale + sanitizzato.
- `python3 tests/test-m0.py`: 568 input font/asset/vendor/USB bloccati, contratti etichette/versione e inventario stringhe. Prima esecuzione segnalava inventario UI non aggiornato; diff rivisto, inventario rigenerato tramite tool previsto, riesecuzione PASS. Non sono stati rigenerati gli hash protetti per far passare file modificati.
- `python3 tests/appearance8_guest.py /var/tmp/tmc-block8-candidate/tmc-block8-candidate.iso`: VM retail, tastiera/mouse reali. Testo `iiiWWW`, clic alla coordinata calcolata col font proporzionale, inserimento verificato come `iiiaWWW`; query lunga/Home/End/frecce/backspace; legenda Aiuto e otto azioni allineate verticalmente; clic sul contenuto inerte; Fine chiude.
- Retirement USB/legacy PASS; 473 file della baseline M8 USB/input tutti byte-identici. Nessun driver storage aggiunto finora.

ISO candidato: `/var/tmp/tmc-block8-candidate/tmc-block8-candidate.iso`, SHA-256 `29cba13f6a95be2f7862a5d6d9817b3dc1f6f79c2422cac1f1b9c07b61a1a928`.

## Misura grafica prima/dopo

Stesso scenario VM TCG, 2 vCPU, 768 MiB, 1280×800, cinque app aperte; eventi QMP reali. Misura host input→ultimo cambiamento osservato in una regione dello sfondo libera, poi cinque catture stabili. Screenshot via QMP: **non è una misura FPS né una certificazione hardware**. Tre azioni in sequenza per tipo, non una campagna statistica. Il primo risultato non giustifica dichiarare risolto ogni caso di lag.

| Operazione | Mediana prima | Mediana candidata | Intervallo prima | Intervallo candidata |
|---|---:|---:|---:|---:|
| Chiaro/scuro | 0,638 s | 0,574 s | 0,628–0,778 s | 0,343–0,631 s |
| Cambio sfondo | 0,655 s | 0,574 s | 0,644–0,707 s | 0,566–0,579 s |

Dati integrali: `appearance-before.json`, `appearance-after.json`. Script riproducibile: `tinymacOS/tests/appearance8_benchmark.py ISO OUTPUT_JSON` con output esterno al workspace. Fonte prima: ISO del blocco 7 SHA `ade556e160ddd2b0e7bd387e2ef9e4ff5a3f3705f6fa65e2dce4e217cbd0e988`. Restano possibili costi di glass/cache durante la dissolvenza e il primo caricamento QOI; non sono stati eliminati o certificati da queste sei misure.

## BaseSystem è già un DMG reale: verifica indipendente

Estratto `/Recovery/BaseSystem.dmg` dall’ISO candidato con xorriso, non generato un file arbitrario per il test. `tests/baseimage8_verify.py` controlla footer `koly` v4, bounds fork/XML, plist/blkx `mish`, continuità dei 32 extent raw, settori e CRC32. **qemu-img riconosce automaticamente `dmg` e lo decodifica interamente**: risultato di 8.388.608 byte identico al data fork. `mdir` sul RAW decodificato legge FAT16 e i file RECOVERY.ELF/RECOVERY.VOL.

DMG: 8.391.958 byte, SHA-256 `460a6654c1c1392b80636af5fe7f1d26196bfc70a939ade466082eb0078d975e`. Generatore `tools/baseimage.py` preesistente e non modificato. Questa è verifica del formato già presente, **non implementazione di hdiutil**. Nessun supporto universale per DMG compressi/cifrati/split dichiarato.

## Audit storage: lavoro ancora aperto

- `compat/linux/usbcore_bridge.c` rifiuta bulk in enqueue e add_endpoint; `compat/linux/{xhci,ehci}_runtime.c` espongono solo controllo/interrupt. Serve estensione bulk tramite controller originali, senza cambiare flussi interrupt/HID. Driver/class mass-storage e test BOT/SCSI, disconnessione/reset/stall/flush/capacità/settori logici restano da scrivere. Nessun percorso alternativo input.
- `storage/blockdev.c` espone soltanto AHCI+NVMe, ABI otto dischi, spinlock globale. USB scoperto dopo fs_init: attenzione al worker canonico serializzato CPU0, al rescan e alle attese/lock durante il block I/O. Non eseguire polling USB ricorsivo sotto lock che ne impediscano il completamento.
- `fs/volumes.c`: namespace /Volumes, discovery lazy, TMCFS2 via mount nativo lazy, FAT32/exFAT/NTFS tramite ForeignIo; generazione globale codificata nei descrittori. Non esiste stato di unmount persistente. `vol_refresh()` invalida tutte le generazioni e chiude backend, quindi non sostituirlo a unmount per volume senza policy FD aperti/flush. Proteggere volume attivo e ruoli di sistema.
- Non sono ancora implementati i menu contestuali di mount/unmount in Finder/Utility Disco, i comandi mount/unmount nativi o hdiutil RAW/IMG/ISO/DMG. Il vecchio `mount_apfs` è soltanto un messaggio sul volume di sistema già montato, non va spacciato per la nuova funzionalità.
- Release GitHub 10.3 (ISO/checksum), commit/push, backup finale e video M8-only su Drive **non eseguiti**: seguono solo l’accettazione completa, inclusa Recovery e regressioni dei filesystem.

## Spazio e provenienza

Recuperati 2.764.299 byte: gzip lossless di HTML/log/manifest storici, quattro vecchi screenshot in WebP con RGBA decodificato identico. Receipt completo in `workspace-compression.json`, originali confrontati con i blob di main prima della rimozione. Le vecchie SHA PNG restano registrate come originali; il manifest screenshot indica ora percorsi/SHA WebP e il tipo di trasformazione. Per leggere un vecchio file compresso: `gzip -dc percorso.gz`; per ricostruirlo, scrivere il risultato in `/var/tmp`, non nel workspace quasi pieno.

Backup Git completo da 61.934.891 byte preservato. Workspace al checkpoint circa 127,7 MB decimali / 121,8 MiB: sotto entrambe le interpretazioni di 128 MB, ma il margine resta ridotto. Nessun ISO/disco/video aggiunto al workspace. Evidenze nuove soltanto ritagli PNG lossless, con sorgente/full-frame SHA nel manifest. Input/font/upload originali non eliminati.
