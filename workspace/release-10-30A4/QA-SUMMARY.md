# tinymacOS 10 / 30A4 — QA finale

**PASS** del gate completo `collect_qa.py`, con risultati legati alla ISO corrente.

## Modifiche verificate

- Titolo esatto **tinymacOS Recovery** nella barra superiore e nella finestra principale; nessuna dicitura Tastiera ITA nel binario o nel framebuffer. Mappa fisica italiana mantenuta e nuovamente provata.
- Applicazioni nel Finder: simbolo dedicato slot 13 realmente disegnato in griglia (54 px), elenco (24 px) e barra laterale (19 px). Controlli host ASAN/UBSAN sulla funzione reale: una cartella utente omonima rimane generica, un file omonimo non diventa una cartella e la priorità delle icone delle app è invariata.
- Installer Recovery: byte identici alla risorsa a 64 px usata dal desktop; nuovo simbolo effettivamente individuato nel menu Recovery disegnato. Rimane procedurale, non un'immagine importata.

## Regressioni

- Due build indipendenti identiche, 517 input congelati verificati.
- 17 profili live + 10 installati senza ISO, 236 asserzioni input e 27 verifiche degli assi indipendenti.
- 4 profili tastiera nelle app, 4 Recovery e 5 client aggiuntivi; Mission Control e allineamento menu.
- 10 profili di aspetto, filesystem/atlas/Recovery, icone applicazioni e simboli procedurali, associazioni sfondi, wordmark e versione 10.
- Installer BIOS/UEFI, persistenza, avvio da disco senza ISO e ripristino locale sigillato con entrambi i puntatori.
- 8 prove Recovery ottico BIOS: R/F12 × EHCI/xHCI × mouse/tablet, oltre a Recovery ottico UEFI. Il controllo negativo originale 30A2 è storico e usa lo stesso harness byte-identico; tutti i PASS positivi sono della nuova ISO.
- 29/29 regressioni kernel e kernel delle cinque sonde identico alla produzione. Non sono test di un browser.

ISO: `tinymacOS-10-x86_64.iso`, 225599488 byte.
SHA-256: `660bd4f2656ccbce70fe53d878015f0dae993d9ff16d55f5488d9fd2b2a2b65e`.

Nessuna nuova prova sul PC fisico ASRock: nessuna soluzione hardware confermata. Safari/WebKit rimane sospeso e non incluso. L'esito della pubblicazione e dei download è registrato separatamente in `github-release-verification.json`.
