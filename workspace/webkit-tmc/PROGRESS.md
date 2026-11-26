# Safari / WebKit — stato verificato del port

## Obiettivo 10.3: clock, deadline e coda eventi

Sviluppo su `main`. **38/38 casi PASS**: 18 gate host, cinque build diagnostiche da base OS fresca, quindici guest events/heap/C++/lifecycle/pthread su 1/4/8 CPU. Aggiunti clock MONOTONIC e sleep a risoluzione dichiarata 100Hz, condition pthread con clock/deadline assoluta, coda concorrente FIFO/task/timer/cancellazione/stop. Quattro producer, 400 gare cancel/dispatch e 400 task C++ con RAII; pulizia esattamente una volta e zero heap/DSO residui. Rieseguito integralmente il precedente stress C++ originale non ancorato con cache/trim, senza cambiare round o deadline. Dettagli: [RUNTIME-EVENTS.md](RUNTIME-EVENTS.md).

Audit di tre sorgenti upstream verificati byte per byte al pin, con sei riscontri: il percorso POSIX di WTF usa anche WallTime, quindi la nuova condition MONOTONIC **non è un drop-in**. Realtime resta ENOTSUP; la coda nativa non è WTF::RunLoop già compilato/integrato.

Non sono ancora consegnati il vero WTF/JSC/WebCore o Safari funzionante, né un runtime POSIX/C++ completo. Nessuna ISO intermedia pubblicata; binari 10.2.5 invariati. Ricevute e archivio precedente conservati su GitHub, con offload locale verificato per il budget 128MB.

## Ripresa precedente: base 10.2.5 / 30B16

La release 10.2.5 è pubblicata e verificata; non viene ripubblicata da questo lavoro. Lavoro nato su `work/safari-webkit-runtime`, ora consolidato in `main`: allocatore nativo VM con mutex, cinque simboli C di allocazione opt-in e prove condivise host/guest. Dettagli e risultati correnti in `RUNTIME-HEAP.md`. Il vecchio overlay viene aggiornato **dai** sorgenti OS autorevoli tramite `--capture`, non applicato al contrario. Nessun sorgente OS di produzione modificato. WTF/JSC/WebCore e interfaccia Safari non sono ancora compilati.

**Tutte le sezioni 2.4.x seguenti sono storico**, non la versione corrente o una promessa di versione futura. Il requisito attuale resta UI identica e WebKit completo, senza renderer sostitutivo.

## Criterio di consegna

Applicazione **Safari**, UI desktop classica, integrazione con tutti i temi di tinymacOS. Progetto indipendente da Apple.

Le manutenzioni fino alla **2.4.4 Levante / OC2**, esplicitamente autorizzate, sono state distribuite senza browser. OC2 aggiunge l’installer/disco BIOS attraverso OpenDuet/OpenCore e mantiene UEFI. Prossima release browser solo con vero HTTPS/HTML/CSS/JavaScript, temi e sicurezza collaudati.

## Release intermedia 2.4.4 autorizzata dopo il backup

L'utente ha esplicitamente chiesto di pubblicare la ISO corrente prima di proseguire con Safari. Versione **2.4.4 Levante, build 21B5**, senza browser, con ISO UEFI. Regressioni fresche: identità disegnata/plist/Terminale/Recovery, installer e verbose off/on/off, mouse/tablet/Recupero, suite backend/runtime 15/15 e stress SMP 4/8 vCPU PASS. Report `../release-2.4.4/RELEASE.md`. Questa autorizzazione supera il precedente divieto di ulteriori ISO intermedie per questa sola release. Prossima patch browser concordata nel piano: **2.4.5 Levante**, subordinata a motore reale e temi verificati.

## Stato corrente: SMP positivo e backup GitHub

- VM/MMU reale su 1 e 2 vCPU: operazioni positive e sette fault intenzionali per profilo; suite backend/runtime 15/15 PASS.
- Stress concorrente: quattro worker, 400 round e 1.600 syscall con buffer in gara con mutazioni VM; template del framebuffer verificati, PASS con 4 e 8 vCPU. Risultati in `../integrated-next/evidence/smp-stress-{4,8}/result.json`. Non è una libc pthread o un test di WebKit.
- Verbose nativo UEFI con V e comando Terminale, bootstrap BIOS/OpenDuet/OpenCore con BOOTCFG, verificati separatamente. Test installato off/on/off con riavvii e controllo byte ESP/Recovery PASS, in `../integrated-next/evidence/verbose-installed/`.
- Backup privato effettivamente caricato: https://github.com/ESPNex/tinymacOS ; database Git e credenziali fuori dal workspace. Sorgenti, evidenze e strumenti conservati estratti.
- Nessuna nuova ISO pubblicata. WTF/JSC/WebCore/Safari non compilati. Tutti i gate browser rimangono bloccanti.

