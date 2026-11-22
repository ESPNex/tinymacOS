**Build 30B6 · manutenzione input.**

## Correzioni
- Mouse Boot: negoziazione Boot prima di Report, senza dedurre il formato effettivo dalla sola disponibilità del descriptor.
- Accettati i frame Boot a otto byte e i report numerati con padding zero nelle forme supportate; restano esclusi frame incompleti e ID estranei.
- Recovery non disconnette più il proprietario firmware del mouse; USB I/O diretto limitato alle interfacce non reclamate.
- EHCI non ferma la schedule periodica sui poll senza dati.
- Launcher QA corretti: media e KernelCache verificati contro la produzione prima delle prove guest.

## Verifiche
Due build indipendenti identiche; gate di 80 ricevute, 27 profili input live/installato con 236 asserzioni, 18 avvii Recovery e 29 regressioni kernel. Undici nuovi controlli negativi sul codice 10.1.4. Conservati SVG, vetro e funzioni del Centro di Controllo.

## Problemi noti
**Non ancora verificata sulla ASRock Q1900M/J1900 fisica.** La 10.1.4 aveva il mouse immobile in entrambi gli ambienti; queste correzioni software non dimostrano la soluzione su quel PC. Restano limiti per report HID ambigui. Safari/WebKit non incluso.

ISO ottica BIOS/UEFI, non USB-DD e non aggiornamento automatico di un sistema installato. Per l'installer usare solo dischi virtuali vuoti dedicati.

Asset: `tinymacOS-10.1.5-x86_64.iso` e `SHA256SUMS.txt`.
