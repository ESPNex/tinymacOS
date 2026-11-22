**Build 21B5 · revisione installer OC2.**

## Novità
- Il disco installato avvia autonomamente anche in BIOS: ponte MBR → OpenDuet completo USB/AHCI → OpenCore → loader nativo.
- Lo stesso disco mantiene l'avvio UEFI, senza dipendere da una voce NVRAM precedente.
- Directory NVRAM preparata prima del sigillo; Recovery enumera i puntatori firmware disponibili.
- ABI TLS per-thread FS aggiunta come prerequisito runtime, non come implementazione HTTPS.

## Verifiche e limiti
Installazione grafica BIOS, due avvii dal solo HDD, crossboot UEFI, verbose persistente e ripristino offline con documenti invariati. Controllati sigilli, GPT e rifiuto di una destinazione non vuota. La live è ottica BIOS/UEFI, non USB-DD; nessuna promessa di compatibilità hardware universale.

Asset: `tinymacOS-2.4.4-x86_64.iso` e `SHA256SUMS.txt`.