## Storico delle milestone precedenti

Le sezioni seguenti registrano lo stato al momento dei rispettivi test. In particolare **il vecchio rifiuto SMP non descrive più il kernel di sviluppo corrente**; resta corretto per le vecchie release distribuite.

### Dopo il deploy 2.4.3: primo blocco SMP (sviluppo)

La manutenzione è congelata in `../maintenance-2.4.3/sources/` e su Drive. L'albero OS di sviluppo ora differisce: mailbox TLB release/acquire e servizio dei pending shootdown negli spinlock IRQ-off, senza lock nell'handler. Compilazione target PASS; 8 worker host, 10.001 richieste e 80.008 ACK con ASan/UBSan PASS. Suite ricostruita e guest rete/VM 1/2 vCPU **15/15 PASS**.

**S_VM rimane deliberatamente single-CPU:** questi test non completano il fix SMP. Restano lifetime prima del page_put, protezioni/mapping e concorrenza uaccess/thread da validare. Dettagli: `../smp-next/STATUS.md`. Nessun WebCore/JSC/Safari compilato e nessun upload di queste build diagnostiche.

## Ultimo blocco: VM reale per il runtime (15 verifiche PASS)

Aggiunta **S_VM**, accodata all'ABI senza rinumerare le syscall esistenti, con backend `posix/src/vm.c` e header `tmc_vm.h`/`vmabi.h`. Sorgenti kernel in `kernel/uservm.c` e aggiornamenti `kernel/memory.c`, `kernel/k.h`, `kernel/syscall.c`, `include/abi.h`, tutti salvati e sincronizzati nell'overlay prima della build.

Implementati:

- riserva di spazio virtuale senza backing fisico, allineamento fino a 2 MiB e guard page su entrambi i lati;
- commit di sottorange interamente non committed, pagine inizialmente azzerate e rollback dei nuovi mapping se manca memoria;
- protezioni reali READ, READ|WRITE e NONE: NONE conserva i dati ma toglie l'accesso user; non è simulato;
- decommit idempotente, restituzione del backing e recommit azzerato;
- release esatta della riserva; controllo integrale del range prima dei cambiamenti di protezione;
- mapping sempre NX, richieste EXEC/JIT rifiutate;
- query esplicita delle capacità. Nessuna falsa dichiarazione di conformità mmap/mprotect POSIX o di integrazione già completata con WTF.

**Limite intenzionale:** modifiche VM solo con **1 CPU online**. Con 2 CPU la query segnala indisponibilità e le operazioni falliscono ENOTSUP. L'attuale invalidazione remota del kernel non è stata validata per il lifetime di queste nuove pagine; non la uso fingendo sicurezza SMP. È una base per la prima configurazione interprete, non una soluzione finale per il browser multithread.

Altri limiti: arena virtuale **240 MiB**, 32 riserve, massimo **64 MiB** per riserva; non sono supportati mapping file, fixed mapping, alias condivisi, grandi riserve Gigacage/JIT o tutte le richieste possibili di JSC. Quelle incompatibili vanno gestite nel vero port WTF/JSC, non nascoste da successi fittizi.

### Test nuovi

- Host ASan/UBSan: contratti ABI/errno, prevalidazione, guard/range, fallimento commit con rollback, **50.000 operazioni** di riserva/frammentazione/allineamento confrontate con invarianti di non sovrapposizione. Page table e lock simulati su host: non prova hardware.
- Guest **1 vCPU**, MMU reale: riserva/commit, zero iniziale, contenuti conservati attraverso READ/NONE/RW, decommit/recommit zero, rifiuto EXEC, slot esauriti/liberati, backing e allineamento a **2 MiB**.
- **Sette processi deliberatamente in fault**: lettura guard sinistra e destra, scrittura read-only, lettura NONE, lettura dopo decommit/release, esecuzione NX. Ogni processo emette prima un marker ARMED; la shell legge poi `$?=1`; nessun marker di sopravvivenza. Sono test della protezione attesa e della terminazione del processo, non un audit generale della sandbox.
- Desktop e Terminale rimangono operativi. Un comando nuovo dopo tutti i fault ripete positivamente le operazioni VM e stampa un marker univoco `VM POSTFAULT PASS`.
- Guest **2 vCPU**: rifiuto esplicito delle modifiche VM verificato, non spacciato per supporto VM SMP.
- Rete/file: rieseguita la regressione con il **nuovo kernel**, su 1 e 2 vCPU; TCP 1.048.893 byte e font 2.215.962 byte con SHA256 ancora PASS.

