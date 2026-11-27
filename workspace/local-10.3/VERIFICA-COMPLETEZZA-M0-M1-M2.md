# Verifica di completezza M0–M2

## Aggiornamento dopo le correzioni M1

**M1: gate locali host/guest ora chiusi**, con accettazione fresca completa, round-trip nativo di file UTF-8 da 144.000/141.735 byte, input/caret continuo e password in setup/Installer/Recovery, no-echo e vero sblocco FV003 da Terminale. Ricevuta dettagliata: [`m1/VERIFICA.md`](m1/VERIFICA.md).

**M2 resta NON completo:** nessun supporto >2 TB aggiunto; restano le prove 4Kn in coesistenza e il confronto integrato delle tre viste. Nessun avanzamento M3.

Il resto di questo documento conserva il precedente audit su `f7576f6` per rendere verificabile quali mancanze sono state corrette; non è il nuovo verdetto su M1.

## Audit precedente (conservato)

Sorgenti verificati: `f7576f6dc58eb4cf2810ebae2cee4138e564bf5b` su main. Audit rispetto a `tinymacOS/docs/PIANO-10.3.md`, non al solo elenco dei commit. Nessuna modifica funzionale effettuata in questo controllo.

## Esito

| Blocco | Esito rispetto al piano |
|---|---|
| M0 | Gate tecnici locali soddisfatti; consolidato come baseline di sviluppo. Revisione visuale campionaria, non certificazione di ogni schermata. |
| M1 | NON completo: implementazione principale presente, ma accettazione incompleta e requisito Terminale senza eco non dimostrato. |
| M2 | NON completo: correzioni principali presenti, ma requisito >2 TB incompatibile con il limite attuale; verifiche 4Kn e coerenza runtime fra le viste incomplete. |

Non è corretto dichiarare «M0, M1 e M2 tutti completati». I precedenti PASS di M1/M2 descrivono prove mirate, non tutti i criteri del macroblocco. Le prove hardware dell'intera release non vengono usate come unico motivo per tenere aperti i blocchi: qui ci sono anche lacune specifiche di test e un limite funzionale concreto.

## M0

Verificato nuovamente:

- Contratto M0, inventario statico 135 file/2337 letterali, sincronizzazione overlay e protezioni USB: PASS.
- Ricostruzione dei font forniti senza fallback: PASS, TFN e panic font esatti.
- Suite foundation corrente, incluse varianti normali/sanitizzate e compilazioni freestanding: PASS.
- Tutti i 32 artefatti della build M0 conservata coincidono ancora con gli hash della ricevuta di ripetibilità. La ricevuta documenta due build pulite identiche, compresa la ISO.
- Hash della copia locale dei sorgenti originali: PASS. La baseline estesa è stata ricostruita dai commit immutabili, non acquisita retroattivamente prima di M1.
- Nuova lettura GitHub del tag e dei sei asset pubblicati 10.2.6: commit, dimensioni e digest invariati. Non è un nuovo download della vecchia ISO.

Limite dichiarato: inventario statico e revisione mirata non coprono automaticamente ogni testo dinamico o messaggio di librerie terze. Il collaudo visuale integrale resta nella matrice finale M7; M0 non va descritto come una certificazione visuale dell'intero sistema.

## M1 — ciò che c'è

- Wrapping UTF-8 dipendente dalla larghezza, scroll indipendente, click e Home/End sulle righe visive.
- Serializer compatto e test della vera paint su 27 combinazioni, fino a 1278×1024, senza aumentare il budget Draw.
- Caret condiviso con reset della fase, soppressione per focus/overlay e aggiornamenti parziali.
- Host test di campi vuoti, mascheramento password, focus e Note/Memo; guest storico con USB e pixel per setup, Terminale, TextEdit, Note e Memo.

### Mancanze per chiuderlo

