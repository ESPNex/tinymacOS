# 10.3 — megablocco clock, deadline e coda eventi

## Ambito

Nuovo substrato nativo per il lavoro sul runtime WebKit: clock monotono, attese temporizzate con timespec, condition pthread con clock esplicito e una coda concorrente di task/timer con gestione del lifetime. **Non è WTF::RunLoop già portato, né JavaScriptCore/WebCore/Safari funzionante.** Non sono modificati i programmi o i binari della 10.2.5 distribuita; nessuna ISO diagnostica pubblicata.

**Risultato finale: 38/38 casi PASS nella stessa esecuzione**, inclusi tutti i quindici profili nativi.

La matrice `--events-megablock` riunisce 18 gate host, cinque build diagnostiche da base OS fresca e quindici profili guest: events, heap, C++ originale non ancorato, lifecycle e pthread, ciascuno su 1/4/8 CPU. L'esito e gli hash effettivi sono in `test-results/events/verification.json`.

## Clock e sleep

`tmc_time.h` / `posix/src/time.c`, con alias sorgente separati e opt-in in `posix/pthread-headers/time.h`:

- `clock_gettime/getres(CLOCK_MONOTONIC)`: secondi/nanosecondi dal boot, risoluzione reale dichiarata **10 ms**, non un clock ad alta risoluzione mascherato. I tick provengono da S_TICKS.
- `CLOCK_REALTIME` restituisce **ENOTSUP**. Nessuna conversione arbitraria della data visualizzata in UTC fidato: HTTPS e validazione delle date dei certificati restano bloccati.
- Conversione dei timespec con verifica di puntatori, secondi negativi, nanosecondi fuori intervallo e overflow; deadline arrotondate verso l'alto. La deadline nativa zero significa infinito, perciò l'epoca zero viene rappresentata dal primo tick già scaduto sul sistema avviato.
- `clock_nanosleep` relativo/assoluto monotono restituisce un numero d'errore e conserva errno. `nanosleep` espone invece il contratto -1/errno.
- Per una durata relativa non nulla viene aggiunto conservativamente un tick alla durata arrotondata: senza un contatore sub-tick, attendere soltanto il prossimo tick potrebbe dormire meno del richiesto. La durata zero non dorme.
- Sleep lunghi suddivisi nel limite reale di S_SLEEP (**360000 tick / un'ora**), con rilettura del clock dopo ciascun risveglio e nessun busy-wait. Deadline già scadute ritornano subito. Nessuna gestione di segnali/cancellazione o contratto di resto EINTR dichiarato.

Test host deterministici con trasporto simulato, sia normali sia ASan/UBSan: precisione della conversione/ceil, overflow, output non modificati su errore, sleep zero/scaduti, tre chunk per una deadline di 7201 secondi senza attenderli realmente, risvegli anticipati e fallimenti syscall. I test nativi verificano il clock e sleep realmente eseguiti, non quei fault injection simulati.

## Condition pthread con clock esplicito

- `pthread_condattr_setclock/getclock` e `pthread_cond_timedwait` aggiunti all'adattatore sorgente prefissato. Non si intercettano le funzioni pthread/time della libc host.
- Default coerente con POSIX: **CLOCK_REALTIME**. Finché il realtime non è disponibile, un timedwait su questa condition restituisce ENOTSUP; l'attesa senza timeout continua a funzionare.
- Con attributo **CLOCK_MONOTONIC**, timedwait usa un timespec assoluto e il parcheggio nativo. Il mutex viene riacquisito anche su timeout e il chiamante deve sempre ricontrollare il predicato.
- Timespec non validi rifiutati prima di rilasciare il mutex. Restano ownership, ERRORCHECK/RECURSIVE, errno preservato e il rifiuto di attese con ricorsione maggiore di uno. L'estensione precedente a tick `tmc_pthread_cond_wait_until` resta disponibile.
- Prove condivise host/guest: timeout reale, mutex ancora posseduto dopo timeout/errore, deadline passata, notifica prima della scadenza, clock invalido e realtime rifiutato. L'intero precedente probe pthread viene rieseguito separatamente, senza rimuoverne join/TSD/once/capacity.

## Coda nativa di task e timer

`tmc_runloop.h` / `posix/src/runloop.c`:

- Oggetto stabile posseduto dal chiamante; **un solo dispatcher**, molteplici producer. Non crea thread nascosti. Il thread chiamante deve aver attaccato il TCB nativo.
- Task immediati e one-shot a deadline monotona assoluta. Stessa deadline: ordine FIFO di inserimento sotto lock. I timer non vengono chiamati prima della deadline; quelli scaduti vengono eseguiti appena possibile.
- Coda allocata dinamicamente dall'heap nativo. Nessun limite artificiale a un piccolo array di callback; i limiti di memoria dell'heap continuano a valere. Inserimento e cancellazione O(n), non una struttura hard realtime.
- Il dispatcher inattivo si **parcheggia** su condition; l'inserimento o la cancellazione lo risveglia per rivalutare la prima deadline. Un task immediato interrompe l'attesa per un timer lontano. Non è un loop di polling del clock.
- Token globali non riutilizzati, cancellazione di un task pending, EBUSY per quello attivo, ESRCH per quello consumato o sconosciuto. Nessuna falsa cancellazione di una callback già iniziata.
- Callback e cleanup fuori dai lock: possono pubblicare altri task, cancellare o fermare la coda. Devono ritornare normalmente, senza exit/throw/cancel.
- Dopo un post riuscito, cleanup opzionale eseguita **esattamente una volta**, dopo la callback oppure al suo posto in caso di cancellazione/stop pending. Può girare sul dispatcher, sul chiamante cancel o sul chiamante stop: il proprietario dell'argomento deve rispettare questo contratto.
- Se il post fallisce, token in uscita, coda e proprietà dell'argomento rimangono invariati; cleanup non viene chiamata.
- Stop idempotente: rifiuta nuovi post con ESHUTDOWN, consuma le cleanup pending, lascia completare callback/cleanup attiva e fa uscire run. **Stop non è join.** Prima di destroy occorre joinare il dispatcher e fermare tutti gli altri chiamanti API/cleanup. Destroy durante run o prima di stop è EBUSY; niente reset/copia di oggetti vivi.

Non è un multiplexer file/socket, non aggiunge timer ricorrenti, run mode annidate, integrazione GUI o il lifetime di WTF::TimerBase. Non viene rinominata in una classe upstream per simulare un port già fatto.

## Consumer C/C++ e regressioni

Stesso consumer `<pthread.h>/<time.h>` su host e guest, con trasporto host in un TU separato che usa gli header veri del sistema:

- **68 callback ordinate**, pubblicazione annidata da callback, tre timer con la stessa deadline e controllo di mancata esecuzione anticipata;
- callback-barriera per verificare dispatcher duplicato, destroy occupato e cancellazione della callback attiva;
- **quattro producer, 400 gare cancellazione/dispatch**, verifica di una sola cleanup per ogni post riuscito;
- **400 ulteriori task C++**: new/nothrow, delete, lock RAII, static locale non banale attraverso il runtime e distruttore compiler-generated registrato nel lifecycle; distruzione del contesto una volta per task;
- risveglio da timer lontano, stop durante callback, pulizia pending, rifiuto dei post tardivi, stop prima di run e destroy dopo join;
- zero oggetti heap, span, byte riservati e distruttori DSO pendenti alla fine;
- errore d'allocazione al post, output/ownership invariati e nessuna cleanup prematura: fault injection **host only**.

Le prove C++ normali non collegano direttamente libstdc++. Gli oggetti di test host rinominano soltanto i due simboli ABI lifecycle per non intercettare glibc/ASan; il target usa i veri simboli. I profili SMP richiedono esecuzione su più CPU. Deadline interna guest invariata a 145 secondi; timeout e deadlock non sono successi.

Le altre dodici prove native rieseguono integralmente i quattro probe del precedente megablocco. Lo stress C++ mantiene il worker originale senza buffer-ancora, 4×1000 round e 12000 distruttori, cache heap opt-in e trim finale. La nuova coda non sostituisce quei test con un carico più facile.

## Riscontro con il vero upstream

Aggiunti **senza modifiche**, con copyright/licenze conservati, `RunLoop.h` e `generic/RunLoopGeneric.cpp` dal commit già fissato `23a3d9001767ddc9a4b120b6d7db33ca21230ab8`. `events-reference-lock.json` registra gli hash; `tools/audit-events-upstream.py` verifica sei riscontri puntuali con file/righe, non una compilazione.

Punto importante: `ThreadCondition::timedWait` del percorso POSIX esaminato riceve **WallTime**, mentre il generic RunLoop organizza timer **MonotonicTime** e usa proprie condition, refcount, RedBlackTree, allocator TZone e classi WTF. Le nostre condition monotone **non sono quindi un rimpiazzo trasparente del percorso POSIX attuale**. Servono una scelta esplicita di backend/clock e la chiusura delle dipendenze C/C++/WTF; non basta aggiungere macro Linux o includere il nuovo header.

Il gate di audit passa se gli hash e i riscontri rimangono veri, **non** se WTF compila. I gate del motore, HTTPS e UI Safari restano false. Nessuna data del pin viene usata per dichiarare che sia la versione upstream più recente.

## Riproduzione e prove persistenti

```sh
sh workspace/webkit-tmc/tools/test-time.sh
sh workspace/webkit-tmc/tools/test-events.sh
python3 workspace/webkit-tmc/tools/audit-events-upstream.py
PYTHONDONTWRITEBYTECODE=1 \
PYTHONPATH=/var/tmp/tmc-python-release \
TOOLROOT=/var/tmp/tmc-tools \
python3 workspace/webkit-tmc/tools/run-tests.py --events-megablock \
  --output /var/tmp/tmc-events-final/results/acceptance.json
```

Build, toolchain e immagini rimangono fuori da `/home/user`. L'archivio corrente conserva la ricevuta completa dell'esecuzione, stdout/stderr dei gate, i quindici esiti guest, manifest build/source/media, sorgenti esatti e tre crop lossless dei nuovi risultati events. Frame completi e frame delle regressioni rimangono scratch esterno con percorso/hash registrati: non sono dichiarati persistenti.

L'archivio del precedente megablocco è stato confrontato byte per byte con il [commit GitHub immutabile 5c2febb](https://raw.githubusercontent.com/ESPNex/tinymacOS/5c2febbbd6aec5ebe86d358f92dfa2ee2b9cd3cb/workspace/webkit-tmc/test-results/megablock/megablock-evidence.tar.xz) prima di rimuoverne la copia locale per il budget 128 MB. Mappa completa in `test-results/REMOTE-OLDER-RUNTIME-EVIDENCE.json`.

## Ancora bloccante

Questo lavoro non risolve libc++ completa, UTC validato, CSPRNG/TLS, modello di thread/stack/sospensione richiesto dal motore, allocator WTF/JSC, grafica/font/input, IPC/sandbox o UI Safari/temi. Il passo verso il vero build deve usare la configurazione e le dipendenze effettive del motore, senza dichiarare questi probe un WebKit ridotto.
