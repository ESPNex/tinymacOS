**Build 21B5 · Levante · avvio UEFI.**

## Novità
- VM multi-CPU: coordinamento delle mutazioni, invalidazione TLB e attesa degli ACK prima di liberare le pagine.
- Verbose nativo con V all'avvio e comandi `bootverbose` per stato, log e preferenza persistente sul sistema installato.
- Identità di versione aggiornata nelle applicazioni e nel Recupero.

## Verifiche e limiti
Suite backend/runtime di 15 verifiche, stress a 4/8 vCPU, installazione su SATA virtuale, Recovery e persistenza del verbose. Questa revisione della ISO e del disco installato è **solo UEFI**; il bootstrap BIOS era ancora un supporto QA separato. Nessuna verifica hardware implicita e nessun Safari/WebKit incluso.

Asset: `tinymacOS-2.4.4-x86_64.iso` e `SHA256SUMS.txt`.
