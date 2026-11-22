**Build 30B7 · pulizia interfaccia e icone.**

## Correzioni
- Rimossi «Cambia sfondo» e «Cambia tema» dal menu contestuale della Scrivania e le voci equivalenti da Vista. Tema e sfondo restano disponibili nel Centro di Controllo e nelle Impostazioni.
- Eliminati richiami generici alle Impostazioni nei menu contestuali di barra e Dock; alleggerite le intestazioni ripetute in Finder, Impostazioni, Info e Utility Disco.
- Applicazioni nel Finder usa l'asset App Store fornito, datato 2026-09-23, anche nella barra laterale.
- Utility Disco in Recovery usa la stessa icona importata dell'OS, non il vecchio simbolo procedurale.

## Verifiche
Due build indipendenti identiche; confronti delle risorse estratte dalla ISO e dei pixel delle nuove icone. Rieseguite le prove di interfaccia, installazione BIOS/UEFI, input, ripristino offline e kernel. Driver USB/PS2 invariati rispetto alla 10.1.5; conservati vetro e SVG originali del Centro di Controllo.

## Limiti
L'utente conferma avvio e USB funzionanti in VirtualBox con la 10.1.5. **La 10.1.6 non è ancora verificata sulla ASRock Q1900M/J1900 fisica:** il risultato virtuale non garantisce quello reale. La 10.1.4 su quella macchina aveva il mouse immobile. Safari/WebKit non incluso.

Due controlli QEMU (trascinamento con hub e tastiera Recovery PS/2) sono passati dopo una ripetizione; cause non accertate ed evidenze conservate.

ISO ottica BIOS/UEFI, non USB-DD e non aggiornamento automatico di un sistema installato. Per l'installer usare solo dischi virtuali vuoti dedicati.

Asset: `tinymacOS-10.1.6-x86_64.iso` e `SHA256SUMS.txt`.
