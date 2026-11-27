# Macroblocco 1 — registro di lavorazione

**Stato: IN CORSO, NON COMPLETO. Non autorizza il passaggio al macroblocco 2.**

Riferimento esclusivo: [`piano.pdf`](../../../tinymacOS/docs/piano.pdf), fornito dall’utente, SHA-256 `fe66291ae6368af7abbec48877f4086ed4c76180469c2e87873abd6aa175a86f`. Il PDF è copiato senza alterarne un byte o il titolo «Changelog 10.2.7 – Piano di Sviluppo». Il target 10.3 precedentemente confermato non viene cambiato dal titolo del documento. Non è stato scritto un altro piano.

Il vecchio `tinymacOS/docs/PIANO-10.3.md` è eliminato. I vecchi identificatori M0–M7 nelle ricevute conservate sono storici: **M3 non significa macroblocco 3 di questo PDF**. Non trasferire automaticamente i loro verdetti ai sei macroblocchi richiesti ora.

## Operazioni già eseguite

- Fermato il test prestazionale incompleto del vecchio M4. Nessun risultato baseline/after utilizzabile, nessuna ottimizzazione certificata.
- Conservate patch e aggiunte incompiute in `../m4-paused/`; rimosse dalla compilazione attiva. Non perdute, non accettate, non pubblicizzate come funzionalità.
- Ripartenza funzionale dal checkpoint `1b545caba4abb34145d37d088866eb77e3149129`; nessun cambio di font, USB canonico, cifratura, partizioni o versione pubblica.
- Trovato un filtro reale del loader: richiedeva larghezze GOP multiple di otto, benché sia un limite del DISPI emulato, non del protocollo UEFI GOP. Il filtro escludeva ad esempio una modalità 1366×768 eventualmente offerta dal firmware.
- Corretto quel filtro, mantenendo i limiti reali dei buffer (massimo 1920×1080). Accettate anche le maschere RGB/BGR a 32 bit equivalenti ai due layout già supportati; rifiutati BLT-only, maschere arbitrarie, stride invalido e framebuffer troppo piccolo/fuori mappa. Nessuna modalità viene inventata o forzata sul firmware.
- Estesa la codifica della larghezza in BootConfig usando tre bit precedentemente riservati: le vecchie modalità allineate mantengono gli stessi byte; le larghezze come 1366 non vengono più arrotondate a 1360 al riavvio. Il percorso DISPI immediato resta limitato alle sue larghezze allineate.

## Verifica dei quattro punti del primo macroblocco

| Punto del PDF | Evidenza e stato attuale |
|---|---|
| Bootloader | Il vero OpenCore 1.0.8 è già incluso nei supporti; il loader registra una voce dedicata prima del precedente BootOrder senza sovrascrivere le variabili Microsoft e salva l’ordine originale. Test host normale/ASan/UBSan dell’algoritmo EFI rieseguiti: PASS (`boot-default-host.log`). Questo non dimostra il chainload di una vera installazione Windows, né firmware fisico o rollback end-to-end. Non dichiarato definitivamente chiuso. |
| Partizionamento | La baseline possiede già full-gap FS2 v3 e geometria a 64 bit. Le ricevute storiche `../m2/` e `../m3/regression-m2-capacity-4096-result.json` dimostrano casi guest 3 TiB/4Kn, gap da 2,5 TiB e preservazione di partizioni estranee sintetiche. Rieseguiti inoltre test host mirati GPT/MBR, gap Windows, installazione e grandi dischi/16 TiB, normali e ASan/UBSan: PASS (`windows-gaps-host.log`, `partitions-host.log`, `large-disks-host.log`). Non si nasconde un gap reale, non si tratta RAW come libero, non si assorbono partizioni altrui. Quelle prove appartengono alle ISO indicate nelle ricevute, non automaticamente alla nuova build. |
| Risoluzione GPU | Nuova correzione del filtro GOP e della persistenza verificata da test host normali/ASan/UBSan (`gop-host.log`) e build completa. **Non dimostra** che il GOP della GeForce 310 esponga altre modalità, né risolve da solo un eventuale limite del firmware. GeForce/HDMI/monitor reali non collaudati. Punto aperto. |
| Cartella Utente | Il nome account è già mostrato nella sidebar e in alcuni nomi Finder. Però `HOME` e la vera home restano `/Users/User`, visibile anche nei percorsi. Non è corretto contare la sola etichetta come rinomina effettiva della cartella. La migrazione sicura del nome reale, con compatibilità di app, snapshot, installazione e cifratura, **non è implementata in questo intervento**. Punto aperto. |

## Limiti del registro

Questo file documenta lavoro e risultati, non sostituisce o riorganizza il PDF e non è un secondo piano. I log dei test elencano esattamente ciò che è stato eseguito; build, test host, guest virtuale e prova sul PC restano livelli distinti. Nessuna release/tag 10.3, nessuna certificazione Windows/GeForce, nessun passaggio ai macroblocchi 2–6.

## Risultati di questa esecuzione

- Build BIOS/UEFI/Recovery completa: **PASS**, `build.log` e `artifacts.json`. ISO di sviluppo SHA-256 `9ec7a4dae20fade346b86a59b50fe6ad1f37a7d8407063bf7bd70f0b7be2fc6e`; non distribuita come release o blocco completo.
- Guest sulla stessa ISO: **PASS** test mirato USB/pixel di setup, etichetta account, impostazioni schermo e accesso alla Recovery. Ricevuta `uefi-recovery-result.json`, log `uefi-recovery-guest.log`. È il test storico `m0_ui_guest.py` riutilizzato come smoke test: non una nuova accettazione completa del vecchio M0 o del macroblocco 1.
- Host GOP/BootConfig e variabili EFI OpenCore: **PASS** normale + ASan/UBSan.
- Regressioni mirate partizioni/gap/limiti grandi dischi: **PASS** normale + ASan/UBSan. Non è una nuova installazione GUI sulla ISO corrente.
- Handoff grafico boot→desktop, input protetti, font forniti e overlay: **PASS**, log separati nella presente directory.
- Le prove di riesame avviate sul vecchio checkpoint sotto `../revisione-foto/` sono precedenti a questo riavvio del lavoro: foundation conclusa, sequenza M2 interrotta e non dichiarata PASS complessiva. Non sostituiscono i risultati espliciti qui sopra.

**Verdetto finale di questa lavorazione: primo macroblocco NON COMPLETO; nessun macroblocco successivo iniziato secondo il PDF.** Il commit di avanzamento non è una chiusura della milestone. Nessun processo di build/test resta attivo al termine delle verifiche.