1. **Digitazione continua:** il guest misura il lampeggio dopo una pausa e verifica il testo scritto; non misura sistematicamente la visibilità del caret durante tutta una sequenza di digitazione/cancellazione/movimento. Il meccanismo ha test host, ma manca la prova end-to-end richiesta.
2. **Matrice focus/password/Recupero:** mancano prove guest dedicate e complete di click/Tab, perdita del focus, password grafiche e campi editabili nel Recupero. Il test M0 entra nel Recupero e apre Info: non collauda questi campi.
3. **Terminale senza eco:** i test Terminale esistenti sono soprattutto clipboard; non provano un prompt password no-echo con posizione del cursore indipendente dalla lunghezza. Nei sorgenti esaminati non risulta una modalità interattiva no-echo dedicata: durante un job il normale input viene ignorato (`if(job)return`). Non è una prova di divulgazione della password, ma il requisito non è soddisfatto/dimostrato da quanto consegnato.
4. **Round-trip e confini in integrazione:** i test host dimostrano che il wrapping non modifica il buffer; il mock di `writefile` non è una rilettura del filesystem. Manca una prova dedicata completa di salvataggio/rilettura byte-identica dopo click, scroll, resize, inserimento e cancellazione ai confini, con file lunghi con e senza newline.

Queste lacune non dimostrano che ogni percorso sia guasto: impediscono di dichiararne conclusa l'accettazione.

## M2 — ciò che c'è

- Formatter SI e test a 999/1000, 10^6, 10^9, 10^12, overflow e limiti di buffer: PASS.
- Input MB decimale GUI/CLI, allineamento per difetto e dimensione effettiva confermata: test host PASS.
- Geometrie vere in Utility Disco, nome account sulla home e rimozione dell'azione duplicata snapshot.
- Contenitore FS2 v2 sull'intero gap scelto; compatibilità v1.
- Guest storico con installazione in gap di 10 GiB, metadati/estensione FS2 verificati e successivo cold boot; protezioni delle partizioni estranee documentate.

### Mancanze per chiuderlo

1. **Capacità >2 TB: blocco funzionale certo.** Il piano la include nell'accettazione. `include/partition.h` imposta `TMC_DISK_MAX_SECTORS = 2147483648` a settori da 512 byte: massimo **1 TiB**. `storage/partition.c` rifiuta capacità superiori. Il test large-disk verifica espressamente questo rifiuto, non il supporto >2 TB. Occorre implementazione coerente delle capacità/LBA oppure una modifica di ambito approvata; il limite non può essere ignorato trasferendolo a M7.
2. **4Kn sulla nuova geometria:** rilettura FAT nativa 4096 nelle build non equivale a installazione/cold boot FS2 v2 in coesistenza su disco 4Kn. La ricevuta guest M2 disponibile riguarda NVMe a settori logici di 512 byte.
3. **Stessa installazione, tre viste:** il guest controlla Installer, tabella e superblock, poi il cold boot e un file importato. Non confronta ancora in modo completo e automatizzato le capacità di Installer, Utility Disco e Terminale sulla stessa installazione prima/dopo riavvio. Il test Utility Disco usa geometrie simulate: utile, ma non sostituisce tale verifica integrata.

## Ordine consigliato

1. Non avanzare M3 come se M1/M2 fossero già chiusi.
2. Chiudere la matrice e il requisito no-echo di M1.
3. Risolvere esplicitamente il vincolo >2 TB e completare le prove 4Kn/coerenza delle viste di M2.
4. Conservare separati i gate hardware e di release M7.

Riferimenti: `m0/VERIFICA.md`, `m0/build-repeatability.json`, `m1-guest-result.json`, `m2-guest-result.json`, `M2-VERIFICA.md`; test `textedit_open_host.c`, `textedit_frame_host.c`, `caret_overlay_host.c`, `ui_stream_host.c`, `terminal_clipboard_host.c`, `m1_text_caret_guest.py`, `diskutility_extent_host.c`, `large_disk_host.c`, `pmbr_signature_guest.py`.
