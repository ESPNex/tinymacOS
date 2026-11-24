# tinymacOS 10.1.9 Tramontana · build 30B10

## Login
- Cursore di inserimento lampeggiante nei campi nome, password e conferma, anche vuoti.
- Frecce sinistra/destra, Home/End, Backspace e Canc lavorano sui confini UTF-8.
- Le password restano mascherate; il cursore segue i pallini senza mostrare il testo.

## Pannelli Apri e Salva
- TextEdit usa pannelli modali navigabili al posto della richiesta di digitare un percorso.
- Preferiti, cartella corrente, elenco reale di cartelle/file, selezione con mouse o tastiera, doppio clic, scorrimento e risalita alla cartella superiore.
- Campo percorso (Ctrl+L), campo nome modificabile e nuova cartella reale.
- Il primo Salva e Salva come mostrano il pannello; i salvataggi successivi usano il documento corrente.
- Conferma esplicita prima di sovrascrivere; Annulla/Esc non cambiano il documento o il file esistente.
- Cartelle inesistenti, componenti di percorso troppo lunghi, destinazioni in sola lettura e documenti oltre 8192 byte vengono rifiutati; errori di scrittura lasciano aperto il pannello.
- Finder e pannelli condividono posizioni e accesso al filesystem. Mostra nel Finder apre una finestra sulla medesima cartella; le modifiche del filesystem aggiornano l'elenco.
- Il componente `userspace/filepanel.*` e riutilizzabile. Non e un'implementazione Cocoa/NSOpenPanel e non aggiunge supporto a formati non gestiti dalle applicazioni.

## Continuita e limiti
- Font della precedente ISO mantenuto, senza usare il riferimento 0bd59h.zip.
- Icone SVG nelle Impostazioni, menu compatto, wordmark unito e card Recovery Regular preservati.
- ISO ottica BIOS/UEFI, non USB-DD ibrida. Cambio risoluzione Intel persistente al riavvio su disco installato; browser WebKit non incluso.
- Test in QEMU e test host non certificano hardware fisico. La verifica ASRock Q1900M/J1900 resta all'utente.
- Il profilo font esterno e nel pacchetto sorgenti corrispondente; la repository mantiene gli asset standard. La dichiarazione dell'utente sulla base Inter modificata non e una verifica indipendente della licenza.
