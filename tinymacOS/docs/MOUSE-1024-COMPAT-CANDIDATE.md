# Storico preparazione: rollback input/desktop 10.2.4

> Questo documento conserva lo stato della preparazione iniziale. La candidata è stata
> successivamente ricostruita come 30B19: vedere [note aggiornate](NOTE-10.2.5-30B19.md).
> Il mouse sul PC fisico resta da verificare.

Segnalazione confermata: USB con filo, porta diretta, puntatore fermo già nella
prima schermata account; 10.2.4 funzionante. La precedente modifica di fairness
non ha risolto il problema fisico e non è mantenuta come presunta soluzione.

## Rollback effettuato nei sorgenti

`desktop/main.c` e `drivers/input.c` sono ripristinati byte per byte dal commit
10.2.4 `1a4ca9af4c4e7bb42cc819e7f931da34f6a4af69`, refetch pubblico confrontato
con hash. Eliminato `input_budget.h`, ora inutilizzato. Nessuna modifica ai nuovi
font UI/Mono, al backend FS2 o alle correzioni storage già presenti.

**Conseguenza voluta:** il compositor non avvia né completa più i trasferimenti
file drag-and-drop. Le vecchie prove DnD non possono essere richieste/annunciate
come passate per questa candidata. Il restante supporto IPC/app rimane nel
sorgente, ma il percorso desktop della funzionalità è disabilitato dal rollback.
Il ripristino dei due file non equivale a ripristinare l'intero kernel 10.2.4:
inizializzazione storage, layout binario e altre modifiche restano da isolare.
Non è ancora dimostrato che il rollback risolva il mouse fisico.

## Installer: diagnosi aperta, non scritture forzate

I 10 GiB non allocati sono ora visibili sul PC dell'utente, ma la tabella viene
rifiutata. Non conosciamo ancora la causa precisa: non si può dichiarare la
GPT effettivamente corrotta, né dichiarare errato il controllo senza metadati.

Separati i messaggi per primaria non verificabile, backup non verificabile,
copie discordanti, MBR ibrido e MBR protettivo incoerente. Non cambiano le
condizioni di idoneità alla scrittura. Nessuna conversione di RAW/MSR/NTFS in
spazio libero e nessuna riparazione della tabella Windows senza consenso.

Gli intervalli positivi inferiori a 1 MiB vengono visualizzati come `<1 MiB`
invece che `0 MiB` in Utility Disco/Terminale; stessa precisione nella riga
destinazione dell'installer quando la capacità allineata è positiva. Questo
non prova che lo specifico `0 MB` segnalato sia uno di tali intervalli: potrebbe
anche riferirsi a un altro disco senza spazio disponibile.

## Controlli eseguiti sulla candidata locale

- Contratto sorgenti: hash esatti 10.2.4 per desktop/input, DnD disabilitato,
  font lock e build FS2 obbligatorio conservati: PASS.
- `test-input-maintenance.sh`: PASS host; scenari di allocazione fallita sono
  iniezioni negative previste dal test, non prove su controller fisico.
- `test-windows-gaps.sh`: PASS normale e ASan/UBSan, inclusi gap selezionabile
  con GPT sana, protezione con copie discordanti e motivo MBR protettivo distinto.
- Riproduzione font senza fallback e contratto cleanup: PASS.

Nessun risultato host di questa candidata è una certificazione hardware o una
nuova accettazione della ISO. Servono ricostruzione e verifica della candidata,
e la diagnosi in sola lettura della tabella del disco reale prima di risolvere
il blocco dell'installer.
