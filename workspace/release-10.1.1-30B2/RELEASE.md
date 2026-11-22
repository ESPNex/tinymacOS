**Build 30B2 · manutenzione Recovery e input.**

## Correzioni
- Titolo Recovery centrato con la geometria delle finestre desktop.
- Un trasporto USB diretto inattivo non esclude più i puntatori firmware.
- Tentativo di routing USB2 Intel soltanto in assenza di interfacce HID native configurate, senza resettare dispositivi già enumerati.

## Verifiche e riscontro fisico
Doppia build identica, contratti ASan/UBSan, installazione BIOS, crossboot UEFI, ripristino locale e regressioni kernel/runtime.

**Riscontro successivo dell'utente:** mouse non funzionante né al login né in Recovery. I PASS virtuali non costituiscono una soluzione su quel PC.

Asset: `tinymacOS-10.1.1-x86_64.iso` e `SHA256SUMS.txt`.
