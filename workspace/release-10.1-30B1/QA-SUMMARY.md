# tinymacOS 10.1 Tramontana · 30B1

## Centro di Controllo
- Composizione modulare ispirata al riferimento: pillole, pulsanti circolari, riquadro con anteprima dello sfondo, due cursori orizzontali e scorciatoie.
- Nessun pannello pieno circostante. Stesso materiale **MAT_DOCK**, con vera convoluzione dello sfondo e rifrazione, non un'immagine sfocata sostitutiva.
- Sei preferenze reali: aspetto chiaro/scuro, tema, sfondo, Dock, movimento e trasparenza. I cursori selezionano gli **8 temi** e i **13 sfondi** esistenti: nessuna luminosità, radio o volume fittizi.
- Simboli di sistema interamente procedurali. Mission Control, blocco sessione, Calcolatrice, Calendario, Terminale e Impostazioni aprono le funzioni/app vere.
- Mouse: clic e trascinamento; tastiera: Tab/Maiusc+Tab, Invio/Spazio, frecce, Home/End, Esc. Geometria condivisa tra disegno e hit test, fallback opaco leggibile.

## Collaudi sulla ISO definitiva
- **16 verifiche** black-box del pannello: etichette nel framebuffer, sfondo libero fra moduli, preferenze, trascinamento/rilascio, frecce, scorciatoie, blocco e sblocco reale.
- Contratti del codice di produzione con **AddressSanitizer e UndefinedBehaviorSanitizer**, geometria a 5 risoluzioni, bersagli disgiunti, gestione fallimenti syscall.
- **29/29 regressioni kernel/runtime**, con KernelCache e kernel.elf identici a quelli di produzione. Un timeout causato dalla pressione di memoria delle VM concorrenti è conservato nel verbale insieme alla ripetizione isolata riuscita; non è stato trasformato in PASS senza riesecuzione.
- **6 profili input reali QEMU**: BIOS/UEFI × PS/2, EHCI e xHCI con tastiera USB dietro hub.
- Installazione GUI BIOS, rimozione ISO, riavvio da disco, persistenza documento e avvio UEFI dello stesso disco.
- KernelCache corrotto su copia sacrificabile: rifiuto, Recovery locale senza ISO, reinstallazione sigillata, documenti preservati e ritorno alla scrivania.
- Tastiera italiana nel Recovery: **2 profili BIOS/UEFI**, versione 10.1/30B1 e caratteri accentati/AltGr verificati.
- Identità verificata nei plist di sistema e app, Terminale, About, installer e Recovery; sigilli/payload controllati da lettori indipendenti.
- **Due build indipendenti byte per byte identiche**. 575 file sorgente congelati e ricontrollati.

## Artefatto
`tinymacOS-10.1-x86_64.iso` · 225,611,776 byte

SHA-256:
```
32e8ba640c4ba45e8df01db56ad69dfe27bed30497c0ea512a2cb51140b00275
```

## Limiti e pubblicazione
I test sono virtuali, non certificano il mouse sul computer ASRock fisico. Safari/WebKit non incluso.

ISO generata e collaudata localmente. **Non ancora sincronizzata né pubblicata su GitHub**: l'autenticazione GitHub CLI della sessione precedente non è disponibile. La release v10 esistente non è stata modificata. Il download temporaneo viene fornito dalla sessione corrente.

## Consegna
ISO presentata direttamente nel visualizzatore file della sessione; SHA-256 ricontrollato anche trasferendo integralmente tutti i byte via HTTP locale. L’anteprima web è protetta dalla piattaforma: il suo URL anonimo non è un link pubblico di distribuzione.
