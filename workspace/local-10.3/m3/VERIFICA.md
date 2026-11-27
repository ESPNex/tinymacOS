# M3 — monitoraggio CPU e memoria: chiusura completa

**M3 completo rispetto a tutti i criteri locali del piano 10.3.** Audit dei produttori, correzioni kernel/SMP, Monitor, diagnostica, suite host normale/sanitizzata e matrice guest reale conclusi. M0/M1/M2 ricollaudati; M4 non avviato. Nessuna certificazione hardware, sospensione S3/S4 o release 10.3.

ISO finale comune a tutti i collaudi guest: **`1e12b7a6f740b67c956d89588a341245b0e22be72a08a244c1f6f49fd4e15132`**, 227647488 byte, `/var/tmp/tmc-10.3/m3-final-build/m3.iso` (sviluppo, non pubblicata come release). Hash degli artefatti in `artifacts.json`, sorgenti in `source-manifest.json`. Le ricevute sono persistenti; la ISO e i dischi virtuali in `/var/tmp` possono scomparire.

## Problemi reali risolti

1. **Affinità confusa con idle:** `pin < 0` escludeva anche il worker USB pinned. Ora si esclude soltanto lo slot idle effettivo del core; il lavoro kernel è incluso.
2. **Incrementi SMP persi:** thread dello stesso processo incrementavano `cpu_ticks` fuori dal lock. Accounting e snapshot usano `sched_lock`, incluso il clock aggregato di campioni per finestre processo coerenti.
3. **Fallback senza metriche:** il PIT senza APIC schedulava ma non produceva gli stessi contatori. Ora usa il medesimo produttore; con APIC il PIT non duplica i campioni.
4. **Maschera CPU e UI:** esportata la bitmap online effettiva; baseline resettata ai cambiamenti, coperti AP non contigui. Il Monitor mostra fino a otto core, non soltanto quattro.
5. **CPU processo cumulativa:** percentuale e ordinamento usano differenze fra campioni, normalizzate sul totale multicore. Un processo con un worker su otto non viene mostrato al 100% del totale.
6. **Memoria ambigua e non serializzata:** snapshot sotto lock del vero pool fisico; distinzione tra RAM descritta UEFI, riservata/non gestita, pool libero/in uso e backing utente. Conteggio per processo dalle page table, non una percentuale inventata della RAM.
7. **Underflow e pausa:** barra RAM protetta anche con input incoerente; reset/wrap/no-progress non creano picchi. Ordinare in pausa non campiona, ripresa con nuova baseline. Processo pronto se almeno un thread è pronto.

Contratto dettagliato, formule, ordine dei lock, campi grezzi e limiti: [`../../../tinymacOS/docs/MONITORAGGIO-10.3.md`](../../../tinymacOS/docs/MONITORAGGIO-10.3.md).

## Accettazione M3: nessun criterio rinviato

| Criterio | Prova | Esito |
|---|---|---|
| Origine total/free, core_ticks/core_busy, tick processo e SMP | Audit di memory.c, sched.c, cpu.c, smp.c, syscall.c; gate produttori/lock | PASS |
| Idle e kernel pinned | Vera funzione timer, idle escluso e worker kernel pinned incluso; guest idle reale | PASS host normale + ASan/UBSan e guest |
| Incrementi condivisi SMP | Otto pthread host chiamano la funzione reale con lock e accumulano esattamente 240000 tick nello stesso processo; guest con thread reali su 1/4/8 core | PASS |
| Carico su un core | Un worker reale, altri dormienti; percentuale processo e denominatore multicore letti dal kernel | PASS APIC 1/4/8 core e PIT UP |
| Carico multicore | Un worker per core online, nessun finto S_INFO o contatore preimpostato nel guest | PASS APIC 1/4/8 core e PIT UP |
| Sleep | Worker in S_SLEEP, ritorno della CPU processo quasi a zero | PASS in tutti e quattro i guest |
| Allocazione/rilascio fisici | S_ALLOC 8 MiB e VM commit/release 4 MiB reali; delta esatto del backing globale e per processo, crescita/diminuzione del pool libero | PASS in tutti e quattro i guest |
| Teardown, fallimento e pagine residue | Vero allocator/VM host: duplicate/absent map, unmap, teardown, dieci cicli senza leak, esaurimento memoria e liberazione completa | PASS normale + ASan/UBSan |
| Contatori e overflow | UINT64_MAX, wrap/reset, busy incoerente, intervallo zero, limite delle somme a otto core; nessun underflow o picco artificiale | PASS normale + ASan/UBSan |
| Monitor vero, non solo helper | sample/paint/serializer reali, CPU processo e sort, pausa/rebaseline, 42 campioni, otto core e maschera sparsa, barra RAM a 660/820/1280 px | PASS normale + ASan/UBSan |
| GUI guest | Barre per tutti i core richiesti, etichette memoria, pausa/sort con carta CPU pixel-identica, ripresa | PASS su quattro configurazioni |
| Persistenza/cold boot | Report scritto dal guest, importato dalla GUI in FS2, estratto indipendentemente in sola lettura; successivo cold boot nativo senza ISO con report e Monitor funzionanti | PASS su quattro configurazioni |

