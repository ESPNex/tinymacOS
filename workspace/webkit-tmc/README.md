# Backend nativo per il port WebKit

Lavoro preparatorio sul runtime di tinymacOS. **Non è un browser e non è ancora un port compilabile di WebKit, JavaScriptCore o WebCore.** Non è incluso come Safari nella ISO.

## Ripresa dopo 10.2.5

La base OS corrente è **10.2.5 / 30B16**, già pubblicata senza Safari. Lo sviluppo è ripreso dal runtime esistente ed è ora consolidato in `main` (il ramo temporaneo `work/safari-webkit-runtime` è stato unito e rimosso); i riferimenti a 2.4.x nei documenti storici non sono la prossima versione promessa. Nessun renderer sostitutivo e nessuna shell vuota: obiettivo invariato, UI Safari desktop classica identica e motore WebKit completo.

Il primo blocco nuovo è l’allocatore VM e il sottoinsieme C di allocazione, descritto in `RUNTIME-HEAP.md`. Checkout completo di WebKit, toolchain e build rimangono fuori da `/home/user`; sorgenti del port e ricevute sono salvati su GitHub, con workspace sotto 128 MB decimali.

## Componenti presenti

- `posix/`: adattatori prefissati `tmc_` per rete, descriptor, VM, clock, TLS per-thread, sincronizzazione, ciclo di vita dei thread e heap VM. `libc_alloc.c` esporta soltanto le cinque funzioni C di allocazione in modo opt-in. Non sostituiscono una libc/POSIX completa.
- `native-abi/`, `kernel-overlay/`: header e sorgenti d'integrazione con il sistema nativo; verificare la direzione di ogni sincronizzazione.
- `platform/`: lettura dello stato d'aspetto del sistema, non un'interfaccia browser.
- `upstream-reference/`, `upstream.lock.json`: sottoinsieme di riferimento e commit fissato; non un checkout completo del motore.
- `tools/`, `tests/`, `test-results/`: harness, contratti host e ricevute guest.

## Prove

Da questa directory:

```sh
python3 tools/run-tests.py
python3 tools/run-tests.py --guest
```

La seconda modalità richiede toolchain e media QA esterni. Le ricevute della pipeline 10.1.5 riportano 29 regressioni kernel/runtime e verificano l'associazione al kernel di produzione. I risultati restano legati ai relativi hash e non validano automaticamente modifiche successive.

Le prove coprono anche VM/SMP, FS/errno per-thread, attesa/risveglio, mutex/condition e startup/join/TSD. I conteggi dei vecchi milestone non vanno sommati a quelli correnti. Il TLS per-thread riguarda il thread pointer, non HTTPS.

## Lavoro mancante

Restano libc e runtime C/C++, integrazione pthread/WTF, esecuzione di JavaScriptCore sul guest, grafica/font/event loop, WebCore e HTTPS con validazione. L'eventuale applicazione Safari dovrà usare il motore reale e superare test HTML/CSS/JavaScript: nessun renderer sostitutivo.

`release-gates.json` definisce i criteri di accettazione; `LIMITATIONS.md` descrive i limiti. Le analisi `THREADING.md` e `RUNTIME-THREADS.md` approfondiscono i contratti di thread senza duplicarli qui.
