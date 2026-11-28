# M8 — correzioni prioritarie e mount/unmount

**Checkpoint storico prima della correzione toolbar. M8 non concluso.** Il seguito autorevole è `TOOLBAR-DOCK-E-BACKUP.md`: commit di sicurezza pushato, backup Git spostato su Drive, Monta/Smonta ora nella toolbar di Utility Disco (non nel suo menu contestuale). `VERIFICA-PARZIALE.md` è anch’esso storico. Le prove qui sotto si riferiscono alla precedente ISO.

## Ordine richiesto e rispettato

Prima revert accento e fix Libreria/Calcolatrice/Mission Control, poi test/screenshot reali e upload Drive verificato; **solo dopo** implementazione del mount/unmount nativo.

### Priorità grafiche

- Ripristinato il percorso originale nel loop desktop per cambio accento/tema/sfondo con transizione. Il mapping kernel `theme_wallpaper()` non era stato rimosso ed è rimasto invariato. Conservata solo l’ottimizzazione dello scaling X, già verificata pixel-identica.
- Libreria app: rimosso il velo scuro esterno e l’ombra propria. Dopo una prima verifica visiva rimaneva un bordo scuro generato dalla rifrazione: introdotto materiale dedicato frosted (MAT_LIBRARY) senza bordo a lente/rim/shine, preservando blur e tinta del vero sfondo. Seconda build e nuova acquisizione verificate prima dell’upload. Gli altri materiali non sono cambiati.
- Calcolatrice: rimosso esclusivamente lo Sheet full-bleed del tastierino, seconda lente che creava un bordo interno rialzato in basso a destra. Nessuna modifica al vetro della Cronologia o alla cornice esterna. Test della Draw list aggiornato per richiedere l’assenza della lente duplicata e conservare posizione/raggio/inset della Cronologia; aritmetica, ripetizione e divisione per zero passano.
- Mission Control: barra dei menu oscurata dopo la composizione della barra, una sola volta con lo stesso mcdim del desktop. Le miniature non vengono oscurate in aggiunta. Pixel del campione barra coincidono al 100% con blend 112/256; ritorno ai valori originali all’uscita verificato.

`tests/priority8_guest.py`: PASS, input QMP reale, screenshot nativi senza sovrapposizioni. Libreria: regione esterna e striscia dell’ombra 100% invariate. Accento: cambia realmente lo sfondo. Test normali/ASan/UBSan: apps5, appearance8, animation-performance PASS.

### Screenshot consegnati su Drive

Cartella: https://drive.google.com/drive/folders/13rcn8izgQOjVSbgxALro1ftP9oqwmj3R

1. Calcolatrice: https://drive.google.com/file/d/1gGqb3XJ3astUEDMnDg0BJPFOhetWqA3Y/view
2. Libreria app: https://drive.google.com/file/d/1q42hroh6sUDlR0wVyGqqliq6oKZXFrx3/view
3. Mission Control: https://drive.google.com/file/d/16qAjcUPVuUULIBQR2vzHh7Us4QXUQtLl/view
4. Accento/sfondo: https://drive.google.com/file/d/1Me5qzfyxZMBYVeu0jgX32wluVFECOH1o/view

1280×800, frame integrali reali, JPEG senza ritocchi (sola ricodifica per rimanere sotto il limite workspace durante l’upload). Manifest con SHA PNG sorgente, JPEG, qualità e MD5 remoto: `priority-screenshots-drive.json`. Ogni upload ha restituito dimensione e MD5 identici ai file preparati; staging locale eliminato solo dopo la verifica. ISO degli screenshot: SHA-256 `d26464abef13a101fd04244fedb14c86b0cbc769c5156fa530a8fffcd65cafed`. Quel candidato è stato successivamente sostituito, nello stesso percorso scratch esterno, dalla build che integra mount/unmount; non è una release pubblica.

## Prosecuzione: mount/unmount implementato

