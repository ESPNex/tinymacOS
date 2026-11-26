# 10.3 / Safari — runtime C++ in sviluppo

## Aggiornamento: megablocco integrato

**Lo stress originale non ancorato è stato ripristinato e passa su 1/4/8 CPU**, con cache heap esplicitamente abilitata (256 KiB), stessi 4×1000 round/12000 distruttori e deadline 145 s; disabilitazione cache e trim prima delle asserzioni finali. La matrice integrata ha 31/31 casi PASS. Vedere [RUNTIME-MEGABLOCK.md](RUNTIME-MEGABLOCK.md) e `test-results/megablock/verification.json`. Non è una dichiarazione di miglioramento del percorso eager-release di default, né un PASS retroattivo del vecchio timeout. La sezione seguente descrive **la precedente milestone new/guard**, con i suoi risultati e limiti al momento delle prove.

## Ambito

Obiettivo della prossima versione: **10.3**, con UI Safari desktop classica identica al riferimento del progetto e motore WebKit completo. Il lavoro continua direttamente in `main`; non vengono ricreati rami di lavoro e non è pubblicata una ISO 10.3.

Questo blocco implementa un sottoinsieme di runtime C++ **freestanding, opt-in, senza eccezioni** sopra l'heap nativo esistente:

- `cpp/include/new`: dichiarazioni di allocazione; non è l'header standard completo.
- `cpp/src/new.cpp`: otto overload globali new/new[] e dodici delete/delete[], compresi sized, aligned e nothrow; placement inline nell'header. Allineamento fondamentale 16 byte, richieste allineate fino a 2 MiB nei limiti della VM.
- `std::set_new_handler/get_new_handler` con pubblicazione atomica. In caso di esaurimento reale, l'handler può liberare memoria e ritentare. `nothrow` restituisce null quando non resta un handler; ordinary new senza handler interrompe il processo con trap anziché restituire un falso puntatore valido. Gli handler non devono lanciare eccezioni.
- `cpp/src/guard.cpp`: ABI x86-64 Itanium `__cxa_guard_acquire/release/abort` per gli static locali emessi dal compilatore. Il primo byte del guard è pubblicato release solo dopo la costruzione; lettura acquire. Registro dinamico degli inizializzatori, identità native dei thread, mutex/condition nativi; nessuna tabella a capacità fissa. Supporto a nesting, attesa dei concorrenti e abort/retry; ricorsione dello stesso inizializzatore sullo stesso thread rifiutata con trap.

Richiede TCB/errno nativo attaccato prima dell'allocazione e dell'inizializzazione dinamica. Nessuna modifica ai programmi del sistema distribuito: i nuovi simboli sono collegati soltanto negli oggetti di verifica e nel probe QA.

## Verifiche e stato

- Suite host della milestone new/guard: **13 gate PASS**, inclusa compilazione freestanding C/C++ e test normali + ASan/UBSan.
- Il test host esercita l'heap e gli algoritmi nativi di thread/sincronizzazione con trasporti pthread/futex/mmap, non il kernel target. Quattro worker, deallocazione cross-thread, 12.000 distruttori di array, static locali concorrenti e annidati, abort/retry e recupero via new_handler dopo esaurimento delle 32 riserve VM. Casi negativi in processi separati verificano SIGILL per OOM non gestita e inizializzazione ricorsiva.
- Il binario host normale non collega libstdc++. La variante UBSan carica indirettamente libstdc++ per la diagnostica del sanitizer: il test verifica comunque le definizioni forti del nostro eseguibile per new e guard, non attribuisce quel runtime al target.
- Profili guest funzionali **1, 4 e 8 CPU PASS** sullo stesso ISO SHA-256 `77df68dd13a0229fd5d0c3b24455453ec890544d2427226bf9c9b37a60c34c70`: quattro worker, 12.000 distruttori per profilo, esaurimento reale delle 32 riserve e recupero tramite new_handler, inizializzazione statica una sola volta, nesting, abort/retry concorrente, nessun simbolo irrisolto e zero riserve finali. Sui profili SMP è obbligatoria l'esecuzione su più CPU.
- **Limite storico della milestone precedente (ora riprovato con cache opt-in, vedere sopra):** lo stress iniziale liberava il buffer trasferito prima del ciclo e poteva quindi restituire al kernel l'ultimo span a ogni piccola coppia new/delete. A 4 CPU ha superato il termine di 145 secondi. Gli snapshot QEMU mostravano attività kernel in TLB shootdown/scheduler; sono compatibili con churn VM molto costoso, ma non provano da soli l'assenza di un altro problema kernel. Questo stress NON è un PASS e richiede profilazione/correzione della politica di rilascio o del percorso VM.
- Nella verifica funzionale finale ogni worker **mantiene vivo il proprio buffer trasferito fino alla fine**, ne verifica di nuovo i dati e lo elimina dal thread destinatario. Restano quattro worker, 1.000 cicli per worker, 12.000 distruttori, identica contesa sui guard, deadline di 145 secondi e zero riserve finali: non vengono ridotti i cicli né alzato il timeout. È una modifica esplicita della forma del carico, **non un fix dell'allocatore e non la convalida dello stress non ancorato**. Il sorgente esatto di quest'ultimo, con hash confrontato al manifest compilato, la ricevuta false e i registri sono conservati in `test-results/cpp/unanchored-stress-timeout-evidence.tar.xz`.
- I 13 gate host e i tre profili C++ non sono una riesecuzione dell'intera suite guest storica e non sono test del browser. Ricevute nuove in `test-results/cpp/`; quelle di heap/thread restano storiche e separate.

