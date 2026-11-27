# Testi e unità — interfaccia italiana

## Terminologia

Usare i nomi italiani dove descrivono realmente la funzione: **Impostazioni di Sistema**, **Schermi**, **Privacy e sicurezza**, **Riduci trasparenza**, **Riduci movimento**, **Monitoraggio Attività**, **Memoria utilizzata**, **S.O.S.**, **Vista rapida**, **Apri**, **Salva**, **Salva col nome**, **Riduci a icona**, **Blocca schermo**.

Nel setup il nome richiesto è il **Nome account**: non chiamarlo “Nome completo”, perché identifica anche la cartella utente. La schermata è “Crea un account computer”. Il campo password mantiene il requisito minimo reale. La sessione temporanea e il rischio di perdere dati al riavvio devono restare riconoscibili.

Non copiare promesse di funzionalità assenti: il prodotto resta **tinymacOS**, non macOS. La Recovery conserva “Ripristina da una copia locale”: gli snapshot sullo stesso disco non vanno presentati come compatibilità con backup Time Machine esterni. Restano visibili le limitazioni della cache, del campionamento CPU, del firmware, della cifratura e delle operazioni distruttive.

Rimuovere istruzioni duplicate o stati privi di informazione (“Pronto”, spiegazioni ripetute dell’orologio, dell’account persistente o della posizione del Dock), non gli errori, le conferme o gli avvisi di sicurezza. Le etichette non devono suggerire pulsanti o operazioni inesistenti: la finestra di chiusura di TextEdit propone Annulla/Non salvare, non un salvataggio implicito.

I `CFBundleDisplayName` delle app native sono italiani nei tre volumi generati (live, disco e Recovery). Nomi degli eseguibili, cartelle `.app`, identificatori, comandi e alias inglesi rimangono stabili. Non modificare il browser o i messaggi dei componenti di terze parti per una semplice revisione dei testi nativi.

## Capacità

La funzione comune `include/display_size.h` usa **SI decimale**:

- 1 kB = 1.000 byte;
- 1 MB = 1.000.000 byte;
- 1 GB = 1.000.000.000 byte;
- 1 TB = 1.000.000.000.000 byte.

Seleziona automaticamente B/kB/MB/GB/TB/PB/EB; sopra B mostra una cifra decimale, troncata senza anticipare la soglia successiva. Il separatore dell’output comune resta il punto; l’immissione delle dimensioni MB accetta punto e virgola come già previsto dal parser. Non cambiare la geometria o moltiplicare per 1024 per poi apporre “MB”.

Esempi: 999.999.999 byte → `999.9 MB`; 1.000.000.000 → `1.0 GB`; 1.000.000.000.000 → `1.0 TB`.

I limiti binari dei backend restano invariati; gli errori rivolti all’utente ne riportano l’equivalente SI esatto:

- disco: 17.592.186.044.416 byte → `17.592186044416 TB`;
- ridimensionamento FAT32/NTFS: 2.199.023.255.040 byte → `2.19902325504 TB`.

LBA, offset nell’ispettore esadecimale, `wc -c`, hash, JSON di diagnostica e altri formati macchina conservano i conteggi esatti in byte/settori. MiB/GiB/TiB restano legittimi nei commenti tecnici che descrivono pagine e allineamenti reali, non nelle stringhe di capacità dell’interfaccia.

## Controlli

```sh
bash tests/test-localization6.sh
bash tests/test-display-size.sh
python3 tools/audit-ui-strings.py --check
TMC_BUILD=/percorso/build python3 tests/localization6_bundle_check.py
```

Il registro revisionato è `docs/audit-10.3/localization-6-changes.json`. L’inventario statico non sostituisce un controllo grafico: dialoghi, effetti traslucidi e testi dinamici richiedono anche schermate e input reali. Le verifiche del blocco 6 sono in `workspace/local-10.3/macroblocco-6/VERIFICA.md`, dalla radice del workspace.

Riferimenti terminologici consultati:

- [Supporto Apple: Monitoraggio Attività](https://support.apple.com/it-it/guide/activity-monitor/welcome/mac)
- [Supporto Apple: Utenti e Gruppi](https://support.apple.com/it-it/guide/mac-help/mtusr001/mac)
- [Supporto Apple: macOS Recovery](https://support.apple.com/it-it/guide/mac-help/mchl82829c17/11.0/mac/11.0)

L’adozione della terminologia non implica equivalenza funzionale o certificazione Apple.
