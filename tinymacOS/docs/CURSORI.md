# Cursori e finestre — macroblocco 7

## Risorse reali e formati

Il tema deriva dal `Cursors.zip` fornito dall’utente. `tools/cursor-assets.py` importa esclusivamente i nomi previsti e conserva senza ridisegno i DIB originali da 32 px, gli hotspot, i frame e i metadati temporali ANI. Rimuove soltanto le rappresentazioni di dimensioni non utilizzate. Il manifest conserva gli SHA-256 dell’archivio, dei file originali e dei file importati.

I 15 `.cur` e i due `.ani`, insieme al manifest, sono presenti in `/System/Library/Cursors/` nelle immagini live, installata e Recovery. Non sono solamente incorporati nell’eseguibile. Il controllo `tests/cursor7_bundle_check.py` legge i tre volumi sigillati prodotti dalla build e confronta tutti i byte.

Il decoder nativo `include/cursor_format.h` supporta:

- CUR con DIB non compressi a 1/4/8/24/32 bit; palette, alpha, maschera AND e inversione bianco/nero; hotspot e limiti verificati;
- preferenza per 32 px, con campionamento delle rappresentazioni DIB fino a 128 px;
- ANI RIFF ACON con frame CUR, `anih`, sequenza e tempi individuali opzionali;
- massimo 64 frame e 256 passi, senza allocazioni guidate da dimensioni non verificate;
- tempi ANI a 60 Hz calcolati dal clock OS a 100 Hz, senza accumulare errori per frame;
- primo frame statico con Riduci movimento.

CUR con sole immagini PNG o compressioni DIB non supportate vengono rifiutati esplicitamente. Non si dichiara la decodifica universale di ogni estensione dei formati Windows. Risorse assenti o invalide mantengono una freccia di emergenza.

## Accesso prima del login: nessuna apertura dei dati utente

Il blocco di autenticazione di `S_OPEN`, `S_READ` e delle altre operazioni di sessione resta **byte per byte invariato**. Non è stato rimosso per far funzionare il tema nel setup.

WindowServer legge invece le risorse sigillate tramite l’esistente `S_ART`, già utilizzata per font e sfondi. Sono aggiunti solamente identificativi numerici appartenenti a una lista fissa di nomi CUR/ANI. Il chiamante deve essere il desktop; non può fornire un percorso arbitrario. Il kernel risolve file reali nel namespace di boot in sola lettura e verifica offset, lunghezza e destinazione utente. Il manifest e le risorse restano esplorabili con le normali operazioni filesystem dopo l’autenticazione e in Recovery.

Il caricamento avviene una volta; il movimento del puntatore non effettua letture di file né richieste IPC per l’hover.

## Contesti del puntatore

`C_CURSOR` descrive una regione del client e un ruolo del puntatore nella stessa transazione Draw che pubblica l’interfaccia. WindowServer risolve il contesto rispettando finestre sovrapposte, finestre ridotte, dialoghi, overlay e cattura durante il trascinamento.

Gli eventi locali enter/leave/update cambiano il puntatore senza obbligare l’app a ridisegnarsi a ogni movimento. Sono gestiti campi del setup/login, campi UI condivisi, editor, pulsanti, Terminale, bordi/angoli e pagina di installazione in corso. Le risorse per ruoli senza una funzione applicabile restano disponibili; non aggiungono funzionalità simulate.

## Movimento e resize

- Resize su quattro bordi e quattro angoli, con minimi e limiti dello schermo; il bordo opposto resta fermo.
- Un’ultima geometria per finestra e per frame. Invii falliti per coda piena vengono riprovati anche dopo il rilascio.
- Nel kernel vengono accorpati soltanto resize adiacenti. Non si saltano eventi di tastiera, clic o azioni interposte.
- Durante il movimento si riutilizza la superficie della finestra anziché ricalcolare il vetro per ogni rapporto del mouse.
- Durante il resize si riutilizza la superficie finché non arriva una nuova transazione dell’app. Dopo il rilascio il desktop ridisegna il vero backdrop Liquid Glass.

I test con QEMU possono verificare posizioni/frame intermedi e geometria effettiva; non costituiscono certificazione di un framerate su GPU o hardware fisico.

## Font proporzionale

TextEdit, Note e Memo disegnano il documento con `F_BODY`, lo stesso font proporzionale di sistema già fornito, senza sostituire l’atlante o usare un font esterno. Wrapping, caret, clic e navigazione verticale misurano gli avanzamenti reali TFN2.

`C_BODY` trasporta riferimenti compatti al repertorio di glifi, ma il renderer usa gli avanzamenti proporzionali, non celle da 8 px. Il limite condiviso delle transazioni passa da 144 a **256 comandi**, mantenendo invariata la struttura Draw e gli identificativi esistenti. Questo evita troncamenti anche in finestre alte/larghe piene di glifi stretti come `i` o `Ì`; non aumenta il numero di comandi inviati dalle interfacce ordinarie. I test completi includono questi casi limite.

Terminale, campi tecnici e password restano distinti; il rendering delle password continua a serializzare solo maschere, non il testo segreto.
