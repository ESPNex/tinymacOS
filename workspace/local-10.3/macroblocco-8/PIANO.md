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

Priorità aggiornate eseguite prima del resto: revert accento/sfondo, Libreria app senza velo/alone, bordo interno Calcolatrice, barra menu oscurata in Mission Control; screenshot consegnati su Drive. Poi mount/unmount nativo, corretto secondo richiesta più recente: menu Finder, toolbar Utility Disco e comandi, test host/guest PASS. Risolti successivamente anche replay rettangolare del Dock e rifrazioni congelate nelle miniature. Vedere `MISSION-CONTROL-OTTICA.md` e `NEXT.md`; `VERIFICA-PARZIALE.md` è storico. `input-baseline.json` conserva i 473 hash originali: ora 466 file sono identici e 7 estesi soltanto per integrazione storage. Bulk/BOT/SCSI e due guest USB FAT32 xHCI/EHCI PASS, handler tastiera/mouse/HID invariati, audit M0 separato e trasparente senza cambiare hash storici. Vedere `USB8-STORAGE.md` per stato, limiti e matrice USB residua. `workspace-compression.json` documenta 2.764.299 byte recuperati tramite gzip lossless e WebP con RGBA identico, con originali confrontati contro il main pubblicato. Backup Git spostato su Drive dopo verifica; bundle locali rimossi su richiesta, repository attivo conservato. Commit di sicurezza `8053761` già pushato su richiesta esplicita, senza attendere la fine M8. Nessun blocco 8 dichiarato concluso; nessuna release anticipata.


## Aggiunte richieste — da implementare, non ancora avviate

- **Anteprima drag & drop Finder:** sostituire il quadrato generico con la miniatura/immagine dell’elemento trascinato: cartella o file coerente con la sua rappresentazione nel Finder; per file senza anteprima, icona del relativo tipo. Nessuna modifica agli handler hardware input/HID.
- **USB nella sidebar Finder:** mostrare il dispositivo USB e i relativi volumi con stato reale; aggiornare inserimento, distacco ed espulsione, senza lasciare voci obsolete.
- **Espelli nel menu contestuale Finder:** implementare l’espulsione effettiva dell’intero dispositivo USB, distinta da Smonta del singolo volume. Controllare tutti i volumi del dispositivo, rifiutare se occupati/protetti, completare flush e smontaggio prima del detach logico tramite stack canonico. Su errore non mostrare un successo fittizio e non autorizzare la rimozione. Non promettere spegnimento elettrico della porta.
- **Prima dell’avvio:** chiarire all’utente che l’attach/mount generico di immagini RAW/IMG/ISO/DMG e la loro navigazione nel Finder non sono ancora implementati. BaseSystem.dmg UDIF esistente riguarda il boot e non dimostra questa funzionalità. Attendere il seguito dell’utente prima dell’implementazione di queste aggiunte.


## Correzione successiva — immagini grandi e DMG compressi, prima dei nuovi test

Richiesta utente: supporto DMG inclusi compressi, ISO9660, RAW e IMG, anche oltre 128 MB; eliminare il limite provvisorio di 64 MiB. Non avviare ulteriori test prima dell’implementazione di questa estensione.

- [ ] Sostituire il caricamento completo in RAM con un backend file-backed a lettura posizionale e cache limitata; offset/capacità a 64 bit, senza limite artificiale a 64/128 MiB. Conservare bounds e protezione del file sorgente durante attach/detach.
- [ ] Decodifica on-demand dei blocchi DMG compressi; selezionare e documentare i codec effettivamente implementati, senza dichiarare «tutti» sulla sola base di zlib.
- [ ] ISO9660, RAW e IMG devono utilizzare lo stesso backend per immagini grandi, non un percorso separato ancora limitato a 64 MiB.
- [ ] Chiarire se «tutti i DMG» include anche filesystem Apple HFS/HFS+/APFS e DMG cifrati/password, oltre al contenitore e ai codec. Nessun driver per tali filesystem o cifratura è attualmente implementato; non confondere l’apertura del contenitore con la navigazione del volume.
- [ ] Il limite workspace sotto 128.000.000 byte resta valido: immagini grandi, build e fixture vanno fuori dal workspace.

Stato: requisiti registrati; il codice provvisorio conserva ancora il limite 64 MiB e il sottoinsieme UDIF raw/zero. Non è ancora conforme alla nuova richiesta. Nessuna chiusura M8 o release/deploy finale autorizzata dai risultati precedenti.


## Perimetro immagini confermato — scelta apple_fs

L’utente ha scelto anche **HFS, HFS+ e APFS in sola lettura**, oltre ai contenitori RAW/IMG/ISO9660/DMG compressi e >128 MB. Cifratura/password escluse. Nuovi test sospesi fino all’implementazione dell’estensione. Il precedente backend snapshot da 64 MiB non è accettabile: sostituzione con lettura posizionale, cache limitata, pinning della sorgente e protezione dalle mutazioni. Distinguere riconoscimento della firma, decodifica del contenitore e reale navigazione dei file. Nessuna dichiarazione di supporto universale, release o pubblicazione anticipata.


## Checkpoint successivo — estensione scritta, non ancora accettata

Vedere `IMMAGINI-ESTENSIONE.md` per stato corrente e limiti effettivi. Il vecchio snapshot da 64 MiB è stato sostituito nel codice con file backing nativo/bootfs a 64 bit, cache limitate e pin sorgente; aggiunti multi-blkx, ADC/zlib/bzip2/LZFSE/XZ-LZMA2, HFS/HFS+/HFSX, APFS v2 RO iniziale e Apple Partition Map RO. Compilate soltanto 12 unità e dipendenze decode-only (log esterno `compile-only.log`), senza diagnostiche: **nessun nuovo test, sanitizer, guest, link/build ISO, commit o push**. Non supporto universale: restano sorgenti esterne, APFS sealed/compressione file/Unicode completo, varianti UDIF non XML/segmenti e limiti decoder da completare/valutare prima dei test. I PASS storici non verificano questo codice. Main e ISO pubblicati invariati. Input/HID non modificato.


## Rimozione limitazioni — checklist corrente

La richiesta successiva è rimuovere tutte le limitazioni una per una, prima del test finale e del commit. Lo stato aggiornato, distinguendo codice compilato e funzionalità ancora non implementate, è in `IMMAGINI-ESTENSIONE.md`; il checkpoint precedente sulle sorgenti esterne e i limiti numerici è superato. Main pubblicato invariato. Stack sorgente dedicati e nuovo allocatore richiedono verifica; il link incrementale non è un test né una build finale. Le voci residue non sono state escluse dal perimetro: rimangono da implementare.
