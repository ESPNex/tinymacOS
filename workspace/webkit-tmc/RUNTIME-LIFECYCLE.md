# 10.3 / Safari — macroblocco ciclo di vita C++

## Risultato

**PASS: 14 gate host + build diagnostica fresca + guest nativi 1/4/8 CPU**, nella stessa esecuzione `--lifecycle-guest` (18 casi). Nessuna release o ISO pubblicata; nessun cambiamento ai binari della 10.2.5. Il recupero delle release storiche è stato interrotto su richiesta: nessuna release storica creata.

Questo blocco completa un altro sottoinsieme del runtime necessario al futuro port WTF/JavaScriptCore: **inizializzazione globale esplicita e distruzione di processo/DSO**, senza eccezioni. Non è libc++, un loader dinamico o WebKit.

## Implementazione

- `cpp/src/lifecycle.cpp`: `tmc_cpp_run_initializers`, `__cxa_atexit`, `__cxa_finalize`, contatore dei distruttori pendenti. Nessuna dipendenza da librerie C++ host nel target.
- Il runner `.init_array` inizializza una sola volta un intervallo immutabile condiviso, con pubblicazione tramite mutex/condition nativi. Chiamate concorrenti attendono il completamento; la ricorsione dallo stesso inizializzatore restituisce `EDEADLK`. Un intervallo differente viene rifiutato con `EINVAL`.
- Registro dinamico dei distruttori sul vero heap nativo, senza tabella a capacità prefissata. OOM restituisce errore senza cambiare il registro.
- Finalizzazione selettiva per identità DSO o globale con puntatore nullo, nell’ordine inverso di registrazione. Il record viene rimosso **prima** della callback e nessuna callback viene eseguita sotto il mutex del registro.
- Finalizzatori concorrenti serializzati; rientranza sullo stesso thread permessa, anche sullo stesso DSO. Le nuove registrazioni fatte da un distruttore entrano nella scansione successiva. Chiamate ripetute non rieseguono callback consumate. `errno` del finalizzatore viene conservato.
- `cpp/lifecycle.ld`: layout opt-in con gli stessi indirizzi alti e segmenti W^X del userspace nativo, più `.init_array` conservata e ordinata per priorità. Il probe fornisce il proprio `__dso_handle`.

**Prerequisito:** TCB nativo già attaccato. Il bootstrap è esplicito: non sono stati modificati il loader, `userspace/start.S` o lo startup delle app distribuite. Il chiamante deve completare l’inizializzazione e fermare/joinare gli altri thread prima della distruzione finale del processo. Registrazioni tardive concorrenti dopo l’ultima scansione richiedono un’altra finalizzazione. Callback che registrano senza fine possono impedire la terminazione; callback devono ritornare normalmente. Niente cancellazione o gestione di eccezioni/signali.

## Verifiche effettive

Lo stesso `tests/lifecycle_cases.cpp` viene compilato su host e target:

- **due oggetti globali C++**, con allocazione nel costruttore e distruttori realmente registrati dal compilatore;
- uno **static locale non banale** conteso, con costruttore/distruttore una sola volta;
- quattro worker attendono una barriera prima di concorrere allo startup;
- **400 registrazioni concorrenti**, più una registrazione eseguita all’interno di un distruttore: **401 callback worker**;
- DSO intercalati: finalizzazione selettiva e verifica esatta dell’ordine inverso, inclusi sei callback di controllo separati;
- finalizzatori su quattro thread, ricorsione sullo stesso thread/DSO e ripetizione senza doppia distruzione;
- distruttori compiler-generated in ordine **static locale → secondo globale → primo globale**;
- nessun distruttore pendente e **zero span, byte riservati e oggetti heap vivi** alla fine;
- fallimento della prima allocazione del registro e callback nulla verificati su host. Il fault injection non è dichiarato eseguito nel guest;
- ASan/UBSan nelle prove host; il normale eseguibile host non dipende da libstdc++. Per evitare interferenze con glibc/sanitizer, solo negli oggetti di test host i due simboli ABI sono rinominati con `objcopy`; il target esporta i veri nomi `__cxa_atexit/__cxa_finalize`;
- su host viene spostata soltanto la sezione `.init_array` del consumatore nella sezione di test chiamata esplicitamente: le inizializzazioni dei sanitizer rimangono automatiche. Nel target è verificata la presenza reale di `.init_array`, dei simboli ABI e l’assenza di simboli eseguibili irrisolti;
- stessa ISO diagnostica per 1/4/8 CPU, kernel confrontato con la base appena compilata ed estratto indipendentemente dalla ISO. Profili SMP richiedono esecuzione su più CPU. Scadenza interna invariata: 145 secondi; timeout/deadlock non sono PASS.

La prima compilazione del probe si è fermata per un include path mancante verso `cpp/include` nel driver C. Corretto solo il builder QA, senza alterare il kernel o le asserzioni. Log del tentativo conservato nelle evidenze. La matrice finale ha ricostruito la base e il probe ed è passata integralmente.

## Riproduzione

```sh
sh workspace/webkit-tmc/tools/test-cpp-lifecycle.sh

# Toolchain/Python/QEMU esterni al workspace:
export TOOLROOT=/var/tmp/tmc-tools
export PYTHONPATH=/var/tmp/tmc-python-release
export PYTHONDONTWRITEBYTECODE=1
python3 workspace/webkit-tmc/tools/run-tests.py \
  --lifecycle-guest --output /var/tmp/tmc-lifecycle-final/acceptance.json
```

Per i prerequisiti della toolchain vedere i precedenti documenti runtime. Media e dischi VM rimangono esterni; non distribuire il mezzo diagnostico, dove tutti gli alias dei comandi avviano il probe.

## Confini e prossimo macroblocco

Lo stress C++ **non ancorato** scaduto nel precedente milestone resta **aperto**: questi test non correggono la politica di rilascio heap né il percorso SMP/TLB. I PASS di thread/heap/C++ delle milestone precedenti non diventano nuove riesecuzioni guest: qui sono nuovi soltanto i tre profili lifecycle e i 14 gate host.

Prossimo lavoro: politica di rilascio/riuso dell’heap e ripetizione del carico non ancorato, quindi colmare libc/pthread/TLS richiesti dal vero WTF/JSC. Rimangono incompleti libc/libc++, unwind/eccezioni/RTTI, compiler TLS e relativi distruttori, C `exit` completo, caricamento/scaricamento DSO, bmalloc/GC/JIT, build WTF/JavaScriptCore/WebCore, HTTPS validato, rendering e UI Safari classica identica con tutti i temi. I gate di browser e release restano false.