Le asserzioni sono condivise in `tests/cpp_cases.cpp`. Il driver guest verifica anche otto new e dodici delete effettivamente definiti e tre guard, assenza di simboli eseguibili irrisolti e corrispondenza fra sorgenti, manifest, ISO e kernel realmente estratto. Il mezzo è diagnostico: tutti gli alias dei comandi eseguono il probe. **Non distribuirlo né caricarlo come sistema operativo o browser.**

Il linker del probe mantiene i GOT load dei callback C/C++ con `--no-relax`: il userspace vive sopra il limite degli indirizzi assoluti a 32 bit; la precedente rilassazione ld produceva un errore GOTPCREL. Non viene disabilitato alcun controllo di integrità del loader.

La base QA viene ricostruita dagli attuali sorgenti OS con `TMC_FS2_BACKEND=1` e `TMC_USB_CANONICAL=0`; non sostituisce i file pubblicati e non è dichiarata identica byte per byte alla ISO 10.2.5. La Recovery conserva il volume sigillato originale della build di base.

## Riproduzione

Toolchain, build e media devono rimanere fuori da `/home/user`:

```sh
sh workspace/webkit-tmc/tools/test-cpp-runtime.sh
python3 workspace/webkit-tmc/tools/run-tests.py --output /var/tmp/cpp-host.json

export TOOLROOT=/var/tmp/tmc-tools
export TMC_QA_RESULTS=/var/tmp/cpp-results
mkdir -p "$TMC_QA_RESULTS"
bash workspace/webkit-tmc/tools/build-guest-cpp-probe.sh
export TMC_PRODUCTION_BUILD=/var/tmp/tmc-webkit-10.3-system
TMC_SMP=4 python3 workspace/webkit-tmc/tools/test-guest-cpp.py
```

Richieste le dipendenze Python/Pillow e gli strumenti QEMU/OVMF/FAT/xorriso nel prefisso esterno. Il test deve fallire su timeout o deadlock; non va alzato il conteggio dei PASS né esclusa la contesa per aggirare il problema.

## Ancora mancante

Questo milestone new/guard non completa libc, libc++, eccezioni/unwind/RTTI, compiler TLS o pthread generale. Il successivo macroblocco [RUNTIME-LIFECYCLE.md](RUNTIME-LIFECYCLE.md) aggiunge startup globale esplicito opt-in e `__cxa_atexit/__cxa_finalize`, con nuove prove separate; non aggiunge uno startup automatico a tutte le app né C `exit` completo. Non è bmalloc e non contiene un build di WTF/JavaScriptCore/WebCore. Restano il collegamento del vero motore, HTTPS con validazione, font/grafica/event loop, UI e acceptance HTML/CSS/JavaScript/temi/sicurezza. I relativi gate restano false. Nessun renderer sostitutivo e nessuna shell vuota presentata come Safari.