## Risultati osservati sulla ISO finale

Percentuale **del processo diagnostico sul totale**, non frequenza, cicli o benchmark di prestazioni del PC. La tabella deriva dai file grezzi JSONL conservati qui.

| Configurazione | Idle | Un worker | Worker multicore | Sleep |
|---|---:|---:|---:|---:|
| APIC · 1 core | 0,00% | 98,98% | 97,46% | 0,00% |
| APIC · 4 core | 0,00% | 25,00% | 98,73% | 0,00% |
| APIC · 8 core | 0,00% | 12,55% | 87,38% | 0,00% |
| PIC/PIT · 1 core | 0,62% | 98,50% | 99,00% | 0,00% |

Il totale di sistema multicore è rispettivamente 100,00%, 100,00%, 88,34%, 100,00%; include gli altri processi/kernel. **Non è stato arrotondato il risultato a 100% per farlo sembrare ideale.** Otto worker sotto QEMU/TCG e scheduler reale non garantiscono un 100% osservato in ogni intervallo. I test verificano aumento sotto carico, corretto denominatore, calo dopo sleep e coerenza dei contatori, con soglie dichiarate nel test; non un benchmark hardware.

In tutte le configurazioni:

- allocazione 8 MiB: `user_bytes` e backing del processo aumentano esattamente di **8388608 byte**;
- free: entrambe le categorie ritornano alla baseline;
- commit VM: aumento esatto di **4194304 byte**; release ripristina tutto il backing utente;
- rimangono soltanto poche page table vuote, entro 16 pagine nella prova, fino all'uscita del processo; il teardown host dimostra il loro recupero;
- `0 ≤ user ≤ total - free ≤ total ≤ ram_UEFI`, riservata = RAM_UEFI − pool; busy aggregato = somma dei contatori core;
- contatori monotoni, campioni per core e clock aggregato processo coerenti; nessuna sottrazione unsigned usata per nascondere un reset.

## Regressioni chiuse prima del commit

- **Foundation finale**, comprendente M3, M1 normale/sanitizzato, paint e compilazioni freestanding: PASS.
- **Intera suite M2 host**, 13 famiglie normali/sanitizzate, inclusi FS2 install/runtime/import/conversione, GPT grande, 4Kn, FAT/NTFS/RAW, resize, NVMe: PASS, log `regression-m2-host.log` concluso con `PASS M2 host gates`.
- **M1 guest completo sulla ISO M3 finale:** setup/Terminale/TextEdit/Note/Memo/Installer/Recovery, file UTF-8 da 144000 e 141735 byte salvati/riaperti con SHA-256 esatto: PASS.
- **FV003 realmente creato dalla GUI:** cold Recovery senza ISO, no-echo, password errata rifiutata e corretta accettata, verifica FileVault indipendente: PASS.
- **M2 guest 3 TiB NVMe 4Kn:** gap reale 2,5 TiB, installazione GUI, native e Recovery cold senza ISO, confronto Installer/Utility/Terminale e byte delle partizioni estranee sintetiche preservati: PASS. Non sostituisce la precedente matrice completa M2; la ricollauda sul caso più vincolante della nuova build.
- **M0 workspace, 568 input protetti, font TFN/panic ricostruiti dai font forniti, USB canonico e overlay/ABI sincronizzati:** PASS.

