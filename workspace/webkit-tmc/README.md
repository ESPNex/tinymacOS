# Backend nativo per il port WebKit

Lavoro preparatorio sul runtime di tinymacOS. **Non è un browser e non è ancora un port compilabile di WebKit, JavaScriptCore o WebCore.** Non è incluso come Safari nella ISO.

## Componenti presenti

- `posix/`: adattatori prefissati `tmc_` per rete, descriptor, VM, clock, TLS per-thread, sincronizzazione e ciclo di vita dei thread. Non sostituiscono una libc/POSIX completa.
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
