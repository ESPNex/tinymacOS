# Macroblocco 7 — perimetro autorizzato

Estensione esplicita del piano PDF (che termina al blocco 6), confermata dall’utente in conversazione.

1. Integrare il pacchetto Cursors.zip fornito su Drive: cursori contestuali, hotspot corretti, animazioni ANI. Le risorse devono essere file reali in live, installato e Recovery, non solo sprite incorporati.
2. Supportare CUR e ANI nel motore nativo, con parsing limitato e rifiuto sicuro dei dati non supportati/corrotti.
3. Ridimensionamento fluido da bordi e angoli; verificare e correggere il trascinamento. Limitare IPC, riallocazioni e rigenerazione del vetro durante il gesto, senza alterare Liquid Glass a riposo.
4. Testo proporzionale di sistema nel documento di TextEdit, Note e Memo: wrapping, caret e hit testing devono usare gli stessi avanzamenti reali del renderer. Terminale e campi tecnici restano distinti.
5. Conservare regressioni dei blocchi precedenti: sicurezza, TMCFS2-only e rifiuto legacy, scorciatoie, input, desktop, unità SI, finestre di apertura/salvataggio e Calcolatrice.

## Stato

Implementazione e accettazione locale completate. Build finale, foundation normale+sanitizzata, M0, tutte le 12 famiglie storage M2, volumi sigillati e guest reali live/installato/Recovery PASS. Dettagli, limiti e tentativi falliti in `VERIFICA.md`. Pubblicazione del checkpoint Git separata dal rilascio pubblico; nessuna certificazione hardware.

## Risorse e limiti dichiarati

- 15 CUR e 2 ANI forniti dall’utente. Importazione lossless della rappresentazione originale a 32 px, conservando bitmap, hotspot e ordine/tempi dei frame. Le risoluzioni inutilizzate sono eliminate; nessun ridisegno.
- File di sistema: `/System/Library/Cursors/`, stesso set per le tre modalità.
- Motore CUR: DIB non compressi 1/4/8/24/32 bit, trasparenza AND/alpha e inversione monocromatica. Selezione 32 px con campionamento limitato fino a 128 px; CUR con sole immagini PNG o compressioni non supportate vengono rifiutati, non interpretati come DIB.
- Motore ANI: RIFF ACON con frame CUR, `anih`, sequenza opzionale e tempi individuali, massimo 64 frame/256 passi. Animazione su clock monotono OS 100 Hz con unità ANI 60 Hz, senza arrotondamento cumulativo; Riduci movimento ferma il primo frame.
- Eventi contestuali enter/leave/update gestiti localmente da WindowServer e regioni `C_CURSOR` pubblicate insieme alla transazione grafica; niente IPC continuo per il solo hover.
- I cursori che rappresentano capacità non implementate non creano finti comandi: le risorse rimangono disponibili al motore.
