# 10.2.5 / 30B16 — gate di pubblicazione ancora aperto

## Decisione di perimetro

L'utente ha richiesto pubblicazione, test e commit, scegliendo esplicitamente di
**completare anche installazione/avvio 4Kn nativi e conversione/deattivazione della
cifratura in-place prima della release**. Non è stata autorizzata l'esclusione di
queste funzionalità. Il successivo rinnovo dell'autorizzazione GitHub non modifica
questa decisione. Non basta documentare i limiti per considerare passato il gate.

## Verificato

- Accesso GitHub CLI ripristinato, account `ESPNex` verificato tramite API;
  permesso di push su `ESPNex/tinymacOS`. Configurazione/token solo in `/var/tmp`,
  mai nel repository, nel workspace persistente o nella ISO.
- Finder/Terminale/drag-and-drop: suite host normali e ASan/UBSan e 11 controlli
  GUI, incluso riavvio a freddo e verifica completa del volume. Vedere
  [FINDER-TRANSFERS-10.2.5.md](FINDER-TRANSFERS-10.2.5.md).
- Installer plaintext e Recovery/reinstallazione sul medesimo candidato Finder:
  PASS, account/documenti/snapshot conservati e DATA invariato alla reinstallazione.
- Installazione cifrata, backup/UI scalabili, reset e importazione di checkpoint
  esterni hanno proprie ricevute storiche con hash espliciti. Non sono una matrice
  finale completa sul candidato Finder.

Candidato Finder: ISO SHA-256
`c2eb0a5f1ea3ea931d12048c380ca5d2f389aa691f0ac34b3b5c34e75ac62b3a`;
KernelCache `3d98e1118960bfd3c7b4267e5a8b7148ab6b87c72552e5119fa5afc58cad3f72`.

## Blocchi tecnici effettivi

1. **4Kn nativi non completati.** Il trasporto NVMe gestisce già namespace
   4096 B e RMW parziali nei test host, ma `drivers/nvme.c` li esclude
   deliberatamente dall'inventario. GPT/MBR e il percorso di installazione
   presuppongono LBA logici da 512 B; `boot/containerboot.h` e
   `boot/recoveryboot.h` richiedono ancora BlockSize=512. Servono geometria nativa,
   supporti FAT/ESP coerenti e veri test installazione, avvio e Recovery 4Kn.
   Rimuovere solo il controllo del driver sarebbe scorretto e rischioso.
2. **Conversione in-place non completata.** `fs/tmcbfs2_volume.c` rifiuta
   attivazione/decrittazione su FS2 esistente. FV003 utilizza record autenticati
   da 20 KiB per blocchi logici da 16 KiB, più header/journal: non può sostituire
   plaintext negli stessi offset senza pianificazione dello spazio e ripresa
   dopo interruzione. Occorrono mantenimento di account/documenti/snapshot,
   preflight senza scritture, autenticazione, journal persistente e fault tests,
   oltre all'integrazione Recovery/UI/CLI. La nuova installazione cifrata non è
   una conversione in-place.
3. **Accettazione finale sul nuovo artefatto.** Dopo qualsiasi modifica ai due
   punti precedenti occorrono build pulita e nuova matrice esatta, inclusi BIOS
   con firmware NVMe disabilitato/helper, UEFI, Recovery, cifratura/reset/import,
   regressione Finder/Terminale e verifica indipendente degli asset distribuiti.

Il reset cifrato esistente è esplicitamente irreversibile e non atomico. Non è
secure erase fisico; non promette rollback o resistenza al replay di immagini
precedenti. Nessuna certificazione hardware fisico o Windows.

## Commit e pubblicazione sono operazioni distinte

È consentito salvare e sincronizzare questo sviluppo su
`work/10.2.5-scalable-fs-nvme`. Un commit/push di sviluppo non costituisce
accettazione della release. **Non creare/promuovere `v10.2.5`, non aggiornare
`main` e non dichiarare un deploy pubblico concluso finché i gate sopra restano
aperti.** La release pubblicata resta 10.2.4.

L'esecuzione corrente dei 29 script host e il suo esito sono registrati in
`workspace/local-10.2.5/release-completion-progress.json` e nei log esterni
`/var/tmp/tmc-release-10.2.5/`. Non dedurre PASS dalla sola presenza di un log:
serve il risultato del processo completo.
