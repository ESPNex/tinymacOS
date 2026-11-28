# M8 — correzione rifrazioni Mission Control e fascia sopra il Dock

**Checkpoint parziale, M8 resta aperto.** Segnalazione dell’utente: `uploads/image-4.png`. Questa correzione supera la verifica precedente del solo Dock luminoso: quel test controllava le icone, non escludeva né il rettangolo di sfondo ricopiato né le rifrazioni delle vecchie posizioni.

## Due problemi distinti corretti

1. **Dock:** `chrome(1)` conservava vetro, ombra e pixel grezzi di un rettangolo di padding. Riprodurre quel blocco può sovrascrivere la porzione sottostante del Calendario con una scena precedente. Il piccolo materiale del Dock e la sua ombra ora vengono composti sul fondale corrente, senza riprodurre un rettangolo di pixel di finestre. Vale anche per Dock laterale, chiaro/scuro e Riduci trasparenza. Non si copre la discrepanza con un’ombra o un pannello opaco.
2. **Finestre Mission Control:** la surface era prodotta alle coordinate native e includeva già sfocatura/rifrazione delle finestre allora dietro. `AN_MC` congelava espressamente quella texture. Ora viene catturata la scena già composta alla **destinazione corrente** della finestra e rimappata allo spazio logico del client prima di eseguire il vero `wincontent()`. Anche i fogli interni campionano quel fondale, non altre app nelle vecchie coordinate. Contenuti e testo rimangono quelli del client, con il suo layout normale, poi ridimensionati nella miniatura.

La cattura precede qualsiasi scrittura anche quando origine e destinazione si sovrappongono; il rettangolo scratch viene ripristinato esattamente. Usato il buffer sheet già esistente, halo di 32 pixel, mappa X precalcolata per evitare divisioni per ogni pixel. Nessun nuovo buffer per ciascuna finestra, nessuna modifica input/driver.

Le chiavi includono geometria della destinazione e scena effettiva; niente vecchio riuso congelato AN_MC. Corretto anche il percorso di uscita da Mission Control, evitando sovrapposizioni di texture precedenti e segni hover alle vecchie coordinate. Il normale trascinamento fuori da Mission Control mantiene la politica di cache precedente.

## Verifiche definitive

ISO candidata SHA-256: `81dff498571f4f21f9741a650d8d072268076a47832f80d824bb114ab55b5d82`.

- Build finale exit 0; `u-glass.c.o` successivo all’ultima modifica sorgente.
- `test-mc8-optics.sh`: PASS normale + ASan/UBSan. Usa **wincontent/cache/sprite/glass reali**, non un disegno alternativo. Cambiare i colori delle app fantasma alla vecchia posizione ha effetto nullo sulla nuova surface; cambiare il vero fondale alla destinazione cambia vetro e sheet interno; contenuto opaco conservato. Verificati chiaro/scuro, Riduci trasparenza, chiave invariata con destinazione diversa, origine/destinazione sovrapposte, clipping e halo ai bordi, ritorno alle ottiche normali.
- Test Dock con helper `chrome()` e renderer reali: una scena luminosa precedente, poi scura, con identica chiave di cache, produce esattamente i pixel di una composizione fresca shadow+glass; nessun replay rettangolare. Dock in basso e laterale, chiaro/scuro/Riduci trasparenza.
- `mc8_optics_guest.py`: PASS VM retail, Finder + Calcolatrice + Calendario. Regione Calendario/Dock invariata al **100%** fra due composizioni stabili. Poi Finder spostato realmente tramite titolo sul desktop (69,98% di pixel cambiati nella regione di controllo), ordine/focus ripristinati e Mission Control riaperto: regioni Calcolatrice e Calendario **identiche al 100%**, indipendentemente dalla vecchia posizione Finder. Uscita e finestre native verificate.
- `priority8_guest.py`: PASS sulla nuova ISO: Calcolatrice 7+5=12, Libreria senza velo/ombra, cambio accento→sfondo originale, blend barra menu e nuclei opachi icone Dock esatti al 100%.
- Regressioni host normali/sanitizzate: animation-performance, appearance8, apps5, desktop4, cursor7 PASS. Il confronto shader ottimizzato/reference conserva 88 frame bit-identici per modalità; i tempi host non sono una misura FPS del sistema.
- M0 e inventario UI rigenerato dopo revisione: PASS. Tutti i **473** file della baseline USB/input ancora byte-identici.

Processi definitivi: build e due guest exit 0; suite regressioni finale exit 0. Log salvati in questo percorso (`mc8-optics-*.log`, `mc8-optics-regressions-final.log.gz`, `build-mc8-optics-final.log.gz`). Il primo tentativo della suite richiedeva solo di rinominare anche la nuova API nel generatore della reference shader, per evitare simboli duplicati; la suite definitiva è stata rieseguita integralmente.

## Evidenza

`evidence/mission-control-vetro-corretto.png`: frame PNG nativo completo, senza ritocchi, crop, ricodifica o cursore sintetico.

Drive: https://drive.google.com/file/d/1Mk-PyBtrKhOPX8oTMyIJC7g_3775fVNj/view

792.969 byte; MD5 `09346b55b450882efd70fa388f58281a`; SHA-256 locale `8071a81d639642ae5606069078a74892b6a9bdcdab9bb49b8bb647eecf076810`. Dimensione e MD5 restituiti da Drive identici al file locale; ricevuta `mc8-optics-screenshot-drive.json`.

## Stato restante

Nessuna modifica ai volumi, USB/HID o gate preautenticazione. Nessun nuovo backup Git pesante nel workspace. Fix della toolbar precedente conservato. Nessuna release/version bump/video finale M8: storage USB canonico, hdiutil e accettazione finale rimangono da completare (`NEXT.md`). Le prove runtime qui sono live/VM, non una nuova certificazione hardware, installato o Recovery.
