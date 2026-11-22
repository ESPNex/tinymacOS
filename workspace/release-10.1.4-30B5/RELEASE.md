**Build 30B5 · controller USB e icone SVG.**

## Cambiamenti
- Revisione rispetto a Linux v6.12, senza importarne codice: pubblicazione dei TRB e completamenti Short Data/Status xHCI corretti.
- EHCI: overlay modificati a schedule ferma e toggle interrupt mantenuto dall'hardware.
- Decodifica Report preferita per i descriptor validati, con test negativi sul codice 10.1.3. Questa preferenza viene ritirata nella 10.1.5.
- Tredici SVG originali per il Centro di Controllo, compilati in maschere alpha e presenti nel filesystem; vetro e comandi deduplicati conservati.

## Verifiche e riscontro fisico
Doppia build identica, 62 ricevute QA, contratti dei controller, matrice input/Recovery e regressioni kernel.

**Regressione fisica confermata:** sulla ASRock Q1900M/J1900 il mouse è fermo sia al login sia in Recovery. Questa release non risolve il problema, nonostante i test QEMU superati.

Asset: `tinymacOS-10.1.4-x86_64.iso` e `SHA256SUMS.txt`.