Suite completa: **15/15** verifiche. Report `test-results/milestone-tests.json`, `vm-host.json`, `guest-vm-smp{1,2}/result.json`; screenshot positivi e per ogni fault nelle stesse cartelle. Hash sorgenti OS prima della build: `test-results/vm-system-source-hashes.json`; hash probe/build `guest-vm-{source-hashes,build}.json`.

Le ISO appena costruite sono **esclusivamente QA locali esterne a /home/user**, non release e non caricate su Drive. La 2.4.1 distribuita in precedenza non contiene S_VM.

## Blocco precedente: rete guest e descriptor

### 1. Test del percorso reale di rete

Ora eseguito il percorso completo:

`backend target → syscall native → kernel/lwIP → e1000 → QEMU slirp → peer TCP locale controllato`

Non è un mock lwIP. Due avvii separati, **1 e 2 vCPU**, contati anche tramite QMP. Un solo programma di test per volta: questo non dimostra correttezza generale multithread/SMP, isolamento o carico concorrente.

Entrambi i profili passano:

- **1.048.893 byte** con pattern verificato byte per byte, scritture frammentate lato server e pausa iniziale del consumatore; trasferimento oltre 128 volte il ring nativo da 8 KiB. È stress di backpressure end-to-end, non misura strumentata del picco di occupazione del ring.
- SHA-256 `37c257a3a8dac6f78f1961e5e22198a45cab413463981513df57c772adb69503`, uguale sul guest e sul server.
- FIN, EOF ripetibile, stato half-close ancora scrivibile; digest inviato **dopo il FIN del peer** e verificato dal server.
- timeout lettura **120 ms**, nessuna falsa readiness, EAGAIN.
- chiusura abortiva del server: ECONNRESET e stato errore, non EOF pulito.
- handle invalido e poll invalido.

Il successo viene letto dal **framebuffer del Terminale**, con screenshot e riconoscimento dei glifi nativi; non è basato su COM1 diagnostico. Peer solo locale: nessuna dipendenza da siti pubblici, DNS esterno o TLS.

### 2. Tabella descriptor comune per file e TCP

Nuovi `posix/include/tmc_descriptors.h` e `posix/src/descriptors.c`:

- namespace comune 3…63, separato dagli handle nativi file/socket;
- open-description con riferimenti: `dup` condivide offset e lifetime;
- il solo ultimo `close` chiude la risorsa nativa;
- impossibile adottare due volte la stessa coppia tipo/handle;
- errori di tabella piena prima di aprire risorse; ownership trasferita soltanto su adozione riuscita;
- read file limitata a 8 KiB per syscall, TCP con letture/scritture parziali;
- seek assoluto per file, ESPIPE per socket;
- poll misto file/TCP con timeout, errori/hangup/invalidi indipendenti dagli eventi richiesti;
- chiusura consuma il descriptor anche se il close nativo fallisce: non va ritentata sul numero ormai riutilizzabile.

**Non è ancora una libc POSIX**, né una barriera di sicurezza: chiamante serializzato, niente fork/exec/cancellazione/stdio, niente errno per-thread; handle TCP kernel ancora globali.

I file sono **soltanto in lettura**. Nell'audit ho trovato che `fs_write_i` imposta sempre la dimensione al termine della scrittura, troncando la coda anche quando POSIX write la conserverebbe. Non ho mascherato tale differenza e non ho cambiato la semantica legacy usata dalle app: file scrivibili/ftruncate/append restano un blocco aperto.

### 3. Test file sul guest

Nei due stessi profili:

