# WebKit per TinymacOS — avvio del port

## Stato reale

**Aggiornamento sviluppo integrato:** SMP VM non è più limitato a una CPU. Stress nativo concorrente PASS con 4 e 8 vCPU (4 worker, 400 round, 1.600 chiamate buffer S_INFO), evidenze in `../integrated-next/evidence/smp-stress-{4,8}/`. Suite backend/runtime completa **24/24 PASS**, ora con attesa/risveglio atomici nativi, mutex/condition e FS/errno su 1/4/8 vCPU; non sono test JSC/WebCore. Le note di milestone precedenti conservate altrove possono descrivere il vecchio rifiuto SMP. Repository privata di backup: https://github.com/ESPNex/tinymacOS .

**Non è ancora un browser e non è ancora un port WebKit compilabile.**

Nome applicazione concordato: **Safari** (progetto indipendente). Riferimento: Safari desktop classico con tutti i temi del sistema. **La ISO 2.4.1 di manutenzione è stata autorizzata e distribuita prima del browser. Non include Safari/WebKit funzionante.** Il criterio browser resta HTTPS e test HTML/CSS/JavaScript reali. Criteri in `release-gates.json`; avanzamento in `PROGRESS.md`. La 2.4.1 include le nuove ABI kernel ma non il motore. La baseline 2.4.0 precedente non contiene quelle ABI.


È stato iniziato il backend nativo che servirà allo shim POSIX: DNS IPv4, apertura/lettura/scrittura/chiusura TCP, clock monotono e un punto d'ingresso per la casualità sicura che al momento fallisce esplicitamente con ENOSYS. Non è una libc POSIX completa. Le API sono volutamente prefissate `tmc_`: non sostituiscono ancora socket, getaddrinfo o pthread nel motore.

**Sincronizzazione nativa:** `posix/src/sync.c` usa S_WORDWAIT per mutex e condition reali, con timeout e riacquisizione del mutex. Non è pthread completo; startup/join/TSD restano da integrare. [Contratti, prove e confronto con le interfacce WTF reali](THREADING.md). Nessuna nuova ISO pubblicata in questo passo.

### File

- `posix/src/network.c`: DNS/TCP, nuove operazioni S_NET/8–9 con EOF distinto dagli errori e lettura dello stato. Richiede il kernel aggiornato nell’overlay.
- `posix/src/poll.c`: attesa su handle TCP con timeout.
- `posix/src/descriptors.c`: namespace comune file read-only/TCP, dup/offset condiviso, lifetime e poll misto. Contratto backend, non ancora API libc POSIX; chiamante serializzato.
- `posix/src/vm.c`: nuova VM nativa con riserva/commit/protezioni/guard/decommit, validata sul guest a 1/2 vCPU e stress concorrente a 4/8 vCPU; EXEC resta rifiutato, non è mmap POSIX.
- `kernel-overlay/`: diciannove file di integrazione sincronizzati con TinymacOS, inclusa la correzione della backpressure TCP.
- `platform/src/appearance.c`: lettura reale S_INFO, palette condivisa con il compositor, rilevamento cambiamenti. Non ancora collegato a un’interfaccia browser.
- `posix/src/native_syscall.c`: ponte syscall x86-64 nativo, non syscall Linux.
- `posix/src/clock_entropy.c`: clock 100 Hz; nessuna falsa entropia per TLS.
- `native-abi/`: copia degli header ABI correnti, conservata per rendere ripetibile la compilazione del backend.
- `tests/backend_contract.c`: test host con syscall simulata, non test di rete guest.
- `tools/test-backend.sh`: test con AddressSanitizer e UndefinedBehaviorSanitizer.
- `tools/compile-backend.sh`: compilazione freestanding dei soli oggetti; non esegue link di una libc, JSC o WebCore. Usa soltanto i nove header necessari al backend attuale, in `posix/libc-headers/`, con la licenza musl conservata; percorso configurabile con TMC_LIBC_HEADERS. L'implementazione libc è ancora mancante.
- `upstream.lock.json`: commit upstream fissato per l'analisi, non una versione già portata.
- `upstream-reference/`: piccolo sottoinsieme di riferimento upstream, inclusi i veri header threading WTF, scaricato in forma espansa con i copyright originali; NON un checkout completo di WebKit.
- `LIMITATIONS.md`: blocchi noti e tappe necessarie.

## Verifiche eseguite

```sh
python3 tools/run-tests.py
```

La suite base contiene **9 verifiche** host/compilazione oggetti. La suite completa di questo blocco si avvia con `python3 tools/run-tests.py --guest`: **15 verifiche** includendo due build diagnostiche, rete/file sul nuovo kernel e VM/MMU. Probe reale linkata ed eseguita sul guest: TCP 1.048.893 byte con SHA256, FIN/half-close, timeout/EAGAIN/reset; descriptor/dup/seek e font reale 2.215.962 byte con SHA256. VM reale su 1/2 vCPU, 7 fault intenzionali verificati per profilo, stress multi-CPU separato; limiti di arena/mapping documentati. Nessun test JSC/WebCore/HTTPS è ancora disponibile.

