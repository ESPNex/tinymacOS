# Tastiera italiana Windows — scorciatoie desktop

Mappatura del macroblocco 4 del piano fornito (`piano.pdf`). Nessun cambio di versione pubblica.

- **Windows = Command (⌘)**, con il tasto Windows sinistro o destro.
- **Ctrl = Control (⌃)**: non è un alias di Command.
- **Alt sinistro = Option (⌥)**; AltGr resta disponibile per i simboli della tastiera italiana.
- **Maiusc = Shift (⇧)**.

## Sistema

| Combinazione | Azione |
|---|---|
| Win+Spazio | Cerca applicazioni e documenti |
| Ctrl+↑ | Entra/esce da Mission Control; Esc esce |
| Win+Maiusc+A | Applicazioni |
| Win+Tab | Passa alla finestra precedente |
| Win+W / Win+Q | Chiude la finestra/app attiva nel modello a singola finestra per processo |
| Win+M | Riduce a icona la finestra attiva |
| Win+Ctrl+Q | Blocca la sessione |
| Win+Maiusc+N, con la Scrivania attiva | Nuova cartella, con finestra diretta della Scrivania, senza avviare Finder |

I precedenti tasti funzione per lanciare direttamente le applicazioni non sono più scorciatoie di sistema. Usa Spotlight, Dock o Applicazioni.

## Finder

| Combinazione | Azione |
|---|---|
| Win+N | Nuova finestra Finder |
| Win+Maiusc+N | Nuova cartella |
| Win+↑ | Cartella superiore |
| Win+↓ / Win+O | Apre l’elemento selezionato |
| Invio | Rinomina l’elemento selezionato |
| Spazio | Vista rapida |
| Win+I | Informazioni |
| Win+Backspace | Richiede l’eliminazione dell’elemento selezionato |
| Win+C / Win+X / Win+V | Copia / taglia / incolla |
| Win+Maiusc+H / D / O | Inizio / Scrivania / Documenti |
| Win+[ / Win+] | Cronologia indietro / avanti |

Backspace e Canc senza Command non eliminano più un elemento né sostituiscono la navigazione alla cartella superiore. La precedente scorciatoia per rinominare è sostituita da Invio.

## Testo e Terminale

Le scorciatoie delle applicazioni basate su Command, ad esempio Win+S in TextEdit e Win+C/V negli editor, non sono più attivate da Ctrl. Nei campi a riga singola: clic/tocco posiziona il cursore, frecce spostano per carattere UTF-8, Home/Fine e Win+←/→ raggiungono gli estremi quando gestiti dal campo; Win+A seleziona per la sostituzione.

Nel Terminale:

- **Ctrl+C interrompe** il comando; non copia.
- **Win+C / Win+X / Win+V** copiano, tagliano o incollano il testo.
- **Win+K** pulisce la schermata; **Win+N** apre un Terminale.
- **Win+←/→** raggiunge inizio/fine della riga di comando.
- Il clic sulla riga del prompt posiziona l’inserimento, anche in Recovery; il trascinamento sull’output conserva la selezione di sola lettura.
- I prompt senza eco non rivelano lunghezza, posizione interna, testo o contenuto degli appunti delle password.

## Avvio e Recovery

**Win+R durante il logo di avvio del loader UEFI** apre il Recupero nativo. R da solo, Ctrl+R e F12 non lo selezionano. La distinzione richiede il protocollo firmware *Text Input Ex*, con modificatori validi. Se il firmware non lo espone, usa la voce esplicita Recovery di OpenCore: non esiste un fallback ambiguo sul solo tasto R.

In Recovery **Ctrl+F2** apre il menu Utility, navigabile con frecce e Invio; Esc lo chiude. I campi dell’installer e di FileVault usano lo stesso cursore/clic della sessione normale.

## Scrivania e trascinamento

Le icone sono posizionabili liberamente, senza scatto obbligatorio a una griglia, entro lo schermo. La posizione è salvata per utente in `~/Library/Preferences/Desktop.layout`, associata a identità/nome dell’elemento e limitata ai bordi al cambio di risoluzione. Un file di preferenze incompleto o non valido è ignorato.

Il trascinamento distingue riposizionamento della stessa icona, spostamento in una cartella e trasferimento verso Finder. Nel Terminale un elemento rilasciato inserisce soltanto un percorso tra virgolette, senza eseguirlo. Le finestre restano sopra le icone; le icone restano sopra il widget della Scrivania.

Questa guida descrive il comportamento implementato. I risultati dei test locali e i relativi limiti sono nel rapporto del macroblocco; non costituisce certificazione su hardware fisico.
