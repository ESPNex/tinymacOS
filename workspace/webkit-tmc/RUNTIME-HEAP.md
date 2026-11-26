# Safari/WebKit — milestone heap nativo dopo 10.2.5

## Risultato verificato

Ripresa del port esistente sul ramo `work/safari-webkit-runtime`, dalla base Git `c124fde1de76655f70d50aca571a4b09e41a408b` (tinymacOS **10.2.5 / 30B16**, già pubblicata). **Non è Safari pronto e non è ancora un build di WTF, JavaScriptCore o WebCore.** Requisito invariato: interfaccia Safari desktop classica identica al riferimento del progetto, tutti i temi e motore WebKit completo; niente renderer sostitutivo o shell vuota.

Questo blocco aggiunge un vero heap di processo sopra S_VM e il mutex nativo già esistente. Non modifica sorgenti del sistema operativo di produzione e non modifica né ripubblica la release 10.2.5.

| Verifica fresca | Esito | Portata |
|---|---|---|
| Suite host corrente | **12/12 PASS** | Backend, heap, thread, sincronizzazione, VM, descriptor, stream kernel, aspetto, oggetti target e sincronizzazione sorgenti/budget |
| Heap host normale e ASan/UBSan | **PASS entrambi** | 25.000 operazioni pseudocasuali per esecuzione, quattro worker pthread, trasporto VM `mmap` con guardie e iniezione di errori |
| Guest nativo 1 CPU | **PASS** | 9.000 operazioni, quattro worker nativi, ABI C e consumatore C++ RAII |
| Guest nativo 4 CPU | **PASS** | Come sopra, esecuzione su più CPU obbligatoria |
| Guest nativo 8 CPU | **PASS** | Come sopra, esecuzione su più CPU obbligatoria |
| Stato finale guest | **0 span, 0 oggetti vivi, 0 byte riservati** | Per tutti e tre i profili |
| WTF/JSC/WebCore, HTTPS, UI Safari | **NON COMPLETATI** | Nessun gate browser sbloccato da queste prove |

I conteggi host e guest sono distinti; non sono 15 test di browser e non sono la ripetizione dell'intera vecchia suite guest da 29 casi. Ricevute nuove in `test-results/heap/`; quelle storiche restano separate.

## Implementazione

- `posix/src/heap.c`, `posix/include/tmc_heap.h`: allocazione, calloc, realloc, aligned allocation, free e statistiche, con API prefissata.
- `posix/src/libc_alloc.c`, `posix/libc-headers/stdlib.h`: cinque simboli C **opt-in** `malloc`, `calloc`, `realloc`, `aligned_alloc`, `free`; il nuovo header è soltanto un sottoinsieme, non una libc completa. I programmi e la ISO di produzione non li incorporano automaticamente.
- Span normalmente da 256 KiB, riserva VM con una pagina di guardia su ciascun lato e commit read/write. Richieste più grandi usano uno span adeguato ai limiti S_VM. Nessuna memoria eseguibile.
- Primo blocco libero compatibile, metadati in-band, split e coalescenza; realloc ridimensiona in-place quando possibile, altrimenti copia e conserva l'oggetto originale in caso di fallimento.
- Allineamento fondamentale di 16 byte; allineamenti richiesti potenze di due da 8 byte a 2 MiB, con dimensione multipla dell'allineamento per `aligned_alloc`. Zero-size alloc restituisce un oggetto liberabile; `realloc(p,0)` libera e restituisce NULL.
- Overflow e memoria esaurita falliscono esplicitamente. `free(NULL)` e free riuscita preservano errno. Tutti i chiamanti devono avere un TCB/errno attaccato prima dell'uso.
- Un mutex nativo protegge tutti i metadati e consente free da un altro thread. Uno span interamente libero viene restituito al kernel. Se release fallisce rimane tracciato/riutilizzabile; se falliscono commit e successiva release, la riserva resta tracciata senza dereferenziare pagine non committate e viene ritentata la release a un'allocazione successiva.

## Prove e loro limiti

`tests/heap_cases.h` esercita la stessa implementazione nei due ambienti; i trasporti sono distinti. Il test host usa pthread e mmap, **non** il kernel target. Il guest usa realmente S_VM, word parking/mutex, startup TLS/errno e thread nativi. La prova C++ è freestanding C++20, senza STL/eccezioni/RTTI: un oggetto RAII chiama i veri simboli C di allocazione e libera il buffer nel distruttore. Non valida libc++, `operator new/delete`, bmalloc o ABI di WebKit.

