# 10.3 — pulizia e primo componente numerico WTF reale

## Stato verificato

**55 gate distinti chiusi con esito positivo fra la matrice iniziale e UN replay
host esplicito. Non è un 55/55 nella stessa invocazione:** la prima esecuzione ha
54 PASS e un timeout host FS2 a 180 secondi, conservato integralmente. Lo stesso
script FS2, senza modifiche, è poi terminato con exit 0 nelle varianti normale e
ASan/UBSan. Il runner ora concede 600 secondi soltanto agli host pesanti
FS2-install/native4K; workload e deadline native restano invariati.

| Gruppo | Gate distinti | Risultato |
|---|---:|---|
| Contratti host/compilazione, pulizia/FS/input | 29 | 28 subito + 1 replay FS2 |
| Build fresca base/lifecycle + 5 consumer | 6 | PASS |
| stdlib-dtoa/events/heap/C++/lifecycle/pthread × 1/4/8 CPU | 18 | PASS |
| Recovery nativa con R all'avvio × 1/4 CPU | 2 | PASS |

Il primo guest stdlib non ha richiesto correzioni dopo un fallimento nativo.
La nuova componente passa su **1/4/8 CPU**: 1.040 vettori decimali indipendenti
CPython, 8.000 roundtrip binary64 per profilo, 400 task `std::function` con cleanup,
128 verifiche control-word x87/SSE2 attraverso context switch, handoff e distruzione
cross-thread di vector/string, guard static locale e distruzione DSO, zero residui
heap. Cache 256 KiB esplicitamente opt-in e svuotata alla fine.

## Pulizia senza cancellare dipendenze attive

- 1.945 file/88.438.898 byte di vecchi workspace e implementazione Recovery EFI
  ritirata archiviati prima della cancellazione. Commit backup immutabile
  `6e325b0292eba6805412ae813267ca6d991ba6d5`, archivio 65.706.096 byte, SHA-256
  `b9cb0fee8c9d85d91fbe7d2323857f11af4cb21614db19fa401b2c16af7cfe77`.
  Riscaricato da GitHub e controllato OGNI membro prima di rimuovere il locale.
  Pulizia in `0c79183`; ricevuta `../cleanup/github-archive-verification.json`.
- `fs/btrfs.c` **conservato**: dispatcher/fallback attivo per live/Recovery. FS2,
  bootfs, installer e conversione/cifratura non sono unused.
- Baseline storica di 986 input riclassificata esplicitamente: **972 ancora
  identici**, 13 file Recovery EFI ritirati, un include Recovery nativa aggiornato.
  Sfondo condiviso trasferito byte-per-byte in `desktop/recovery_background.h`.
  Nuovo contratto protegge questi input; non si dichiara più «986 invariati».
- Regressioni FS2 core/runtime/install/import/conversione, cifratura vault2,
  adattatore installazione 4Kn e input/manutenzione: eseguite. Due boot Recovery
  UEFI reali via tasto R verificano pannello, assenza barra residua e Terminale.
  Non è una nuova certificazione installata BIOS/4Kn né hardware fisico.

## Integrazione tecnica reale, con confini espliciti

22 file upstream WebKit originali, incluse licenze, fissati a
`23a3d9001767ddc9a4b120b6d7db33ca21230ab8`, riscaricati indipendentemente e
ricontrollati. Port esterno generato con isolamento degli include di quattro
helper WTF e **due cast signed espliciti in strtod.cc**. Gli originali restano
identici; patch completa e manifest degli hash inclusi nelle prove.

Il test indipendente ha trovato un vero difetto nel pin: `0.1` diventava Infinity
per un bound unsigned length + esponente negativo (526 mismatch su 1.040 prima
della correzione). La suite riproduce ancora il caso originale errato, poi richiede
il valore binary64 corretto nel port. Nessun vettore eliminato o oracolo piegato.

Container/template **reali GCC 14/libstdc++**: vector/string/unique_ptr/function/
algorithm/span/array/atomic. Configurazione header **hosted dichiarata**, non
libstdc++ freestanding completa. Chiusura effettiva di **304 header** con versione
compiler e SHA-256, ricontrollata nei guest. Link target `ld -nostdlib`, nessuna
libreria binaria C++/glibc e nessun simbolo indefinito. Piccolo glue nativo per
failure policy fatal, ceil binary64, memchr/strnlen; lifecycle/heap/thread/eventi
forniti dai blocchi precedenti. Eccezioni/RTTI/AVX disabilitati; errori supportati
terminano in trap, non restituiscono falsi successi. I test fatal SIGILL sono host.

Vedi [stdlib/README.md](stdlib/README.md) per API escluse, ABI/toolchain/licenze,
semantica matematica e comandi. Il consumer è in un'immagine diagnostica usa e
getta: **non** nella ISO retail distribuita.

## Prove riproducibili e limiti

`test-results/gigablock/verification.json` e archivio **416.120 byte / 125 file**,
SHA-256 `0f33a2f27df32ea24459d85f5e8fd90bf9eb4fe94d9f4aff28ca269588aeba7f`.
L'archivio contiene matrice iniziale non riscritta, replay riuscito, primi errori
host, mismatch numerici originali, patch/upstream readback, input/header manifest,
log nativi, tre crop stdlib lossless e quattro screenshot completi Recovery.
Tutti i membri sono ricontrollati tramite SHA-256. Le altre schermate intere sono
solo scratch esterno, con hash/provenienza registrati, non promesse persistenti.

```sh
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=/var/tmp/tmc-python-release \
 TOOLROOT=/var/tmp/tmc-tools python3 workspace/webkit-tmc/tools/run-tests.py \
 --gigablock --output /var/tmp/tmc-gigablock-final/results/acceptance.json
```

**Non consegnati:** libstdc++/libc++ completi, WTF completo/RunLoop upstream,
JavaScriptCore, WebCore, HTTPS/TLS validato, renderer o UI Safari. Il componente
numerico non carica pagine né esegue JavaScript. Restano lavoro reale su libc/ABI,
WTF, JSC, WebCore e UI classica richiesta. Nessuna ISO/release/tag nuovo;
10.2.5 distribuita e relativo singolo asset immutati.
