# tinymacOS 10.2 Tramontana · build 30B11

## Interfaccia

- Rimossa l'azione «Mostra nel Finder» dai dialoghi Apri/Salva. Restano navigazione reale delle cartelle, nuova cartella, apertura e salvataggio.
- Data di login con spaziatura esplicita tra giorno della settimana, numero e mese, indipendente dalla metrica dello spazio del font. Font accettato invariato.
- Backup in Impostazioni: nuovo tracciato SVG continuo con freccia e orologio leggibili.
- Liquid Glass dei dialoghi esteso anche all'interno: campionamento della scena sotto la finestra anziché del foglio opaco già disegnato, tinta più traslucida e rifrazione su tutta la superficie. La modalità grafica ridotta resta opaca intenzionalmente.
- Data e ora in Impostazioni mostrate una sola volta; conservata la riga della sorgente.

## Recovery: ricerca storica e correzioni

Riferimento hardware riportato dall'utente: **2.4.5 Levante / 21B9**, tag `v2.4.5`, commit `bdf6837e3cc6a1ebfa56c4f7647795aceb1fadd7`. In Recovery il mouse funzionava; il login aveva invece movimento soltanto verticale. Le note `HISTORY-AND-CHANGES.md` delle release 10.1.2, 10.1.3 e 10.1.5 conservano questo riscontro. Non si tratta di una nuova prova fisica della vecchia ISO.

Sequenza successiva documentata: 10.1.2 movimento breve poi blocco; 10.1.3 solo verticale; 10.1.4 fermo. La 10.1.5 aveva rimosso il takeover delle interfacce possedute dal firmware. Il problema sulla ASRock Q1900M/J1900 è stato segnalato ancora con la 10.1.9, pur dopo prove brevi riuscite in QEMU.

Correzioni 10.2:

1. Nel trasporto USB diretto, una raffica di callback d'errore non può più riportare continuamente a otto il conto alla rovescia, impedendo per sempre cancel/clear/rearm. Riprodotto e verificato con il sorgente reale e callback ripetute.
2. Se il firmware ha già ritirato il trasferimento, `EFI_NOT_FOUND`/`EFI_NOT_STARTED` alla cancellazione non impediscono il successivo riarmo.
3. Conservata la normalizzazione SimplePointer: il tentativo di ritorno integrale ai delta grezzi 21B9 è stato scartato perché riproduce un puntatore inchiodato al bordo con OVMF (8192 conteggi per passo, Resolution 65536). Il fallback per Resolution assente resta otto. Non cambia lo scaling del desktop nativo.
4. Un AbsolutePointer con Mode assente/non valido, o che continua a cambiare, non esclude più il polling dei mouse relativi.
5. Reset morbido iniziale solo dei protocolli appena scoperti; in seguito solo dopo errori espliciti ripetuti e con backoff. `EFI_NOT_READY` è inattività normale e non causa reset. Il Reset di UsbMouseDxe non viene spacciato per riparazione dell'endpoint: nell'implementazione EDK2 esaminata azzera solo lo stato.

Nessun distacco forzato di mouse/tastiera posseduti dal firmware, hub o controller. UsbMouseDxe resta disponibile per i firmware privi di SimplePointer; il suo precedente esperimento disabilitato è documentato nelle note storiche, non assunto come prova della causa attuale.

## Validazione e limite hardware

Contratti host su sorgenti effettivi e prove guest sulla nuova ISO sono registrati separatamente. I test includono raffiche d'errore, riarmo, sorgenti assolute non valide/concorrenti, compatibilità Resolution nulla e conteggi normalizzati OVMF, inattività e movimento prolungato. Il successo in emulazione **non certifica** la ASRock Q1900M/J1900: serve conferma fisica con la 10.2. Non attribuiamo alla scheda un guasto specifico senza una traccia acquisita su di essa.

La ISO è un supporto **ottico BIOS/UEFI**, non un'immagine USB-DD. Sul desktop installato il cambio di risoluzione richiede riavvio.
