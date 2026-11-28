# Macroblocco 8 — perimetro autorizzato

Richiesta esplicita successiva al PDF e al blocco 7.

1. Riprodurre e misurare cambio tema/sfondo; eliminare lavoro grafico ridondante, mantenendo materiali e risultato finale. Non dedurre fluidità dall’assenza di errori.
2. Campo Spotlight proporzionale, con clic/caret/viewport coerenti. Non modificare l’input hardware.
3. Storage USB 2.0/3.0/3.1 tramite stack canonico e driver nativi adattati; integrare il block layer comune perché i filesystem supportati (TMCFS2, FAT32, NTFS, exFAT) non dipendano dal trasporto AHCI/NVMe. Distinguere test emulati da certificazione fisica SuperSpeedPlus. Nessun driver input alternativo, nessuna modifica agli handler tastiera/mouse/HID.
4. Mount/unmount per volume e in base allo stato, menu contestuale sidebar Finder, pulsante toolbar Monta/Smonta in Utility Disco e comandi nativi Terminale. Flush e protezione volume attivo, gestione FD aperti, isolamento partizioni e fallimenti espliciti; niente finti toggle visivi.
5. hdiutil RAW/IMG/ISO/DMG: definire e verificare le varianti supportate, bounds/RO/RW/attach/detach, montaggio backend effettivo. BaseSystem.dmg deve essere un vero DMG. L’audit iniziale ha già trovato generazione UDIF con footer koly/blkx: verificare indipendentemente prima di attribuire alla nuova implementazione un formato già presente.
6. Aiuto → Scorciatoie: simboli Apple con equivalenti Windows ed elenco verticale, testi essenziali, nome tinymacOS conservato.
7. Workspace rigorosamente sotto 128.000.000 byte (anche sotto 128 MiB): solo compressione lossless di evidenze storiche verificabili, font/upload/input preservati; artefatti e video grandi esterni e su Drive.
8. Alla conclusione: build, host normali/sanitizzati, accettazione grafica/storage/Recovery, aggiornamento versione 10.3, commit/push, backup indipendente e release GitHub con ISO/checksum. Nessuna release anticipata o certificazione hardware implicita.
9. Nuovo video su Drive: soltanto M8, registrazioni reali e cursore nativo; niente testi ridondanti.

## Stato

Priorità aggiornate eseguite prima del resto: revert accento/sfondo, Libreria app senza velo/alone, bordo interno Calcolatrice, barra menu oscurata in Mission Control; screenshot consegnati su Drive. Poi mount/unmount nativo, corretto secondo richiesta più recente: menu Finder, toolbar Utility Disco e comandi, test host/guest PASS. Vedere `TOOLBAR-DOCK-E-BACKUP.md` e `NEXT.md`; `VERIFICA-PARZIALE.md` è storico. `input-baseline.json` registra 473 file canonici/input prima di ogni modifica: tutti ancora identici. Eventuali estensioni successive del trasporto bulk richiederanno audit separato senza modificare handler tastiera/mouse/HID. `workspace-compression.json` documenta 2.764.299 byte recuperati tramite gzip lossless e WebP con RGBA identico, con originali confrontati contro il main pubblicato. Backup Git spostato su Drive dopo verifica; bundle locali rimossi su richiesta, repository attivo conservato. Commit di sicurezza `8053761` già pushato su richiesta esplicita, senza attendere la fine M8. Nessun blocco 8 dichiarato concluso; nessuna release anticipata.
