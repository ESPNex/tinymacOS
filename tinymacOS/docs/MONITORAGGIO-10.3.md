# Monitor CPU e memoria — contratto M3

Queste metriche provengono dal kernel, non da percentuali simulate. Sono comunque **campioni timer**, non contatori di cicli, frequenza, istruzioni, consumo elettrico o benchmark hardware.

## CPU: origine e sincronizzazione

1. Ogni interrupt LAPIC vettore 64 chiama `cpu_account_tick()` con `sched_lock` acquisito. In modalità UP senza APIC lo stesso percorso è chiamato dal PIT, vettore 32. Il PIT resta anche l'orologio di sistema; con APIC attivo non duplica l'accounting CPU.
2. Il core incrementa `core_ticks` e il clock aggregato `cpu_samples`. Se il task corrente **non è il vero idle di quel core**, incrementa anche `core_busy`, `busy_ticks` e `Process.cpu_ticks`.
3. L'affinità `pin` non indica idle: il worker USB fissato al BSP è lavoro kernel e viene incluso. Anche il codice kernel eseguito per un processo viene attribuito al contesto interrotto. Il tempo di attesa con `hlt` nel vero idle non è occupazione di un processo.
4. Aggiornamento e lettura di tutti i contatori CPU avvengono sotto lo stesso lock scheduler. Più thread dello stesso processo su CPU diverse non possono perdere incrementi. `S_INFO` esporta la maschera effettiva degli slot online: un AP che non parte non rende validi slot contigui inesistenti.
5. `S_PS` legge `cpu_ticks` insieme a `sample_ticks = cpu_samples` sotto il lock. Questo secondo clock permette al Monitor di usare finestre processo coerenti senza assumere che due syscall separate avvengano nello stesso istante. Lo stato di un processo è pronto se almeno un suo thread è pronto, non lo stato arbitrario dell'ultimo thread visitato.

Il timer nominale è configurato per circa 10 ms. Periodi effettivi, interrupt ritardati/coalescenti, sezioni con IRQ disabilitati, quanti dello scheduler e virtualizzazione possono introdurre bias. Non convertire i tick di un processo in secondi esatti dividendo ciecamente per 100.

### Formule del Monitor

- Per core: `100 × Δcore_busy / Δcore_ticks`.
- Totale normalizzato: `100 × somma(Δcore_busy) / somma(Δcore_ticks)` sugli slot online. Un core pieno su quattro pesa circa 25%, non 100% del totale.
- Processo: `100 × Δcpu_ticks / Δsample_ticks`, sul totale di tutti i core, non percentuale per singolo core. L'ordinamento CPU usa questo intervallo, non il contatore cumulativo della vita del processo.
- La percentuale intera è troncata, senza moltiplicare contatori a 64 bit per 100 con rischio di overflow.

Primo campione, cambio maschera CPU, reset/wrap o finestra inconsistente azzerano la lettura e ristabiliscono una baseline: non pubblicano un picco ottenuto da underflow. Le somme sono limitate per gli otto core esportati. I PID nuovi non ereditano la CPU di un processo uscito. Una finestra senza avanzamento non aggiunge punti alla cronologia.

Il Monitor aggiorna circa ogni 80 tick PIT; la cronologia conserva 42 intervalli. **Pausa** congela i campioni periodici; cambiare ordinamento non campiona. Riprendi acquisisce una nuova baseline, senza attribuire tutto il tempo in pausa al primo intervallo. Sono visibili fino a otto barre, inclusi slot online non contigui. `S_INFO.ticks` serve alla cadenza UI, non al denominatore della percentuale multicore.

## RAM: categorie e limiti

