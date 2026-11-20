# Macroblocco runtime thread — completato e verificato

Questo passo raggruppa **startup TLS dei figli, uscita finale, join, dati per-thread e distruttori**, più un consumatore C++ freestanding e l’intera regressione. Non è un insieme di funzioni pthread vuote e non è ancora WTF/JavaScriptCore compilato.

## Implementazione effettiva

- `posix/src/threads.c`, `posix/include/tmc_threads.h`: oggetti thread caller-owned, inizializzati a zero e mantenuti mappati fino al join. I figli ricevono il TCB e un’identità process-local non riutilizzata **prima** di entrare nella callback. La callback può usare l’errno target senza installare manualmente il TLS.
- Thread iniziale/esterno: bootstrap esplicito `tmc_runtime_enter`, uscita dal runtime `tmc_runtime_leave`; nessuna pretesa di startup C/C++ generale prima dei costruttori statici.
- `S_THREADFINISH`/`threadfinishabi.h`: il kernel azzera FS, pubblica con release la completion word, sveglia i waiter privati e ritira il chiamante. Dopo la pubblicazione non accede più alla memoria utente del controllo/completion. Stack kernel e Task sono protetti separatamente dal meccanismo `at`/reap già presente.
- Il risultato e i distruttori precedono la pubblicazione finale. Il join acquisisce la completion e può poi consentire di riusare o smappare il TCB: non è un semplice flag scritto da un thread che deve ancora eseguire cleanup sul proprio oggetto.
- Join esclusivo, join con deadline assoluta, retry dopo timeout, rifiuto del self-join e secondo join invalido. Un timeout non modifica il puntatore risultato del chiamante e non termina la vita dell’oggetto. Risultati NULL e a larghezza piena sono preservati.
- TSD: 32 chiavi registrate con generazioni, rifiuto degli handle cancellati/stale e nessun wrap silenzioso. I valori sono privati al TCB; riusare lo slot di una chiave non espone i valori della vecchia generazione.
- Distruttori fuori dal lock del registro, valore azzerato prima della callback, ripopolamento consentito e massimo quattro passaggi. Le callback possono creare/cancellare altre chiavi. Cancellare una chiave non invoca il distruttore.
- Nessuna allocazione nascosta per gli oggetti thread e nessun fallback errno globale. Storage, durata dei dati e ordine create/join restano responsabilità esplicite del chiamante.

## Cosa è stato realmente eseguito

Suite ripetibile `tools/run-tests.py --guest`: **29/29 PASS**.

- 11 controlli host, contratti, budget e compilazione target;
- 5 build diagnostiche esterne;
- 13 profili guest: rete/VM 1/2 CPU, sync/FS-errno/nuovo runtime 1/4/8 CPU.

Per ciascun profilo nativo del nuovo runtime:

- **90 creazioni di base**, più i figli bloccati usati per esaurire gli slot Task reali fino a EAGAIN;
- quattro worker, 320 aggiornamenti protetti da mutex e yield nel tratto critico; per i profili SMP campioni di esecuzione su più di una CPU richiesti, senza pretendere che ogni thread migri;
- TCB automatico prima della callback, identità distinte, errno isolato e TSD separati;
- ritorno normale ed exit esplicita con cleanup identico;
- join esclusivo, self-join rifiutato, timeout con successivo join riuscito e risultato non alterato dal timeout;
- esaurimento chiavi, cancellazione/ricreazione con generazioni e rifiuto di handle stale;
- distruttore ripopolato due volte, distruttore che si ripopola fino al limite di quattro passaggi, creazione/cancellazione chiave dentro un distruttore;
- **64 riusi** dello stesso controllo dopo join, con identità nuova e nessun vecchio TSD;
- **16 TCB effettivamente allocati in VM e smappati subito dopo il join**, non copie o puntatori fittizi;
- esaurimento degli slot kernel: fallimento reale, oggetto di creazione fallita resettato, rilascio/join dei figli e successive creazioni riuscite;
- leave/riattach del runtime iniziale senza riusare identità o dati vecchi.

Test hosted con ASan/UBSan: stessi casi logici, senza VM nativa, con pthread/futex come **trasporto di test solamente** e limite artificiale di otto figli per esercitare la gestione dell’esaurimento. Non si confonde questo limite host con quello dei Task reali nel guest.

### C++ effettivo, ma limitato

`tests/threads_cpp_smoke.cpp` è compilato con g++ C++20 freestanding, senza header della libreria standard C++, eccezioni o RTTI. Usa un oggetto RAII per il mutex, un thread nativo, il TLS e il join, passando per la vera ABI C dei nuovi adattatori. Il guest lo esegue e controlla risultato, sblocco RAII ed errno/cella TLS preservata.

Il programma guest viene linkato con `ld -nostdlib`; il runner verifica che non abbia simboli indefiniti. **Questo non prova l’esistenza di libstdc++/libc++, std::thread, new/delete, eccezioni, compiler thread_local o del runtime C++ completo.** Nessuna di queste funzioni mancanti viene fatta riuscire con stub.

## File e prove

- `tests/thread_cases.h`: asserzioni condivise tra host e guest;
- `tests/guest_threads_probe.c`: trasporto nativo, invalid completion e VM/unmap reali;
- `tests/threads_host_transport.c`: trasporto host, esplicitamente non target;
- `tools/test-threads.sh`, `tools/test-guest-threads.py`;
- `test-results/guest-threads-{source-hashes,build,smp1,smp4,smp8}.json`;
- `test-results/milestone-tests.json`: tutti e 29 i controlli;
- `test-results/threads/`: ritagli di screenshot reali dei Terminali guest.

Hash dei sorgenti salvati prima delle build; nel manifest del nuovo guest sono inclusi anche il file C++, gli assert condivisi e le versioni di gcc/g++.

## Confine con WTF/JavaScriptCore e prossimo macroblocco

Gli header upstream reali sono già conservati con pin/hash in `upstream-reference/`. Ora esistono fondazioni native per creazione, identità, join, mutex/condition e TSD con distruttori. **Non sono ancora collegati ai tipi e alle chiamate pthread usati da WTF** e non equivalgono a un port compilabile.

Il successivo macroblocco deve coprire in modo coerente:

1. allocator/libc e runtime C/C++ target realmente linkabili;
2. startup del programma e relativi costruttori, metadati stack, gestione thread detached o un’alternativa corretta, clock realtime/WallTime;
3. selezione di piattaforma e collegamento delle API native ai veri sorgenti WTF/JSC, senza spacciarsi per Linux né alterare i test per nascondere funzioni assenti;
4. build e test del motore reale, poi WebCore, rete HTTPS validata, font/rendering e Safari classico in tutti i temi.

Restano esplicitamente mancanti: pthread completo, detach/cancel, suspend/resume e stack scanning per il collector, segnali, robust/PI/process-shared mutex, ELF/DTV/compiler TLS, eccezioni/unwind e la standard library C++ target. I descriptor condivisi richiedono ancora sincronizzazione e ownership appropriate. Il bootstrap iniziale non è automatico per tutte le applicazioni OS preesistenti.

Nessun nuovo deploy/ISO upload in questo macroblocco. Le immagini diagnostiche sono esterne e non distribuibili; la release OC2 resta immutata. La futura release browser rimane **2.4.5 Levante**, subordinata ai gate reali del motore e dell’interfaccia.