- aperto il vero font `/System/Library/Fonts/Interface.tfn`;
- dup, offset condiviso, seek, chiusura del primo alias e lettura dal secondo;
- read richiesta oltre 8 KiB correttamente restituita come short read;
- rifiuto di write sul descriptor read-only;
- **2.215.962 byte** letti fino a EOF e SHA-256 verificato contro l'asset sorgente:
  `ca99fceb218091816dfbc0f2dfaef7fef846599806d620c3f2c6302c1d1d0b8c`.

Test descriptor host con ASan/UBSan: percorsi di errore, saturazione tabella, alias, close falliti e **100.000 operazioni di lifetime** confrontate con un modello indipendente dei riferimenti.

## Verifiche precedenti conservate

- Backpressure callback TCP: chain accettata interamente o ERR_MEM senza perdita/credito prematuro; credito al consumo, FIN/reset e ownership.
- S_NET/8 e S_NET/9, vecchia op6 preservata.
- Stress host sui corpi kernel reali con mock lwIP/lock: **35.539.167 byte**, wrap e contatori.
- Adattatore aspetto S_INFO/palette condivisa: **56 combinazioni**, non ancora collegato a WebCore o a Safari.
- Backend polling nativo, clock 100 Hz, entropia non disponibile che fallisce chiusa invece di inventare casualità.
- Dieci file overlay e quattro header ABI sincronizzati con `/home/user/tinymacOS/`.

## Suite ripetibile

```sh
# 11 controlli host/oggetti target, senza avvio VM
python3 /home/user/workspace/webkit-tmc/tools/run-tests.py

# 29 controlli: stessi 11 + cinque build QA + tredici profili guest
# Prerequisiti da sorgenti: toolchain local-tools.py, QEMU/OVMF, numpy e Pillow.
chmod u+x /home/user/tinymacOS/tools/run-qemu.sh
python3 /home/user/workspace/webkit-tmc/tools/run-tests.py --guest
```

L'immagine diagnostica viene creata **fuori /home/user**, in `/var/tmp/tmc-webkit-network-guest/`. Tutti gli alias del programma command in quella sola immagine eseguono la probe: **non è un sistema da distribuire e non deve essere caricata su Drive**. Il sorgente OS di produzione `userspace/command.c` non viene modificato. Se mancano gli artefatti baseline, lo script li ricostruisce dai sorgenti OS; la toolchain si ripristina con `tools/local-tools.py`, non serve conservare un unico binario in /var/tmp.

I sorgenti sono salvati prima della compilazione; manifest delle sorgenti e hash degli artefatti in `test-results/guest-network-{source-hashes,build}.json`. Risultati completi: `test-results/milestone-tests.json`, `guest-network-smp1/result.json`, `guest-network-smp2/result.json`; screenshot nelle due cartelle SMP.

## Cosa manca prima dell'ISO browser

1. libc/C++ target, startup/thread trampoline automatici, pthread/attese, ELF/C++ TLS/DTV e allocator per le reali dimensioni/mapping WTF/JSC. FS per-task e errno target sono ora per-thread con attachment esplicito; VM SMP è già verificata, non equivale a runtime completo.
2. Semantica dei file scrivibili/cache, integrazione descriptor nelle vere API libc e ownership kernel dei socket.
3. API sicura di casualità generale e backend TLS mantenuto: trust store, date, hostname/catena, SNI e test negativi.
4. Build e test target di WTF/JavaScriptCore, poi WebCore/rendering, font/shaping ed event loop.
5. UI Safari con schede, navigazione, download e temi collegati al motore.
6. Isolamento contenuti e collaudo rappresentativo HTML/CSS/JS/HTTPS; solo dopo build/test/upload della release browser.

È ancora presente solo il piccolo sottoinsieme upstream di riferimento, **non un checkout completo di WebKit**. Non sono stati compilati JavaScriptCore o WebCore. Nessuna compatibilità universale con i siti è garantita.

## Ripresa immediata dopo OC2 — errno target collegato al TLS