| Campo / testo | Origine e significato |
|---|---|
| `SysInfo.total` / gestita | Pagine UEFI ConventionalMemory rese allocabili da `mem_init()`, sopra 1 MiB e sotto il limite dell'identity map/bitmap, 64 GiB. Pool fisico del page allocator, non RAM virtuale e non DIMM installati. |
| `SysInfo.free` / libera | Pagine attualmente libere nello stesso pool, lette sotto `kmem_lock`. |
| In uso | `total - free`, con protezione UI contro dati incoerenti; include backing di processi, tabelle e altre allocazioni dal pool. |
| `ram_bytes` | RAM descritta dalla mappa UEFI: tipi LoaderCode/Data, BootServicesCode/Data, RuntimeServicesCode/Data, ConventionalMemory, ACPIReclaim e ACPINVS. Non include MMIO, buchi riservati non classificabili o memoria persistente. Non è una lettura SPD/SMBIOS della RAM fisicamente installata. |
| `reserved_bytes` / riservata/non gestita UEFI | `ram_bytes - total`. Comprende loader/kernel/immagini/strutture trattenute dal boot, firmware e RAM non affidata all'allocatore. Non viene dichiarata libera. |
| `user_bytes` / utente | Backing fisico delle foglie delle page table utente, contato in `vm_add`, `vm_unmap` e teardown. Include immagini, stack e memoria impegnata dei processi; esclude semplici prenotazioni virtuali e page table. |
| RAM UTENTE per processo | Passeggiata delle page table di quel processo sotto `kmem_lock`, escluso il mapping kernel condiviso. È backing di pagine utente: non PSS, memoria virtuale prenotata, consumo totale del processo compreso kernel o contabilità POSIX completa. |
| Altro pool | In uso meno backing utente; comprende page table e altre pagine di sistema del pool. Non comprende il kernel statico e l'heap statico già trattenuti nella categoria riservata. |
| Cache | **Non separata**: può risiedere in backing di sistema, heap statico o buffer del processo. Non viene inventato un valore di cache né sommata una categoria sovrapposta. |

Lo snapshot `total/free/user` è atomico rispetto all'allocatore. Il Monitor non pretende che uno snapshot memoria e uno scheduler rappresentino un singolo istante globale: sono letture serializzate dei rispettivi sottosistemi. `S_PS` mantiene l'ordine di lock scheduler → memoria già usato dalla creazione/rimozione processi; la memoria non acquisisce il lock scheduler. Le attese IRQ-off continuano a servire i mailbox TLB.

`S_FREE` e decommit/release restituiscono le foglie fisiche; page table ormai vuote possono rimanere fino al teardown del processo. Non vanno confuse con una perdita di tutti i MiB appena rilasciati. Il teardown libera anche le tabelle. L'heap kernel coalescente statico non viene conteggiato una seconda volta come consumo del page allocator.

## Diagnostica riproducibile

`sysstat` è un comando nativo ordinario, soggetto alle syscall e ai permessi esistenti, non un servizio QA privilegiato o un'iniezione di contatori.

```sh
sysstat
sysstat test > Documents/m3-result.jsonl
```

Il primo comando produce uno snapshot JSON; `test` produce 13 righe con fasi idle, un worker attivo, un worker per CPU online, sleep dei worker, alloc/free di 8 MiB e commit/release VM di 4 MiB. Il picco delle due allocazioni non è simultaneo. Durante la prova il sistema viene intenzionalmente caricato; non è un test di sospensione hardware S3/S4.

Ogni riga contiene:

- `wall`: clock PIT; `busy`: contatore aggregato occupato; `mask`: bitmap CPU online;
- `cores`: otto coppie `[core_ticks, core_busy]`;
- `mem`: `[pool_totale, pool_libero, RAM_UEFI, riservata, backing_utente]`, tutti in byte;
- `self`: `[PID, clock_campioni_aggregato, tick_CPU_processo, backing_utente_processo, thread]`.

I valori sono grezzi: la verifica indipendente calcola le differenze. I test guest eseguono lavoro reale e salvano il risultato nella live, poi lo importano tramite il vero Installer in FS2; un lettore host **soltanto in lettura**, dopo spegnimento QEMU, verifica FS2 ed estrae il file. Il cold boot nativo senza ISO ricontrolla il file e il Monitor. Le prove non modificano offline il report per farlo passare e non chiamano un falso `S_INFO`.

Host: `bash tinymacOS/tests/test-m3-acceptance.sh`. Guest: `tests/m3_telemetry_guest.py`, ISO in `TMC_STAGE_ISO`, directory risultati in `TMC_QA_DIR`, `TMC_M3_CPUS=1|4|8`; per il fallback PIC/PIT usare `TMC_M3_CPUS=1 TMC_M3_TIMER=pit` (QEMU `-cpu max,-apic`). Tutte le fixture sono temporanee sotto `/var/tmp/tmc-m3-guest-*`, mai dischi fisici/utente.

Le nuove strutture ABI richiedono la ricompilazione coordinata di kernel e app native; non si promette l'esecuzione di vecchi binari su un kernel di sviluppo nuovo. Overlay e header ABI sono sincronizzati, senza modificare Safari o il percorso USB canonico. Esiti, hash della ISO finale e limitazioni del collaudo sono conservati in `../../workspace/local-10.3/m3/VERIFICA.md`.
