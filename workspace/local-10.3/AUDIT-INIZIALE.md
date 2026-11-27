# Audit iniziale 10.3

> Documento storico del primo checkpoint: i «non eseguito» qui sotto descrivono quel momento, non lo stato corrente. Per il ritorno a M0 e le verifiche aggiornate consultare `m0/VERIFICA.md`; per M1/M2 consultare le ricevute dedicate e `progress.json`.

## Riscontri dai sorgenti

- `userspace/ui.c:Text` serializza ogni stringa in `Draw.text[100]`: massimo 99 byte, indipendentemente dal numero di caratteri Unicode. TextEdit passava una riga visiva intera in un solo comando: possibile troncamento con accenti. Introdotto `MonoText` per segmentare il testo su confini UTF-8 e mantenere le posizioni orizzontali del Mono fornito (advance 8 px).
- TextEdit limitava il wrapping a 88 colonne indipendentemente dall'ampiezza della finestra; click con origine x=24 mentre il testo iniziava a x=28. Il paint riportava sempre la vista al cursore: ciò impediva uno scroll indipendente. Modificati questi percorsi; aggiunti test sulle funzioni reali, non solo su una replica dell'algoritmo.
- Il Monitor calcolava CPU totale con `delta busy_ticks / delta ticks`. In `arch/cpu.c` busy_ticks somma i core; il denominatore non era la somma dei loro intervalli. Ora il Monitor aggrega core_busy/core_ticks, scarta reset/snapshot inconsistenti e conserva il clock di aggiornamento. Nessuna modifica allo scheduler o al percorso USB.
- RAM: `SysInfo.total/free` provengono da `totalpages/freepages` dell'allocatore. `totalpages` viene inizializzato dalla memoria UEFI conventional disponibile, non dalla RAM fisica totale installata comprensiva delle regioni riservate e delle altre classi di memoria. Quindi **l'audit RAM non è concluso** e il contatore non deve essere presentato come censimento completo dell'hardware. Per ora cambiata solo la rappresentazione SI del valore già esportato.
- Utility Disco concatenava realmente «KB usati» e «MiB»: corretta l'unità unica. Introdotto formatter SI condiviso, usato per RAM visualizzata, utilizzo dati, capacità partizioni/dischi e gap nella sidebar. Le dimensioni in input per create/resize restano esplicitamente in MiB finché parser/backend non vengono migrati insieme.
- Utility Disco costruisce ancora parte delle capacità dei volumi sintetici con costanti `z[]`. Il caso dello spazio non allocato dopo l'installazione non è ancora riprodotto né attribuito a una causa: non è stato cancellato il gap o alterata alcuna tabella GPT per nasconderlo.
- `NCMDS=144`: le righe spezzate richiedono più comandi. Il budget dell'intero frame ad alta risoluzione e con testo a quattro byte per scalare resta un gate M1/M4 aperto. Non aumentata arbitrariamente la memoria delle finestre e non dichiarata completa la correzione grafica.
- OpenCore **è già vendorizzato** (`vendor/opencore-1.0.8`) e presente nel percorso della ISO. Il lavoro M6 non consiste nello scaricarlo da zero: serve verificare e completare installazione persistente, ESP e BootOrder del disco e chainload Windows. Nessuna voce firmware è stata modificata in questa sessione.
- Il loader GOP filtra modalità 800×600..1920×1080 e premia 1280×800/1024×768; le framebuffer sono dimensionate a 1920×1080. `drivers/video.c` usa il cambio runtime DISPI solo per dispositivi emulati riconosciuti. Non ci sono evidenze delle modalità realmente esposte dalla GeForce 310 dell'utente: nessuna promessa di risoluzioni aggiuntive.
- exFAT compare nel riconoscimento delle partizioni; questo **non dimostra** l'esistenza di un filesystem montabile/esportatore scrivibile. Servono driver, mount e copia verificata, non una semplice rinomina di «Copia dati».

## Verifiche effettivamente eseguite

Cinque suite host normali e ASan/UBSan, log in `logs/`:

1. `test-textedit-open.sh`: Open/Save precedenti e nuovi casi wrap UTF-8, viewport, click e End.
2. `test-ui-stream.sh`: streaming file precedente e ricostruzione byte-identica dei segmenti Draw prodotti dal vero `MonoText`.
3. `test-display-size.sh`: conversione SI, confini, UINT64_MAX, limiti del buffer.
4. `test-cpu-sample.sh`: helper numerico e vero `sample()` del Monitor con fixture a quattro core.
5. `test-windows-gaps.sh`: protezioni GPT/PMBR e spazio libero conservate.

Inoltre: guardia di ritiro USB e sincronizzazione overlay PASS; compilazione freestanding in oggetti dei quattro file applicativi modificati PASS. Il primo controllo di compilazione ha individuato il riferimento residuo al clock lastTick nel loop del Monitor: corretto e coperto dal test della vera funzione sample().

## Non eseguito

Nessuna build ISO completa, link finale, avvio guest, prova visuale o prova hardware della 10.3. Nell'ambiente corrente non risultano disponibili qemu-system-x86_64, xorriso, mformat, nasm né gli header EFI nel percorso standard. Le prove della 10.2.6 sono storiche e **non vengono riutilizzate come PASS della nuova build**.

Nessuna release, tag o pubblicazione 10.3. Safari escluso e suoi sorgenti invariati.

## Aggiornamento M1 (successivo all’audit iniziale)

- Budget Draw risolto con C_MONO: un riferimento al glifo per scalare Unicode, verificato contro tutte le 207 voci dell'atlante fornito. Draw e NCMDS non sono stati ingranditi. Test della vera paint di TextEdit su 27 combinazioni fino a 1278×1024 (limite utile delle finestre). Un carattere non disponibile resta un solo glifo «?», senza alterare il file.
- C_CARET è un descrittore statico: il compositor mostra solo quello della finestra attiva, lo nasconde dietro overlay e durante animazioni, reimposta la fase visibile all'input e aggiorna piccoli rettangoli sul lampeggio. Non ripubblica i frame delle app. Il login/setup disegnato direttamente mantiene il suo meccanismo dedicato, ora con caret anche nei campi vuoti e politica temporale condivisa.
- InputText condiviso per campi normali e password: scorrimento orizzontale e posizione UTF-8; le password serializzano soltanto riferimenti ai pallini. Adottato in selettore file, Installer e conferma chiave, Utility Disco, FileVault anche in Recovery, ricerca Impostazioni e dialoghi editabili Finder/Calendario. TextEdit, Note/Memo e Terminale emettono il descrittore appropriato senza duplicare il cursore.
- Ambiente build ripristinato (QEMU, OVMF, EFI, strumenti ISO e OCR). Prima build ha completato compilazione app/kernel ma si è fermata a mkfs.fat fuori PATH; nessun PASS ISO assegnato a quel tentativo.
- Autenticazione CLI riuscita e checkpoint 25314a2 pubblicato su main, hash remoto verificato. Nessuna release 10.3 creata.
