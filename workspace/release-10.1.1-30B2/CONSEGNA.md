# tinymacOS 10.1.1 Tramontana — 30B2

## Artefatto collaudato
- ISO: `tinymacOS-10.1.1-x86_64.iso`, **225.617.920 byte**.
- SHA-256: `fbffff11eb1c0aa6856c8e6f74dbedc2fb10df5698e4fb49de9b860eac5ca508`.
- Due build indipendenti byte-identiche; 575 file sorgente congelati e ricontrollati.
- [Note di rilascio](RELEASE.md) · [QA completo](final-qa.json) · [Ricerca storica e dettagli](HISTORY-AND-CHANGES.md).

## Risultati
| Controllo | Esito |
|---|---|
| Titolo Recovery: pixel, centratura, trascinamento e stessa linea di Terminale | PASS |
| Centro di Controllo completo: 16 prove UI reali, contratto su 5 risoluzioni | PASS |
| Arbitraggio puntatore Recovery, ASan/UBSan e controllo negativo vecchia logica | PASS |
| Driver input, enumerazione, slot già posseduti, legacy | PASS |
| Input live: 17 profili BIOS/UEFI | PASS |
| Input da solo HDD: 10 profili BIOS/UEFI | PASS |
| Verifiche input dei 27 profili | 236 PASS |
| Recovery ottica: EHCI/xHCI, R/F12, mouse/tablet; UEFI mouse/tablet | 10 avvii PASS |
| Recovery italiana: PS/2 e USB senza i8042, BIOS/UEFI | 4 profili PASS |
| Installazione BIOS, riavvio da solo HDD, stesso disco in UEFI | PASS |
| Ripristino Recovery locale sigillato e conservazione documenti | PASS |
| Regressioni kernel/backend/runtime | 29/29 PASS |

Immagini reali conservate in `evidence/`; prove e ricevute legate allo SHA della ISO. Le immagini non sono ricostruzioni. I test kernel/backend non certificano un browser.

## Accesso e conservazione
Credenziali cifrate in `private-backup/github-access.enc`, fuori da Git e dalla ISO. Directory 0700 e file 0600; chiave consegnata separatamente. Verificati archivio autenticato, rifiuto di chiave errata/manomissione, round trip e accesso GitHub dopo ripristino. Conservare la chiave scaricata fuori dal workspace; senza non è possibile il ripristino. [Guida](../ACCESSO-CIFRATO.md).

Per mantenere margine negli snapshot, le evidenze storiche 2.4.2 e integrated-next sono archiviate senza perdita, con verifica SHA-256 di ogni originale prima del consolidamento. `EVIDENCE-ARCHIVE.md` nelle rispettive directory spiega il ripristino dei percorsi. Sorgenti, allegati originali e backup cifrato conservati.

## Limiti non risolti da una prova in VM
**ASRock Q1900M/J1900 fisico non provato:** il problema mouse segnalato dall’utente non può essere dichiarato definitivamente risolto. Nessuna nuova foto F11 necessaria. La 2.4.3 contiene il riscontro storico Recovery funzionante, ma l’identità esatta della versione soprannominata «mouse fix» resta incerta. Safari/WebKit non incluso.

## Pubblicazione source-first
Disposizione autorizzata: **10.1.1 normale/Latest; 10 e 10.1 Pre-release**. La 10.1 è archivio degli stessi byte già compilati, non una ricompilazione. I sorgenti e queste prove vengono sincronizzati prima degli upload. La conferma effettiva, con commit sorgente, asset riscaricati e integrità delle vecchie release, viene aggiunta in `github-release-verification.json` solo dopo la pubblicazione.
