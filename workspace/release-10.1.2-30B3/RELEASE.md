**Build 30B3 · routing Intel e protocollo HID.**

## Correzioni
- Routing USB2 ripristinato prima dell'enumerazione sulle famiglie Intel già supportate.
- GET_PROTOCOL controllato anche dopo SET_PROTOCOL riuscito, evitando di decodificare Boot come Report.
- Un AbsolutePointer firmware inattivo non maschera più il SimplePointer USB.
- Aggiunto Ps2KeyboardDxe ufficiale al percorso BIOS/OpenCore per picker e selezione Recovery.

## Verifiche e riscontro fisico
Doppia build identica, cinque controlli negativi sul codice 10.1.1, matrice input BIOS/UEFI, italiano, installazione/ripristino e 29 regressioni kernel.

**Riscontro successivo dell'utente:** mouse fermo al login; in Recovery piccolo movimento seguito da blocco. Centro di Controllo 10.1 conservato.

Asset: `tinymacOS-10.1.2-x86_64.iso` e `SHA256SUMS.txt`.
