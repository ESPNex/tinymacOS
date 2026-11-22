**Build 21B9 · Levante · tastiera italiana e input HID.**

## Novità
- Unica mappa italiana ISO condivisa tra USB e PS/2, con AltGr, accenti, euro e tasto `<>`.
- Inserimento e cancellazione UTF-8 nelle applicazioni e nei campi account; glifo euro incluso nei font.
- Recovery decodifica i tasti fisici HID boot/PS/2 senza dipendere dalla mappa Unicode del firmware.
- Parser mouse con offset distinti per Report ID e assi firmati fino a 16 bit; controlli separati per X e Y.

## Verifiche e riscontro fisico
27 profili input, 236 verifiche raggruppate, italiano nelle app e in Recovery, 29 regressioni kernel; doppia build identica e installazione BIOS/UEFI.

**Riscontro successivo dell'utente sulla 21B9:** mouse funzionante in Recovery, movimento soltanto verticale al login. Il problema hardware non è risolto.

Questa è la revisione finale della release sostituita in precedenza nello stesso tag; le ricevute 21B6–21B8 descrivono build diverse. La ISO è ottica BIOS/UEFI, non USB-DD.

Asset: `tinymacOS-2.4.5-x86_64.iso` e `SHA256SUMS.txt`.
