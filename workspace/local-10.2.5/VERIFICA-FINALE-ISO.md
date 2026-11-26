# tinymacOS 10.2.5 — duplicazioni corrette e verifica finale

**Esito: PASS — 31 suite host e 11 gate guest rieseguiti da zero.**
Questa verifica usa la **ISO BIOS+UEFI effettivamente riscaricata dalla release pubblica**, non una ricompilazione né una build 10.3. I risultati storici restano conservati separatamente.

## Release senza duplicazioni

- **19 release attive, tutti i titoli univoci** (prima: 21).
- Per la 2.4.4 rimane una sola scheda: [OC2 Developer Preview](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-oc2-developer-preview), con ISO e checksum originali invariati.
- Prima di rimuovere le schede originale e OC1 sono stati scaricati e verificati tutti e quattro gli asset; ISO, checksum, descrizioni e metadati sono stati archiviati su GitHub. L’archivio è stato riscaricato e verificato **prima delle rimozioni**.
- [Archivio storico 2.4.4](https://github.com/ESPNex/tinymacOS/releases/download/v2.4.4-oc2-developer-preview/tinymacOS-2.4.4-varianti-storiche.tar.xz) — SHA-256 `f12a991c02cb8fcaf53b917f730766684e589bc1ed6229d80f8c69a3b24db13a`.
- Tutti i **25 riferimenti Git originari** e i **21 tag di maturità** risolvono agli oggetti originali verificati. Nel controllo finale mancavano 12 alias storici: ripristinati agli SHA esatti dello snapshot precedente, senza spostare alcun tag esistente; la causa della loro assenza non è stata accertata. La maturità continua a essere indicata nei tag, non nei titoli. Schede attive: 9 Developer Preview, 8 Beta, 2 Pre-Release.
- Rimangono solo i rami `main` e `vendor/syslinux-6.04`; il vendor è invariato.

## ISO verificata

[Release 10.2.5](https://github.com/ESPNex/tinymacOS/releases/tag/v10.2.5-pre-release) · [Scarica l’unica ISO BIOS+UEFI](https://github.com/ESPNex/tinymacOS/releases/download/v10.2.5-pre-release/tinymacOS-10.2.5-x86_64.iso)

- Nome: `tinymacOS-10.2.5-x86_64.iso`
- Dimensione: **329.791.488 byte**
- SHA-256: `1ce54378e933ca388f0fa1ba2471612422a826a2ec033722b29daab4bbfcb30a`
- Kernel estratto dal download: `8cbc7c1bf850d9e224ef15d55cfa1cf5621ca24ffa6137c9c6daeabe69d959db`
- Riscaricati e verificati tutti i cinque asset originali; verificati anche il file checksum, i 35 payload condivisi e i 986 input sorgenti compilati contro l’archivio sorgenti pubblicato.
- Kernel e font usati nei controlli guest estratti dal download stesso. Nessun binario ricostruito o sostituito; descrizione e cinque asset originali della 10.2.5 invariati. Aggiunto soltanto l’archivio delle nuove evidenze, non un’altra ISO.

## Nuovi test

| Gate guest | Esito |
|---|---|
| ISO ottica UEFI e BIOS/OpenDuet: desktop, account, Terminale, AHCI e scrittura/lettura | PASS |
| Installazione GUI NVMe 512, avvio senza ISO, snapshot/restore e reinstallazione interna con DATA invariata | PASS |
| Installazione GUI NVMe nativa 4Kn, avvio senza ISO e reinstallazione Recovery con DATA invariata | PASS |
| Attivazione/disattivazione cifratura in-place 512, password/recupero e scansione plaintext | PASS |
| Attivazione/disattivazione cifratura in-place 4Kn, password/recupero e scansione plaintext | PASS |
| Finder: menu Copia/Taglia/Incolla, drag/drop e Ctrl-drag; Terminale e persistenza al riavvio | PASS |
| Helper BIOS NVMe: controllo negativo senza helper, avvio sistema e Recovery con helper pubblicato | PASS |
| Importazione checkpoint esterno: anteprima senza scritture, sorgente read-only invariata e ripristino | PASS |
| Installazione GUI cifrata, credenziali, backup/restore, reinstallazione e reset cifrato | PASS |
| Finder/chooser con 1.100 file, TextEdit 128 KiB, Time Machine 20 snapshot, backup/restore Recovery | PASS |
| Reset logico Recovery: annullamento senza scritture, nuovo account e persistenza dopo riavvio | PASS |

Anche tutte le **31 suite host**, incluse le modalità sanitizer previste dagli script, sono passate. Il nome host `browser-pages` riguarda la paginazione Finder/Time Machine, **non Safari**.

### Trasparenza dei test

- Il primo tentativo ottico si è fermato dopo il PASS UEFI: il runner esportava per errore l’helper BIOS NVMe anche per il caso SATA, rifiutato dal launcher prima di avviare QEMU BIOS. Corretto lo scope della variabile nel solo runner e rieseguito l’intero gate su una fixture nuova: PASS UEFI e BIOS. Prima ricevuta, log ed evidenze sono conservati; la ISO non è stata modificata.
- Installazioni guest su dischi VM nuovi, senza formattazione host. Il test di importazione esterna copia sullo host il disco installato per creare la destinazione. Il test di paginazione aggiunge esplicitamente offline la fixture di stress (1.100 file, 20 snapshot, file da 128 KiB) prima delle prove nella vera interfaccia guest; non è una creazione massiva eseguita dal guest.

## Evidenze conservate su GitHub

[Nuova verifica completa: JSON, log, screenshot e runner](https://github.com/ESPNex/tinymacOS/releases/download/v10.2.5-pre-release/VERIFICA-FINALE-10.2.5.tar.gz)

- Archivio: `18876716` byte, **254 file**.
- SHA-256: `254ff8b92ecf2d244d7cee9aa6c0609d40f85562831263cecc005de1219d35e9`.
- Ogni membro è stato verificato contro il manifest; dopo l’upload, l’archivio è stato scaricato indipendentemente da GitHub e il suo hash è stato ricontrollato.
- Ricevuta compatta locale: `release-final-downloaded-iso-verification.json`.
- Media, dischi VM e archivio completo non occupano il workspace persistente, mantenuto sotto il limite di 128 MB.

## Limiti non nascosti dal PASS

È accettazione software su **QEMU**, non certificazione su ogni PC: nessun nuovo test su hardware fisico. BIOS NVMe richiede l’helper quando il firmware non supporta il controller. La ISO non è USB-DD hybrid. Resize 4Kn e resize cifrato non sono supportati; reset logico non significa cancellazione sicura né atomicità contro ogni interruzione. **Safari/WebKit completo non è ancora disponibile. Nessuna ISO 10.3 è stata costruita o pubblicata.**
