# Macroblocco 2 — risultati in corso

Riferimento esclusivo: punto 2 del PDF fornito dall’utente, `tinymacOS/docs/piano.pdf`. Il macroblocco 1 è stato chiuso su sua indicazione esplicita, mantenendo non verificati i collaudi fisici. Questo registro non è un piano sostitutivo.

**Macroblocco 2 IN CORSO, non completo e non pubblicato come release.**

## Correzioni eseguite

- Trovato un errore reale nella sidebar di Utility Disco: il catalogo `S_SNAP` del volume montato veniva mostrato anche selezionando altri dischi con layout nativo. Il catalogo, inclusa «Copia locale», ora viene associato soltanto al disco effettivamente montato. Non viene duplicato sotto dischi diversi né attribuito a live/Recovery. Una selezione non valida non può verificare per sbaglio lo snapshot del volume corrente.
- «Utilizzo» ora mostra spazio usato e capacità reale, ciascuno formattato una sola volta con unità SI: per esempio `8.1 MB su 10.0 GB`. Nessuna concatenazione di Kb/MiB o unità aggiuntive al valore già formattato.
- PASS test normale + ASan/UBSan del modello della vera app: `diskutility-catalogue-host.log`. Verificati due dischi, «Copia locale», snapshot utente, selezione di un altro disco, live e Recovery; nessuna verifica diretta al volume sbagliato.
- PASS test normale + ASan/UBSan del formatter usato/capacità, inclusi limiti e buffer: `used-capacity-host.log`.
- PASS compilazione freestanding dell’app e contratto di preservazione M0/USB/font/overlay: `freestanding-compile.log`, `preservation-host.log`.

## Limiti attuali verificati dall’ispezione

Non è ancora implementata l’esportazione di una copia exFAT, né l’esplorazione reale dei filesystem NTFS/FAT32/exFAT dalla sidebar. Il riconoscimento di una firma filesystem non è un driver di mount. Nessun pulsante viene rinominato per simulare un’operazione non disponibile. Rimane vincolante la scelta dell’utente «Esporta una copia exFAT», preservando originali e snapshot.

Il catalogo snapshot dell’app resta attualmente limitato ai primi otto elementi; il backend dispone già di lettura paginata. Le correzioni iniziali non costituiscono una chiusura dell’intero punto duplicati/catalogo.

Nessuna nuova ISO o prova guest è stata prodotta per queste modifiche del macroblocco 2. Le ricevute del macroblocco 1 restano riferite alla propria build, non a queste modifiche.