- Backend `fs/volumes.c`, serializzato tramite `fs_gate` nel wrapper di namespace.
- API autenticata S_DISK 15 con struttura delimitata, operazioni query/mount/unmount; app autorizzate Finder, Utility Disco e soltanto i programmi nativi mount/umount/diskutil. Nessuna modifica al gate pre-authentication.
- I volumi dati restano nell’inventario dopo unmount, ma l’intero namespace relativo rifiuta letture/scritture/creazione/rimozione; controlli aggiuntivi al confine block I/O e sui descrittori.
- Unmount rifiutato se qualunque processo conserva FD dello stesso volume; flush prima di chiudere il backend; errore flush lascia volume e stato intatti. Nessun force-unmount. Volume di avvio e ruoli esclusi non diventano gestibili.
- Nessun refresh globale/generation bump per montare/smontare un volume: i FD degli altri volumi restano validi. Rifiutare una rimozione su volume offline non invalida gli altri FD. L’eventuale rescan esplicito conserva lo stato offline per la stessa geometria/formato; disconnessione/reinserimento USB richiederà anche identità/generazione del futuro trasporto.
- Rimontaggio verifica il backend TMCFS2 o la radice FAT32/exFAT/NTFS; allocazione/validazione fallita mantiene offline.
- Comandi `/sbin/mount`, `/sbin/umount`, `diskutil mount/unmount`; nessuna simulazione o rimando a mount_apfs. `mount` senza argomenti elenca stato. Guida `tinymacOS/docs/VOLUMI.md`.
- Finder: clic destro sulla sidebar del volume, senza dover entrare nel volume offline; sola azione coerente con query corrente. Utility Disco: stessa operazione sui volumi/partizioni dati, con identificazione del ruolo dati esterno. Il kernel rivalida lo stato all’esecuzione. Context menu esteso con azioni aggiunte in coda all’ABI, senza rinumerare quelle esistenti; handshake owner/nonce mantenuto.

### Test mount

`tests/test-foreignfs.sh`: PASS normale + ASan/UBSan su filesystem veri FAT32, NTFS, exFAT, TMCFS2. Test su FD occupati, flush fallito, offline/rimontaggio, rollback allocazione, FD altro volume, tentativo rimozione offline, rescan ripetuto, volume attivo e stringa non terminata. Regressioni RW/export e fsck indipendenti PASS.

`tests/mount8_guest.py`: PASS retail live, partizione FAT32 disposable su NVMe. Esegue comandi Terminale, verifica cambio stato e lettura dopo remount; clic destro Finder Smonta→Monta; clic destro Utility Disco Smonta→Monta; lettura finale dello stesso file. **Non è un test storage USB.** SHA-256 di tutto il disco di prova prima/dopo identica: `39f2caebf21ced8791f478e1b147917b242981c55ccf875a4ed731d713fefa6c`; directory riletta anche con mtools.

Build integrata live/Recovery PASS. ISO: `/var/tmp/tmc-block8-candidate/tmc-block8-candidate.iso`, SHA-256 `56ffa99cf38f341bce9c4522b487c3c5f44625a394c6f7ff40120e16df0e8a21`. Font/cursori/shortcut: test-m0 (568 risorse protette), cursor7, desktop4, guard pre-authentication PASS. Tutti i **473 file USB/input** della baseline M8 ancora byte-identici.

BaseSystem estratto nuovamente dalla build integrata e decodificato da qemu-img: UDIF riconosciuto, 32 extent raw, FAT16 e RECOVERY.ELF/RECOVERY.VOL verificati; SHA `9b2f7df24452e9a13b9081fbc3aff514d1bd5c8e57f7678bdacf97c4d9bbd06b`. Questo non completa hdiutil.

## Ancora aperto

Storage USB canonico e relativi test (bulk/BOT/SCSI/registrazione block layer/hotplug/settori/flush, senza regressioni input); hdiutil RAW/IMG/ISO/DMG; accettazione integrata finale, inclusa Recovery; misure finali fluidità; versione/release GitHub 10.3 con ISO/checksum, commit/push e backup; video M8-only su Drive. Nessuna certificazione hardware fisico implicita.

Workspace ~127,75 MB decimali, sotto 128.000.000 byte; backup Git completo invariato. Log voluminosi compressi losslessly. Nessun nuovo ISO/disco/video nel workspace. Vedere `NEXT.md` per l’ordine corrente.
