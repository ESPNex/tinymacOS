# SMP dopo il deploy 2.4.3

La ISO Drive resta immutata. I sorgenti esatti della manutenzione sono congelati in maintenance-2.4.3/sources/.

Primo blocco implementato nell'albero di sviluppo: mailbox TLB con pubblicazione/ack release-acquire, polling negli spinlock IRQ-off e attesa shoot_lock, handler senza lock; il sender disabilita migrazione/preemption durante il protocollo. Questo affronta il deadlock IPI/IRQ-off. È sviluppo, NON una release e NON abilita ancora S_VM su più CPU.

Restano da correggere e validare prima di togliere il guard S_VM:
- vm_unmap: detach PTE, flush locale/remoto, SOLO DOPO page_put;
- vm_access: shootdown sincrono dopo le modifiche protezioni;
- riuso page tables e vm_drop/task lifetime;
- concorrenza fra validazione IN/OUT e syscall che cambiano mapping, pin/copy uaccess e thread del processo;
- stress kernel/guest reale multi-caller su 2/4/8 CPU, protezioni/fault e regressioni.

Non confondere host pthread/mailbox con prova MMU hardware. Non considerare Safari pronto: servono ancora runtime, WTF/JSC, WebCore, TLS e UI collegata al motore.

## Verifiche del primo blocco

- Compilazione target arch/smp.c: PASS.
- Mailbox effettiva con callback invalidazione simulata: ASan/UBSan, 8 worker pthread, 10.001 richieste/80.008 ACK: PASS. Ultimo worker ritardato intenzionalmente: il sender non termina prima dell’ACK. Il fixture host cede la CPU per non saturare la quota del sandbox; prima esecuzione busy-spin fermata dopo timeout, non conteggiata come PASS.
- Suite backend/runtime ricostruita dopo la modifica SMP: **15/15 PASS**, inclusi boot guest rete e VM 1/2 vCPU. La VM a 2 CPU **continua a rifiutare** le mutazioni: regressione/liveness del primo blocco, NON supporto VM SMP completato.
- Nessun nuovo upload dopo 2.4.3. Safari non compilato.