Contratti coperti: zero-size, overflow calloc, azzeramento dopo riuso, allineamenti 8/16/64/4096/2097152, realloc fallita senza perdita dei dati, crescita e riduzione in-place, split/coalescenza, pattern di dati, concorrenza, free cross-thread e ritorno completo degli span. Sul solo host vengono inoltre iniettati fallimenti reserve/commit/release e limite delle regioni. ASan/UBSan controllano il programma host, ma non forniscono redzone per ciascun sotto-blocco del nostro heap: i pattern e le guardie VM sono verifiche aggiuntive, non una prova di assenza di ogni overflow.

Il mezzo QA è costruito con sorgenti OS correnti invariati: il probe verifica hash dell'ISO, input compilati del probe, manifest dei sorgenti OS, assenza di simboli eseguibili irrisolti e kernel realmente estratto dall'ISO uguale alla build di base usata. La build di base è una ricompilazione separata, non una dichiarazione di identità binaria con la ISO pubblicata. Il kernel copiato nel mezzo QA ha KernelCache SHA-256 `a69fa3932bb6c665f9b0383e256446f0053a640b7d0a58f270e4e36f4b821ac5`.

I PASS vengono riconosciuti da pixel/testo del Terminale retail, non da un servizio diagnostico aggiunto al kernel. Le PNG conservate sono ritagli lossless della finestra reale; `terminal-crops.json` registra box e hash degli originali/ritagli. Gli originali completi in `/var/tmp` sono scratch non durevole, non un backup promesso. Tutti i comandi nel volume QA eseguono il probe: **NON distribuire né caricare questa ISO come sistema o browser funzionante.**

## Allineamento alla base di produzione

L'overlay storico includeva copie antecedenti alle ABI della release corrente. `sync-system-sources.py --capture` ora copia **dal sistema autorevole al workspace WebKit**, includendo le dipendenze `base.h`, `diskedit.h`, `partition.h`, `clipboard.h`. `--check` è realmente in sola lettura; `--apply` resta esplicito e non è stato usato in questo blocco. Verificate 25 copie di integrazione e 11 copie ABI. Tutti i 986 input congelati della release 10.2.5 sono rimasti invariati.

La costruzione QA è stata aggiornata per conservare il volume Recovery originale: i loader EFI ne verificano il digest incorporato. Non viene disabilitato il controllo di integrità per far passare il probe.

## Riproduzione

Build e dipendenze devono restare fuori da `/home/user`:

```sh
# Dalla radice del repository; non sovrascrive la vecchia ricevuta guest.
python3 workspace/webkit-tmc/tools/run-tests.py \
  --output /var/tmp/heap-host-results.json

# TOOLROOT deve contenere tool EFI, FAT, xorriso, QEMU/OVMF.
export TOOLROOT=/var/tmp/tmc-tools
export TMC_QA_RESULTS=/var/tmp/heap-results
mkdir -p "$TMC_QA_RESULTS"
bash workspace/webkit-tmc/tools/build-guest-heap-probe.sh

export TMC_PRODUCTION_BUILD=/var/tmp/tmc-webkit-heap-system
# Configurare eventuale PYTHONPATH per le dipendenze Python esterne.
for cpu in 1 4 8; do
  TMC_SMP=$cpu python3 workspace/webkit-tmc/tools/test-guest-heap.py || break
done
```

`tools/run-tests.py --guest` include ora anche costruzione e tre profili dell'heap, riutilizzando la build di base appena prodotta dal percorso completo. Questa intera modalità estesa non è stata rieseguita nel presente blocco; sono stati eseguiti i 12 gate host e i tre profili heap sopra elencati.

## Limiti e prossimo blocco

È un allocatore correctness-first a scansione lineare e lock globale, non hardened, non async-signal-safe, non un protocollo fork/interposition; invalid/double free restano fuori dal contratto C (il controllo dei metadati può trapparli, non garantisce sicurezza contro input arbitrari). Restano frammentazione, benchmark/limiti sotto pressione e politiche di ritenzione. S_VM consente 32 regioni, massimo 64 MiB per regione e arena di 240 MiB: il fabbisogno del motore reale resta da misurare, non viene nascosto da questi test.

Prossimi passi: completare le API libc effettivamente richieste da WTF, il runtime C++ e l'adattamento pthread/atomiche/clock; poi ottenere un build reale di WTF/JavaScriptCore sul target, inizialmente senza JIT finché non esiste una politica EXEC/W^X. In seguito WebCore, rete HTTPS con validazione, font/grafica/event loop, UI Safari identica e acceptance HTML/CSS/JS/sicurezza/temi. Checkout upstream e toolchain grandi devono stare esterni; sorgenti authored e ricevute compatte su GitHub. Nessun cambiamento ai gate di completezza del motore o dell'interfaccia finché non vengono realmente superati.
