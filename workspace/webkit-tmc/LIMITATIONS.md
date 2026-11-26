# Blocchi tecnici del port

| Area | Riscontro attuale | Lavoro richiesto |
|---|---|---|
| DNS | S_NET/3 e net_resolve presenti, IPv4; connect risolve nomi | getaddrinfo e gestione errori/lifetime, test guest, IPv6 separatamente |
| TCP | S_NET/4–9, polling, descriptor backend comuni file/TCP; test guest locale 1/2 vCPU PASS | API descriptor libc POSIX, ownership kernel per processo, attese interrompibili e carico concorrente |
| Integrità TCP | stress host 35.539.167 byte; guest lwIP/e1000 reale 1.048.893 byte + SHA256 con pausa consumatore e half-close PASS | carico concorrente, condizioni di rete diverse, DNS reale e rete non-QEMU |
| EOF/errori | FIN/half-close, reset non-EOF, timeout/EAGAIN e handle invalidi ora testati anche sul guest | altri errori e concorrenza; la manutenzione 2.4.1 include le ABI, la 2.4.0 no |
| Threading | S_THREAD + final handoff kernel, startup TLS dei figli, identità, join/timedjoin, TSD con generazioni/distruttori, mutex/condition; guest 1/4/8 CPU PASS | Bootstrap iniziale generale, detach, metadata stack/sospensione, wrapper pthread/WTF e gestione allocazioni; nessuno stub di successo |
| Memoria virtuale | nuova S_VM: riserva/commit/protezioni/NONE/guard/decommit/release, test MMU guest 1/2 CPU e 7 fault PASS, stress concorrente 4/8 CPU | arena 240 MiB, 32 riserve, max64 MiB; niente EXEC/JIT, mapping file o fixed; adattamento WTF/JSC non ancora eseguito |
| Allocazione | Heap VM prefissato e cinque simboli C opt-in; contratti host normali + ASan/UBSan PASS; risultati nativi in `RUNTIME-HEAP.md` | 16 byte fondamentali, allineamento fino 2 MiB, regioni finite S_VM; non hardened, bmalloc, GC, JIT, allocator ABI C++ o libc completa; nessun uso in produzione ancora |
| C++ | consumatore C++20 freestanding RAII/C-ABI eseguito nel guest; nessuna standard library target/WebKit validata | C++ standard library, atomiche, ABI/runtime e toolchain compatibili col commit |
| Clock | tick kernel a 100 Hz e data/ora via SysInfo | realtime UTC validato per certificati, clock_gettime, timer/event loop |
| Entropia | sorgenti di entropia kernel usate per credenziali; nessuna syscall generale per byte casuali sicuri | CSPRNG kernel con API utenti e gestione fallimenti; niente PRNG basati sul tempo |
| TLS | nessun backend TLS utenti integrato | libreria mantenuta, SNI, trust store, hostname/date/chain validation, test negativi |
| File | descriptor read-only+TCP, dup/offset condiviso; font reale 2.215.962 byte e SHA256 sul guest | native write tronca la coda al punto di scrittura: NON POSIX; servono write/truncate distinti, libc/stat/cache e ulteriori file grandi |
| Grafica/testo | raster nativo e adattatore aspetto con palette condivisa del sistema, 56 combinazioni testate su host | backend WebCore, UI Safari, font/shaping, compositing, input/IME e test visivi guest |
| Processi/IPC | processi nativi e syscall specifiche | modello processi WebKit, IPC, shared memory e sandbox |

## Limiti del codice iniziale

Le funzioni di rete base ricevono handle nativi; la nuova tabella descriptor li distingue dagli handle file. Entrambe richiedono serializzazione del chiamante. Non dichiarano conformità POSIX e non sono ancora collegate al motore. La nuova lettura distingue gli errori; connect/send e DNS espongono ancora errori poco dettagliati. Non esiste ancora ownership degli handle per processo, quindi non è una rete isolata per contenuti non fidati. La compilazione freestanding degli oggetti non dimostra il link di una libc. La nuova probe QA è invece linkata/eseguita sul guest, con errno per-thread ma chiamante descriptor serializzato, non con un runtime completo.

`tmc_secure_random` rifiuta ogni richiesta non vuota con ENOSYS: è un blocco intenzionale. Un backend TLS non deve proseguire se questa funzione fallisce. Non sono stati implementati fallback insicuri.

## Riferimenti upstream esaminati

Il commit e la data dichiarata dal repository sono in upstream.lock.json. Il sottoinsieme locale conserva i copyright upstream. OptionsJSCOnly richiede Threads; OSAllocatorPOSIX usa mmap/mprotect/madvise e gestione delle guard page. Un insieme di macro che rinomini TinymacOS in Linux non soddisfa tali contratti.

Riferimento metodologico: https://trac.webkit.org/wiki/SuccessfulPortHowTo (documento storico: partire da JavaScriptCore e dai suoi test, poi WebCore e test di rendering).

## ABI VM di sviluppo

S_VM è presente soltanto nei sorgenti correnti e nelle immagini QA locali, non nella ISO manutenzione 2.4.1 pubblicata. Query e codice errore rendono esplicita l’assenza di supporto su più CPU. Non rinominare questo backend in mmap POSIX: commit su range già committed ritorna EBUSY, protect richiede tutto il range committed, release solo esatta, nessun mapping file/eseguibile. La capacità effettiva di compilare ed eseguire JSC resta da dimostrare.
