# Rimozione definitiva del filesystem legacy

## Decisione applicata

Richiesta: eliminare il filesystem legacy dal progetto. Scelta esplicita: **vecchi volumi non più supportati, nessuna migrazione**. I dati esistenti non vengono cancellati, convertiti, formattati o importati automaticamente. Questa build non può avviare né montare una vecchia installazione nativa di prima generazione. Per continuare a usare quei dati serve conservarli e accedervi con un ambiente precedente; questa build non include tale ambiente.

Non è stata riscritta la cronologia Git: le vecchie implementazioni rimangono nella storia dei commit, non nel sorgente corrente o nei binari distribuiti. Le evidenze storiche restano identificate come tali.

## Implementazione rimossa realmente

- Eliminato `fs/btrfs.c`, sostituito da `fs/namespace.c`: dispatch esclusivamente TMCFS2 e filesystem esteri, con lo stesso gate ricorsivo e le operazioni cross-filesystem verificate.
- Eliminati driver/formatter/geometry/resize e snapshot CoW di prima generazione (`native_resize.c`, `native_cow.c` e header), lettore legacy nel parser estero e fallback delle operazioni Installer/Recovery.
- Eliminato il backend crittografico FV002 e il relativo adattatore legacy. TMCFS2/FV003 e la conversione esplicita plain-TMCFS2 ↔ FV003 restano distinti e conservati; non sono conversioni di filesystem legacy.
- Eliminati i test/generatori specifici del motore rimosso; i fixture negativi conservano solo firme opache per verificarne il rifiuto, non un parser o un generatore del vecchio filesystem.
- Nessun flag può riabilitare il backend rimosso. `check-legacy-retirement.py` è eseguito dalla build e dalla suite foundation: controlla sorgenti assenti, assenza dei simboli ritirati in entrambi i kernel e firme vecchie confinate al solo riconoscimento «non supportato».

## Sessioni live, Recovery e installazione

Anche live e Recovery ora eseguono il vero core TMCFS2 su pagine RAM sparse. Lo sblocco FV003 usa una sessione volatile separata e poi rimonta il vero volume, senza leggere account o documenti dal volume bloccato.

L'Installer non usa più un'immagine legacy come sorgente: `TMCSEED2` è un trasporto temporaneo verificato con checksum, non un filesystem montabile. Trasferisce dal core TMCFS2 directory, file, sola lettura e due record account verso un nuovo volume TMCFS2 esplicitamente autorizzato. Rifiuta firme vecchie, dati troncati/corrotti e strutture non valide prima della formattazione. Il buffer d'installazione è limitato a 3,219,456 byte e 4096 record: se la sessione live non vi entra, l'installazione viene rifiutata, non troncata. Questi non sono limiti del filesystem TMCFS2 montato.

## Verifiche completate

**Build completa PASS**, con ISO finale byte-identica a quella della prima esecuzione QA. Manifest di 1075 input di produzione invariati in `production-inputs-final.json`; hash artefatti in `artifacts-final.json`.

Host PASS, normali e ASan/UBSan dove previsti dai runner:

- nuovo trasporto: core export/import, file Unicode da 65537 byte, parent creato dopo i figli e poi spostamento, account, sola lettura, snapshot/rimontaggio, rifiuti e interruzioni di scrittura;
- rimozione runtime: rifiuto di TMCBFS1/TINYFS1, formato sconosciuto e TMCFS2 corrotto con SHA del disco invariato e zero scritture; vere sessioni live/Recovery TMCFS2 e passaggio RAM-sblocco/mount;
- dispatcher offline Recovery: verifica/riparazione/reset/backup/restore/crittografia dei formati ritirati rifiutati, SHA dell'intero disco invariato;
- Installer reale GPT/MBR e FV003, PMBR incompleto, partizioni FAT32/NTFS/RAW preservate, failure injection, cancellazione e fonte corrotta senza scritture;
- installazione 4Kn, conversione FV003 esplicita di TMCFS2, dischi da 1 TB/16 TiB e basi oltre 2 TiB;
- filesystem esteri, formattazione Utility Disco, namespace RW e copia su nuova partizione exFAT reale; verifiche indipendenti;
- Home UTF-8 plain/FV003, runtime FS2 con oltre 1100 file e snapshot immutabili, Rinomina, foundation/M0, font/asset/vendor/USB protetti e overlay ABI/sorgenti.

**Guest grafici reali PASS sulla stessa ISO finale**, tramite input mouse/tastiera, non eventi applicativi iniettati:

1. Installer grafico crea TMCFS2 dal live TMCFS2; OpenCore principale verificato prima del primo boot; avvio NVMe senza ISO, account/documenti importati, snapshot e restore; Recovery interna reinstalla il sistema con SHA dell'intera partizione dati invariato; successivo avvio e contenuti verificati.
2. Due firme legacy collegate come partizioni reali: Terminale mostra «non supportato», lookup/scrittura/ridimensionamento confermato rifiutati; Utility Disco non offre «Mostra nel Finder». Dopo spegnimento reale, SHA dell'intero disco collegato identico.
3. Nuova creazione TMCFS2 tramite Utility Disco, apertura Finder, file da 65537 byte; Rinomina di file e directory su Home TMCFS2, FAT32, NTFS, exFAT e nuovo TMCFS2, con collisione preservata. Dopo spegnimento: estrazione TMCFS2 read-only e controlli indipendenti dei filesystem esteri.

I primi tentativi QA sono stati interrotti per pressione di memoria della sandbox (2 GB), compilazioni sanitizzate concorrenti e input grafico perso/ripetuto. La convalida finale è stata eseguita sequenzialmente con press/release tastiera distanziati; nessun workaround nel sistema distribuito. I log iniziali restano distinguibili da quelli `*-final.log`. La prima foundation dopo una pulizia whitespace ha richiesto il riallineamento del manifest statico UI; il controllo finale passa senza cambiare font o avvisi.

## Preservazione e limiti

- Backup persistente limitato alla storia raggiungibile di `main` attraverso il confine shallow documentato; riferimenti ausiliari non necessari esclusi dal bundle del workspace per rispettare il tetto di 128 MB. Nessun file utente o font eliminato.
- Nessuna conversione dei dischi, nessun VHD, nessuna modifica implicita alle partizioni estere; i formati precedenti non vengono trattati come spazio libero.
- Ridimensionamento TMCFS2 ancora non implementato, rifiutato senza scritture.
- Nessuna nuova release/versione, nessun avanzamento al macroblocco 3 e nessuna eliminazione del lavoro M4 congelato.
- Nessuna certificazione fisica GPU/HDMI o Windows: QEMU e test host non la sostituiscono.
