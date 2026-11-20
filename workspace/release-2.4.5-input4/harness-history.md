# QA 21B9: controlli e limiti

- Nessuna ricevuta 21B8 riutilizzata come PASS del nuovo artefatto.
- Una prima candidata 21B9 è stata scartata prima del collaudo completo perché START.TXT riportava ancora 21B8. Aggiornata la guida sul supporto e ricostruita integralmente; collector vincolato al nuovo hash.
- In Recupero UEFI: R alla mela osservata. Il tentativo ottico BIOS non ha osservato lo stesso logo nel tempo previsto, prima di qualsiasi prova della nuova tastiera. Non è stato trasformato in PASS né corretto con remapping Unicode. Il Recupero BIOS è invece verificato dal disco appena installato, con ISO rimossa, corrompendo deliberatamente il KernelCache per esercitare il rifiuto sigillato e l'avvio locale. È lo stesso percorso BIOS usato nel collaudo di ripristino precedente; la selezione ottica con R resta non certificata.
- I profili tastiera USB usano q35,i8042=off e controllano l'intero qtree per i8042; i profili PS/2 ne attestano la presenza.
- I test del mouse selezionano mouse_set/query-mice e verificano il trasporto anche alla fine; aggiunti movimenti soltanto X e soltanto Y con verifica dello sprite effettivamente disegnato.
- La riproduzione del difetto verticale è un controllo negativo del formato dei pacchetti, non una cattura USB del PC fisico.

- L'ispezione visiva prima del deploy ha scoperto che il valore euro era salvato correttamente (hash verificato) ma mancava dal font: veniva disegnato come punto interrogativo. Scartata anche la candidata `7a57bb35…`; aggiunto il glifo in tutte le dieci facce, lasciando immutati descrittori e pixel dei glifi esistenti. Rafforzati i gate grafici per la presenza e il disegno di euro; tutta la matrice media è rieseguita sul nuovo hash. Le ricevute precedenti sono solo scratch esterno, non PASS finali.

- Il secondo giro kernel ha rilevato due FAIL nel probe rete: STREAM, WAIT e RESET erano PASS, ma il controllo SHA-256 del file di sola lettura era ancora fissato al font precedente da 2215962 byte. Conservata la ricevuta in `superseded-network-fixture.json`; aggiornati i valori attesi del fixture al font euro di 2226987 byte, SHA-256 `f8c1b1b5b598d97eedd1765040076f516c8522a7dc8250febd3f914544e6d174`. Nessuna asserzione rimossa o tolleranza ampliata. Rieseguita tutta la suite, non soltanto i due casi.
