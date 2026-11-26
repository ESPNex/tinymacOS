# 10.3 — megablocco runtime integrato

## Ambito e risultato

Estensione congiunta di heap, compatibilità pthread e integrazione C++: non una nuova funzione isolata. La 10.2.5 distribuita non viene modificata; nessuna ISO diagnostica viene pubblicata. **Non è ancora WebKit né Safari funzionante.** Rimangono richiesti il vero WTF/JavaScriptCore/WebCore e la UI Safari classica identica al riferimento, senza renderer sostitutivo.

**Risultato finale: 31/31 casi PASS nella stessa esecuzione.**

Accettazione riproducibile: `tools/run-tests.py --runtime-megablock`, con 15 gate host, quattro build diagnostiche (la prima ricompila la base OS) e dodici profili guest: heap, C++ originale non ancorato, lifecycle e pthread, ciascuno su 1/4/8 CPU. Esito finale e hash in `test-results/megablock/verification.json` e nell'archivio di evidenze collegato.

## Heap: cache limitata, pressione e trim

- Politica **opt-in**, sincronizzata dallo stesso mutex dell'heap. Il default rimane rilascio immediato, così i programmi esistenti non cambiano comportamento.
- `tmc_heap_set_cache_limit(bytes)`: budget 0…262144 byte. Gli span interamente vuoti possono restare riutilizzabili; gli span grandi vengono rilasciati. Non sono nascosti nei contatori: restano in `reserved/spans`, con `cached` separato.
- `tmc_heap_trim()`: rilascia tutti gli span vuoti e le riserve non committate, senza toccare gli oggetti vivi. Non cambia il budget futuro. Ridurre il budget tenta il rilascio dell'eccesso.
- Esaurimento degli slot o fallimento di reserve: tenta di recuperare la cache prima di fallire. I contatori esposti includono reserve/release tentate, release riuscite e riusi di span interamente vuoti.
- **Fallimento release:** proprietà della riserva sempre conservata e ritentabile; l'heap può quindi superare il budget se il kernel rifiuta il rilascio. `trim`/riduzione restituiscono errore, non simulano memoria liberata. Anche commit seguito da release fallita resta tracciato, senza leggere pagine non committate.
- Test host normali + ASan/UBSan: stress preesistente, fault reserve/commit/release, 2000 riusi senza nuove reserve, trim con oggetto vivo, fallimento trim e riduzione, pressione sugli slot, azzeramento finale.
- Guest heap: i precedenti 9000 passi randomizzati e quattro worker rimangono; si aggiungono 2000 riusi verificati con **delta reserve zero**, controllo dati durante trim e zero riserve finali.

### Stress C++ originale ripristinato

Il corpo di `cpp_test_worker` è ripreso **byte per byte** dal sorgente conservato nell'evidenza del vecchio timeout SMP4: libera il buffer trasferito **prima** del ciclo, senza un'allocazione-ancora che tenga vivo lo span. Non sono ridotti i 4×1000 round né i 12000 distruttori; non è aumentata la scadenza interna di 145 secondi.

La nuova politica viene abilitata dopo i test di base/OOM e prima dei worker. La fine del test disabilita la cache ed esegue trim **prima** delle vecchie asserzioni di zero riserve. Si tratta quindi di un fix del comportamento con cache esplicitamente abilitata, non di una presunta accelerazione del percorso eager-release di default né di una prova di assenza di altri problemi kernel. L'evidenza negativa originale resta conservata; non è riscritta come PASS retroattivo.

## pthread: sottoinsieme sorgente, non ABI binaria

`posix/include/tmc_pthread.h` espone i nomi prefissati. Il percorso di include **separato e opt-in** `posix/pthread-headers/pthread.h` mappa i nomi convenzionali alle implementazioni prefissate. Non si esportano i simboli pthread della libc host e non si interposiziona l'adattatore su glibc/ASan.

Implementati:

- create/join di thread joinable e child `pthread_exit`; identità monotone non riutilizzate, registro protetto, un solo joiner, rifiuto di self-join e handle già consumati. Il controllo viene liberato solo dopo il completamento nativo/TCB e il join.
- mutex private con ownership, DEFAULT/ERRORCHECK, NORMAL e RECURSIVE; unlock da thread estraneo rifiutato, ricorsione errorcheck con EDEADLK, trylock busy, destroy occupato con EBUSY. Nessuna copia di oggetti vivi o distruzione concorrente con l'uso.
- condition private, signal/broadcast e attesa in ciclo sul predicato; mutex riacquisito anche su timeout. L'estensione `tmc_pthread_cond_wait_until` usa tick monotoni nativi assoluti a 100 Hz: **non** è `pthread_cond_timedwait` POSIX/realtime. Attesa con profondità ricorsiva maggiore di uno rifiutata.
- chiavi TSD generation-safe e distruttori nativi con reinserimento, once con pubblicazione concorrente, callback annidate e rilevamento della ricorsione sullo stesso once. La callback once deve ritornare normalmente.
- errori restituiti direttamente ed errno preservato; OOM del controllo restituisce EAGAIN senza modificare l'handle in uscita. La capacità thread reale è limitata dal runtime/kernel, non dichiarata illimitata.

**Non implementati:** detach, cancellazione/cleanup stack, pthread_exit del thread iniziale, scheduling/affinità, stack attributes, mutex robust/priority inheritance, process-shared, timedwait POSIX, segnali/atfork, compiler TLS o compatibilità con binari libpthread stranieri. Le opzioni detached/process-shared disponibili negli attr setter sono rifiutate esplicitamente con ENOTSUP. Il thread iniziale deve prima attaccare il TCB nativo.

## Prove integrate C e C++

Lo stesso consumer C include `<pthread.h>` su host e target, ma il trasporto host è compilato separatamente con il vero header pthread del sistema. Quattro worker eseguono 2000 aggiornamenti protetti, alloc/realloc/free, trim concorrente, once e chiavi TSD; un worker termina tramite pthread_exit. Verificati otto richiami distruttori TSD, timeout con riacquisizione, errori ownership/ricorsione/destroy, join concorrente allo stesso target, handle obsoleti, sedici cicli di creazione/join e **vero esaurimento della capacità thread con successivo recupero**.

Un consumer C++ aggiuntivo usa lo stesso header pthread, un lock RAII, `new`, uno static locale conteso non banale e il distruttore DSO emesso dal compilatore. Dopo il join, finalizzazione esplicita e zero registrazioni/oggetti/span residui. Il normale binario host non collega direttamente libstdc++; ASan/UBSan sono strumenti host, non parte del target. I soli simboli lifecycle degli oggetti di test host vengono rinominati per non intercettare glibc/sanitizer.

La regressione lifecycle separata mantiene due costruttori globali, uno static locale, 400 registrazioni concorrenti, 401 callback worker e tre distruttori compiler-generated. Non è sostituita da un test pthread più debole. Tutti i profili SMP richiedono esecuzione su più CPU.

I probe verificano assenza di simboli irrisolti, hash dei sorgenti prima/dopo build, kernel attuale e KernelCache estratta indipendentemente dalla ISO. Quattro immagini diagnostiche distinte, ciascuna identica nei propri tre profili CPU. Questi non sono test HTTPS/DOM/CSS/JavaScript, né una riesecuzione di tutta la QA storica della 10.2.5.

## Riproduzione e conservazione

```sh
sh workspace/webkit-tmc/tools/test-pthread.sh
sh workspace/webkit-tmc/tools/test-heap.sh
PYTHONDONTWRITEBYTECODE=1 \
PYTHONPATH=/var/tmp/tmc-python-release \
TOOLROOT=/var/tmp/tmc-tools \
python3 workspace/webkit-tmc/tools/run-tests.py --runtime-megablock \
  --output /var/tmp/tmc-megablock-final/results/acceptance.json
```

Toolchain, immagini e build rimangono fuori dal workspace. Le evidenze persistenti comprendono ricevute originali, stdout/stderr dei gate, manifest, sorgenti esatti e crop lossless del testo framebuffer. I frame completi restano scratch esterno: ne sono registrati hash e percorso, non sono dichiarati persistenti.

Per restare entro 128 MB tre vecchie evidenze sono state scaricate e confrontate byte per byte da commit GitHub immutabili **prima** della rimozione della copia locale; collegamenti/hash in `test-results/REMOTE-OLDER-RUNTIME-EVIDENCE.json`. Nessuna evidenza storica è stata cancellata dal repository remoto.

## Dopo questo blocco

Resta da portare il vero motore e risolvere i requisiti effettivi delle versioni scelte di WTF/JSC/WebCore: librerie C/C++, clock/event loop, file/rete/HTTPS, grafica/font/input e UI Safari/temi. L'adattatore pthread qui non viene presentato come sufficiente per compilare automaticamente WebKit. Nessuna percentuale arbitraria di completamento del browser e nessuna release intermedia.