- OC2 scaricata da GitHub e verificata: `installer-media/github-verification.json` nella directory release 2.4.4. Contiene kernel S_TLS; non contiene le modifiche successive agli adattatori errno.
- `posix/libc-headers/errno.h` ora usa `__errno_location()`; `posix/src/errno.c` restituisce la cella del TCB nativo corrente. Nessun errno globale, nessun fallback condiviso. L’uso senza TCB valido termina il processo tramite trap: è un errore di startup, non una falsa riuscita.
- Inizializzazione ancora esplicita: ogni thread che chiama un adattatore errno deve mantenere un TCB caller-owned mappato. Non dichiarare pthread/ELF TLS o libc completa.
- Guest TLS su **1/4/8 vCPU PASS**, 4 worker × 150 iterazioni per profilo: `&errno` uguale alla cella FS, errori reali EFAULT/EBADF negli adattatori VM/descriptor, valori distinti preservati dopo yield. Le tabelle descriptor condivise non diventano per questo thread-safe.
- Regressione completa fresca **15/15 PASS**: contratti host, compilazione target, build guest rete/VM, rete/read-only-file e VM reali su 1/2 vCPU. Log e hash sotto `test-results/`.
- Mancano ancora startup/libc/C++/pthread/attese, veri WTF/JSC/WebCore, rendering/font, HTTPS validato e interfaccia Safari collegata a tutti i temi. La futura release browser resta 2.4.5 Levante.

## Passo successivo eseguito — primitive di sincronizzazione per WTF

- S_WORDWAIT privato: compare-and-park, wake, mismatch e deadline assoluta a 100 Hz; implementazione nel kernel/scheduler reale, non polling vuoto.
- Mutex con atomiche acquire/release, trylock/timedlock e condition con broadcast; il timeout della condition ritorna con mutex riacquisito.
- Host ASan/UBSan: 8.000 aggiornamenti serializzati su quattro worker PASS. Native guest 1/4/8 vCPU: 240 sezioni critiche con yield interno, timeout e predicate loop PASS; la copertura richiede più di una CPU nei profili SMP.
- Ripetuta la suite estesa completa: **24/24 PASS**, compresi rete/VM su 1/2 CPU e nuovi test sync/FS-errno su 1/4/8 CPU. Si conserva l’indagine sul precedente fallimento della sola copertura CPU prima della correzione del workload: `test-results/sync-investigation.json`.
- Aggiunti i veri header upstream Threading/ThreadSpecific/StackBounds al piccolo sottoinsieme di riferimento, con hash e copyright. **Non significa che WTF sia stato compilato.** Confronto API e prossimi blocchi concreti in `THREADING.md`.
- Nessun nuovo deploy o upload ISO. Le immagini diagnostiche sono esterne e non distribuibili; OC2 resta immutata. Prossimo confine: startup/lifetime/join e TSD con cleanup, runtime C/C++ e build WTF/JSC reale.

## Macroblocco successivo completato — thread lifecycle, TSD e ponte C++

- Startup TLS automatica per i figli, identità process-local non riusate, kernel final handoff con FS azzerato e completion release; join esclusivo/timedjoin e risultati a larghezza piena/NULL. Il thread non tocca più il proprio TCB dopo la pubblicazione del completamento.
- TSD con generazioni, gestione dell’esaurimento chiavi, distruttori reentranti fuori lock e massimo quattro passaggi; ritorno ed exit esplicita attraversano lo stesso cleanup.
- Suite completa fresca **29/29 PASS**. I profili nativi 1/4/8 CPU eseguono 90 creazioni di base più figli fino all’esaurimento reale dei Task, 320 aggiornamenti concorrenti, 64 riusi e 16 TCB VM smappati subito dopo il join.
- Un consumatore C++20 freestanding RAII/C-ABI è compilato, linkato senza librerie C++ e senza simboli indefiniti, ed eseguito nel guest. Non è libc++/libstdc++, eccezioni o motore WebKit.
- Fonti, contratti, limiti e prossimo macroblocco allocator/libc/runtime C++ + WTF/JSC in `RUNTIME-THREADS.md`. Nessun nuovo deploy/ISO upload; OC2 invariata.


## 10.3 — macroblocco lifecycle C++

Startup `.init_array` esplicito e thread-safe, ABI `__cxa_atexit/__cxa_finalize`, registro dinamico e distruzione DSO LIFO con rientranza. **14 gate host + build fresca + profili lifecycle nativi 1/4/8 CPU PASS** (18 casi nella stessa matrice). Due globali e uno static locale compiler-generated, 400 registrazioni concorrenti e 401 callback worker, zero registro/heap residuo. Dettagli, limiti e riproduzione: `RUNTIME-LIFECYCLE.md`. Non è CRT automatico OS-wide, libc++, WTF/JSC/WebCore o Safari. Stress heap non ancorato ancora aperto; nessuna nuova ISO distribuita. Recupero release storiche annullato su richiesta, senza pubblicazioni.
