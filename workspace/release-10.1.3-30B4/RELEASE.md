**Build 30B4 · ripresa input e Centro di Controllo.**

## Correzioni
- Negoziazione Boot-first e recupero asincrono dopo errori USB, con budget azzerato dal traffico valido.
- Recovery: acquisizione del solo figlio Boot-mouse e applicazione delle delte alla posizione corrente.
- Centro di Controllo compatto: rimossi il selettore Sfondo e l'accesso Impostazioni duplicati, mantenendo il vetro.

## Verifiche e riscontro fisico
Doppia build identica, controlli negativi sul codice 10.1.2, 27 profili input, 18 avvii Recovery e 29 regressioni kernel. Prove prolungate con inversioni X/Y e pausa/ripresa.

**Riscontro successivo dell'utente:** in Recovery il mouse si muove soltanto in verticale. Il problema fisico resta aperto.

Asset: `tinymacOS-10.1.3-x86_64.iso` e `SHA256SUMS.txt`.