Le probe target ora inizializzano un TCB caller-owned e usano errno per-thread; questo non costituisce una libc/pthread completa né rende concorrenti gli oggetti descriptor condivisi. La build QA sostituisce gli alias command soltanto nel volume diagnostico esterno e **non va distribuita**. Gli oggetti standalone conservano dipendenze non risolte finché non vengono linkati con un runtime. Screenshot, hash e limiti in `PROGRESS.md`.

## Sequenza di sviluppo

1. Completare ABI e libc/POSIX verificabili: TLS di thread/errno, pthread e primitive di attesa, memoria virtuale con protezioni, file descriptor, poll, tempo reale, CSPRNG.
2. Portare WTF e JavaScriptCore usando JSCOnly come riferimento. Partire senza JIT e senza WebAssembly; questa è una scelta progettuale da validare sul commit scelto, non una configurazione già funzionante.
3. Eseguire test JavaScriptCore sul guest prima di dichiarare funzionante JavaScript.
4. Integrare un backend grafico CPU, font/shaping e librerie immagini/Unicode, poi WebCore ed event loop.
5. Rete HTTP/TLS con hostname, catena certificati, date, CA aggiornabili, redirect/cookie/cache. Non riutilizzare una fetcher HTTP senza validazione TLS come se fosse HTTPS.
6. Shell browser nativa con schede, cronologia, download, impostazioni e interfaccia ispirata a Safari.
7. Sandbox e isolamento dei contenuti, regressioni, test web e test hardware.

WebKit è il motore, non l'applicazione Safari. Il port non eredita automaticamente AppKit, servizi Apple, DRM, codec e altre integrazioni di Safari. L'identità funzionale assoluta e la compatibilità con tutti i siti non sono criteri garantibili.

## Pulizia del progetto

Su richiesta, eliminati NetSurf, librerie del vecchio port, registro storico, backup vendor e oggetti precompilati. Nessuna dipendenza dal vecchio albero. Sono stati conservati soltanto i nove header effettivamente usati dal backend, con la licenza musl, in `posix/libc-headers/`. Non contengono il motore NetSurf né la vecchia implementazione shim. `errno.h` è l’adattamento target TinymacOS: macro su `__errno_location()` e cella per-thread FS, senza fallback globale.

## Dopo il redeploy OpenDuet/OpenCore: TLS per-thread nativo

Revisione manutenzione pubblicata e riscaricata/verificata: [v2.4.4-oc2](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-oc2). Live ottica BIOS+UEFI, installazione grafica e disco installato autonomo BIOS/OpenDuet/OpenCore+UEFI, con Recupero offline/mouse/tablet verificati. I sorgenti completi corrispondenti Syslinux sono conservati estratti nel branch vendor separato della repository, non nel piccolo workspace.

**ABI kernel incluso in OC2:** aggiunta S_TLS accodata all'ABI, con query/get/set del thread pointer FS nativo. Ogni task ha il proprio FS base, inizializzato a zero per un nuovo thread e ripristinato nelle migrazioni/context switch. GS resta riservato al kernel e FSGSBASE disabilitato. Basi disallineate/non-user/non-canoniche vengono rifiutate senza cambiare il puntatore corrente. `posix/src/tls.c` fornisce un piccolo TCB caller-owned e una cella errno distinta per thread, senza fallback globale condiviso quando non inizializzata.

Probe reale guest **PASS a 1/4/8 vCPU**: quattro worker, **600 verifiche dopo yield** per profilo, TCB/celle errno distinti, valore del thread principale preservato, basi invalide rifiutate e reset FS verificato. Su SMP è richiesta esecuzione su più di una CPU (mask), non necessariamente migrazione di ogni singolo thread. Risultati `test-results/guest-tls-smp{1,4,8}.json`; ritagli del vero Terminale in `test-results/tls/`. Dopo le modifiche, suite backend/runtime completa **15/15 PASS** rieseguita.

**Non è ancora il TLS di rete HTTPS, una libc completa, ELF/C++ TLS (`__thread`/DTV), pthread o JavaScriptCore.** **Dopo il tag OC2**, gli adattatori target ora risolvono errno tramite `__errno_location()` nel TCB: nessun simbolo globale errno, fail-fast se la startup non ha installato un TCB. Startup/creazione thread sono ancora esplicite e caller-owned; non è pthread automatico. La suite reale 1/4/8 vCPU verifica 4 worker/600 controlli post-yield, anche dopo errori negli adattatori VM/descriptor e confronto tra `&errno` e cella TLS. Le tabelle descriptor condivise restano da sincronizzare: errno isolato non le rende thread-safe. Restano attese/futex e join, runtime C/C++, integrazione reale WTF/JSC, grafica/font e WebCore, HTTPS validato, UI e temi. La patch browser resta subordinata a questi gate.
