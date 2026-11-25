# 10.2.2 — requisito FileVault nativo e blocco pubblicazione

**Stato: non implementato. Non è una nota di rilascio.**

Scelta confermata dall'utente: cifratura nativa di tutto il volume dati di tinymacOS, inclusi nomi/metadati, sblocco prima del desktop, chiave di recupero, gestione in Impostazioni e Recovery, migrazione senza formattazione automatica. Non si richiede interoperabilità con APFS/FileVault Apple.

## Risultati della verifica del codice esistente

- `fs/btrfs.c:enc_xform` applica XOR con un flusso SHA-256 derivato dalla chiave e dalla posizione. Le riscritture riutilizzano il flusso: non costituisce un cifrario autenticato per settori e non va esteso a un nuovo formato completo.
- `enc_ex` esenta percorsi di sistema e qualsiasi sottocartella Library. Le preferenze personali non sono tutte protette.
- La tabella `Node`, con nomi, parent e dimensioni, e i settori account 13/14 restano in chiaro. `fs_init` legge la tabella prima di `session_init`.
- `enc_mksalt` miscela data, tick e indirizzi. Non è un generatore di chiavi. `kernel/salt.c` dichiara esplicitamente di fornire sali pubblici, non chiavi o token di recupero.
- `enc_enable_i` salva lo stato prima di trasformare i file e non contiene un protocollo persistente di conversione/ripresa per settore. Un'interruzione può lasciare stato misto. Non va usato per una migrazione completa.
- `fs/install.c:disk_control_i` e `storage/maintenance.c` accedono anche direttamente a copie/ripristino. Una cifratura solo in `bytes()` lascerebbe vie in chiaro o produrrebbe copie non ripristinabili.
- Non esistono keyslot indipendenti per password e recupero, rotazione del codice di recupero o UI di sblocco del volume installato in Recovery.

## Requisiti prima del tag/deploy v10.2.2

1. Formato versionato per il volume dati, capienza validata rispetto alla GPT; identificazione fail-closed senza inizializzazione su header danneggiato.
2. Primitive standard da implementazione mantenuta, vettori noti e confronto con implementazione indipendente. Generazione crittografica di chiavi e nonce, senza fallback a data/tick/indirizzi o riuso dell'entropia di boot.
3. Chiave volume casuale e keyslot separati per password e recupero; KDF parametrizzata, confronto costante, autenticazione degli header, azzeramento dei segreti, rotazione e revoca verificabili.
4. I/O cifrato sotto file, metadati, account, snapshot e copie locali. Nessun percorso raw può aggirarlo per operazioni sul volume dati.
5. Migrazione reversibile e riprendibile, journal/commit verificati dopo ogni possibile interruzione; spazio di recupero riservato prima della prima scrittura. Mai avviare conversioni in-place non transazionali.
6. Mount iniziale bloccato, sblocco prima di leggere dati/account, errori non distruttivi, chiave rimossa dalla RAM al blocco; client sospesi e cache eliminate coerentemente.
7. Impostazioni: attivazione con conferma password, visualizzazione singola e conferma salvataggio chiave di recupero, stato/progresso, disattivazione e cambio credenziali.
8. Recovery: selezione e sblocco espliciti del disco, password o recupero, copie/ripristino e reinstallazione senza esposizione involontaria o reset delle credenziali.
9. Test host con fault injection su tutte le scritture/flush, reboot nel mezzo della migrazione e della rotazione, header/tags danneggiati, password errata, vecchie chiavi revocate, scansione offline che non trovi nomi/contenuti/metadati di prova.
10. Collaudi BIOS e UEFI sull'identica ISO finale, nuovi test Finder, regressioni input e Recovery, commit/tag/pacchetti coerenti e rilettura degli allegati pubblicati.

La correzione dei clic Finder è separata dal formato di archiviazione: può essere collaudata senza modificare i dati esistenti. Finché questi requisiti non sono implementati e verificati, non pubblicare la 10.2.2 come FileVault completo e non muovere il tag 10.2.1.
