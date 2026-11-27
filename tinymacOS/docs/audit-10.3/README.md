# M0 — inventario e revisione dei testi 10.3

Safari escluso. Questo è un audit di sviluppo, non un changelog di release.

## Riproduzione

Dalla radice `tinymacOS`:

```sh
python3 tools/audit-ui-strings.py --check
python3 tests/test-m0.py
# Nella copia di lavoro completa, anche overlay e sorgenti WebKit preservati:
python3 tests/test-m0.py --workspace
python3 tests/test-font-no-fallback.py
bash tests/test-10.3-foundation.sh
```

L'inventario comprende **2337 letterali statici in 135 file**: userspace 1446, desktop 287, include condivisi 151, boot 61, fs 250, storage 142. Copre app, menu, login/setup, Recupero e messaggi disco/filesystem restituiti ai client. Conserva anche percorsi, identificatori e diagnostica, così non si confondono con testo da tradurre. Commenti, costanti carattere e direttive include sono esclusi. CSV: file, riga, tag di revisione, valore decodificato (caratteri di controllo rappresentati con escape) e letterale C originale. Il riepilogo registra l'hash di ogni file analizzato; il gate fallisce se sorgenti o inventario non corrispondono.

**Limiti espliciti:** non è un preprocessore o un'analisi del flusso di chiamate. Non ricostruisce ogni concatenazione dinamica, espansione macro, testo letto da file o messaggio di librerie terze. I tag sono indizi, non una certificazione automatica di visibilità. Il conteggio non rappresenta il numero di schermate collaudate. La matrice visuale completa resta M7.

## Revisione effettuata

- Installer: corretti accento e gergo `enrollment`; mantenuti rischio d'interruzione, irreversibilità, perdita delle vecchie chiavi, conferma del PMBR e distinzione fra account e password volume.
- Impostazioni: risoluzioni e riavvio senza gergo GOP nella descrizione ordinaria; nessuna promessa di nuovi modi video. Il messaggio dipende ancora dalle capacità reali.
- Monitor: «Campioni per intervallo», senza cambiare contatori o fingere risolto M3.
- Utility Disco: formato nativo chiamato tinymacOS, non APFS/Btrfs Linux; stato snapshot «Sola lettura» e risultato di verifica leggibile, senza il nome dell'algoritmo come decorazione. Conservata la descrizione CoW, utile a spiegare perché non è disponibile una misura esclusiva dei byte.
- Time Machine: esplicitato che gli snapshot sono sullo stesso disco e **non proteggono dal guasto fisico del disco**; nessuna nuova promessa di copia esterna. Rimangono avvisi di perdita dei file dopo un ripristino e di eliminazione irreversibile.
- FileVault: istruzione su copia esterna e ripresa possibile ma annullamento non possibile; nessuna garanzia di rollback aggiunta.
- Terminale: rimossi i falsi riferimenti a un formato «BTRFS/APFS». Gli alias `fsck_apfs`, `mount_apfs`, `newfs_apfs` e `diskutil apfs` restano per compatibilità, non dimostrano supporto ad APFS Apple. BTRFS autentico resta distinto dal formato proprietario nella classificazione delle partizioni.
- Finder/selettore, TextEdit, Note/Memo, anteprima, Galleria, Calendario, Calcolatrice, menu desktop e login/setup: inventariati; nessuna cancellazione indiscriminata di errori, consensi, nomi file o istruzioni. Conservati gli avvisi su dati non salvati e azioni distruttive. Geometria della cronologia Calcolatrice, scorciatoie e prestazioni restano M4.
- Recupero: home e conferme utili conservate; versione resta nel pannello Info su richiesta, non anticipata a 10.3. Avvio BIOS/UEFI e istruzioni per scegliere il disco restano informazioni operative necessarie.

20 revisioni testuali (21 occorrenze); diff e motivazioni sintetiche anche in `workspace/local-10.3/m0/ui-changes.json` nella copia di lavoro. I messaggi tecnici in trace/assert o nelle diagnostiche disco non vengono rimossi solo perché tecnici. SHA-256 nell'ispettore di file è una funzione esplicita e rimane.

## Contratti aggiunti

`tests/fixtures/m0-contract.json` blocca 568 input OS pre-M0 (font, asset, vendor e compatibilità/USB), 177 file del workspace Safari/WebKit e i testi di sicurezza essenziali. Questi hash OS derivano dal commit pre-intervento, non da una rigenerazione alla cieca dei file modificati. Il test include una mutazione negativa: rimuovere un avviso distruttivo deve far fallire il controllo. Gli hash non sostituiscono prove hardware.

La baseline pubblicata 10.2.6, i sei asset e le copie originali dei sorgenti modificati sono tracciati separatamente nelle ricevute M0 del workspace. L'archivio pre-intervento è stato **ricostruito ora da commit immutabili**: non si finge che la cattura estesa fosse già stata eseguita prima di M1.