## Provenienza e limiti delle prove

Le quattro esecuzioni finali sono fresche e complete: `m3-1-apic-result.json`, `m3-4-apic-result.json`, `m3-8-apic-result.json`, `m3-1-pit-result.json`, tutte con `passed: true` e il medesimo hash ISO. I relativi `*-raw.jsonl` sono l'output del vero comando nativo `sysstat test`, **non fixture di contatori**. I test host usano invece fixture controllate per reset/overflow, più le vere funzioni timer/allocator/paint: i due livelli sono dichiarati separatamente.

Il carico è eseguito nella live; il report viene scritto dal guest, importato dal vero Installer in FS2, letto dal verificatore host O_RDONLY solo dopo spegnimento e ricontrollato al cold boot nativo insieme al Monitor. Non si dichiara una seconda intera misura di carico nel kernel installato: è una distinta prova di avvio/UI/persistenza. Non si richiede tale duplicazione per chiudere il criterio di carico M3, che è eseguito integralmente su kernel reale guest in tutte le configurazioni.

Il fallback PIT usa QEMU `-cpu max,-apic`; i suoi intervalli mostrano esattamente un campione CPU per tick PIT. Gli screenshot sono ritagli PNG lossless delle vere finestre; coordinate e hash originali in `screenshots.json`. I log seriali firmware sono normalizzati CRLF → LF e senza whitespace finale; non vengono usati al posto dei report numerici per dimostrare la CPU.

Una prima build/accettazione a quattro core ha verificato la direzione delle correzioni. Sono poi stati aggiunti stato processo aggregato, etichetta «altro pool» e diagnostica busy, mantenuta la stringa M0 «Campioni per intervallo», ricostruita la ISO e ripetuta tutta la matrice finale. Due gate M0 intermedi hanno segnalato correttamente una stringa rimossa e l'overlay non aggiornato: corretti prima della build finale. Non sono PASS retroattivi. Dopo la build finale sono cambiate solo prove host e documentazione, non sorgenti funzionali.

Nessuna prova fisica, nessuna misura GPU/frequenza, nessuna RAM DIMM/SMBIOS certificata, nessuna separazione fittizia della cache, nessuna sospensione hardware S3/S4. ABI nativa aggiornata e ricompilata in modo coordinato. Safari, font, USB canonico, DnD e partizioni estranee preservati. Nessun tag/version bump/release; M4–M7 rimangono distinti.

## Riproduzione

```sh
export PATH=/usr/sbin:$PATH
bash tinymacOS/tests/test-m3-acceptance.sh
bash tinymacOS/tests/test-10.3-foundation.sh
export TMC_BUILD=/var/tmp/tmc-m3-build TMC_ISO=m3.iso
bash tinymacOS/build.sh
bash tinymacOS/tests/test-m2-acceptance.sh
export TMC_STAGE_ISO=$TMC_BUILD/m3.iso TMC_QA_DIR=/var/tmp/tmc-m3-qa
for n in 1 4 8; do
  TMC_M3_CPUS=$n python3 -u tinymacOS/tests/m3_telemetry_guest.py
done
TMC_M3_CPUS=1 TMC_M3_TIMER=pit python3 -u tinymacOS/tests/m3_telemetry_guest.py
# Directory diversa per le ricevute delle regressioni:
export TMC_QA_DIR=/var/tmp/tmc-m3-regression
python3 -u tinymacOS/tests/m1_acceptance_guest.py
python3 -u tinymacOS/tests/m1_terminal_vault_guest.py
TMC_M2_STORAGE=nvme TMC_M2_LOGICAL=4096 python3 -u tinymacOS/tests/m2_capacity_guest.py
python3 tinymacOS/tests/test-m0.py --workspace
python3 tinymacOS/tests/test-font-no-fallback.py
```

Tutte le prove usano fixture temporanee, mai dischi fisici/utente. Log e ricevute qui conservati sono il risultato delle esecuzioni, non soltanto comandi proposti.
