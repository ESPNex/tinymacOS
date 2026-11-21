# Collaudo v10 / 30A2

Gate principale superato sulla ISO `tinymacOS-10-x86_64.iso`.

- SHA-256: `f6144153f7732af388310bab78e0947f89044fe11a64e209aa238ea0a611d87e`.
- Due ricostruzioni indipendenti identiche, 514 input verificati.
- 17 profili input live + 10 da disco installato; 236 verifiche.
- 29/29 regressioni kernel; kernel di produzione identico in tutte le cinque sonde.
- 10 combinazioni tema/modalità; icone, versione interna e associazioni sfondi verificate nel framebuffer.
- Recovery: risorse procedurali salvo Terminale, cinque icone di menu confrontate nei pixel.
- 4 profili italiano app + 4 Recovery + 5 app aggiuntive, installer, ripristino locale e Mission Control superati.
- Mouse e tablet USB: Recovery ottico UEFI e Recovery locale BIOS superati in QEMU.

**Non superata:** prova supplementare del selettore R per Recovery ottico BIOS; stesso esito con la ISO 30A1 pubblicata. Non conteggiata come PASS, limite aperto documentato nei log e nelle note.

**Non verificato:** soluzione del mouse fisico sull'ASRock Q1900M/J1900. L'ultimo riscontro utente è mouse fermo anche in Recovery.

Lo stato del deploy è separato: vedere `github-release-verification.json`. Nessun nuovo rilascio/versione da creare; sostituire solo la release v10 esistente.
