# tinymacOS 10.1.1 Tramontana — 30B2

## Artefatto collaudato
- ISO: `tinymacOS-10.1.1-x86_64.iso`, **225.617.920 byte**.
- SHA-256: `fbffff11eb1c0aa6856c8e6f74dbedc2fb10df5698e4fb49de9b860eac5ca508`.
- Due build indipendenti byte-identiche; 574 file sorgente congelati e ricontrollati.
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
**Pubblicazione completata e verificata:** [10.1.1 normale/Latest](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1.1); [10.1 Pre-release](https://github.com/ESPNex/tinymacOS/releases/tag/v10.1) e [10 Pre-release](https://github.com/ESPNex/tinymacOS/releases/tag/v10).

- Sync sorgenti PRIMA degli upload: commit [`50e310af4dd360818b5a6e15816116db90676d1f`](https://github.com/ESPNex/tinymacOS/commit/50e310af4dd360818b5a6e15816116db90676d1f).
- La 10.1 è archivio degli stessi byte già compilati, non una ricompilazione: SHA-256 `32e8ba640c4ba45e8df01db56ad69dfe27bed30497c0ea512a2cb51140b00275`, tag al commit storico `0965f921147062721fed7e7adea9a750cd0beceb`.
- Entrambe le ISO e i checksum riscaricati da GitHub; dimensioni, SHA-256 e digest API corrispondenti.
- Tutti gli asset storici conservati; nuova 10.1.1 unica release normale/Latest, nessuna bozza rimasta.
- Ricevuta completa: [github-release-verification.json](github-release-verification.json). Le ricevute sono sincronizzate nel commit successivo, senza spostare i tag dei sorgenti.
- [Download ISO 10.1.1](https://github.com/ESPNex/tinymacOS/releases/download/v10.1.1/tinymacOS-10.1.1-x86_64.iso) · [SHA256SUMS.txt](https://github.com/ESPNex/tinymacOS/releases/download/v10.1.1/SHA256SUMS.txt). Il repository è privato: serve l’account autorizzato.
