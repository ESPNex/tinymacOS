# Prossima release integrata — istruzione successiva alla 2.4.2

La successiva eccezione esplicita ha autorizzato una ultima manutenzione 2.4.3, ora caricata e verificata. Dopo questa, nessun’altra ISO intermedia da caricare. Una release con correzioni password/USB, SMP reale e Safari funzionante, dopo i test.

## Password
- L'utente rifiuta i puntini più distanziati: ripristinato nel sorgente desktop/login.c passo 9 px (prima manutenzione 12 px), max 19; non ancora compilato/testato.
- Screenshot fisico: evidence/user-password-white-line.png. Banda orizzontale bianca visibile sopra/sul placeholder password; non dimostra che siano i puntini (il placeholder è ancora visibile). Non attribuire la causa senza riproduzione.
- Servono regressione sul renderer/framebuffer e screenshot a campo vuoto/pieno, focus/blink e risoluzioni, preservando puntini compatti. La modifica della spaziatura NON è un fix dimostrato della banda.

## Ordine tecnico
1. Riprodurre/diagnosticare il difetto password dal nuovo screenshot.
2. Invalidazione TLB SMP e lifetime pagine: eliminare deadlock lock/IPI e non liberare backing prima degli acknowledgement. Test reali multi-CPU, non ENOTSUP spacciato per supporto.
3. Runtime libc/C++, pthread/TLS/errno, file e rete; casualità sicura e TLS con verifiche negative.
4. Build target WTF/JSC e WebCore, rendering/input/font/event loop.
5. Safari classico, navigazione/schede/download e tutti i temi, collegato al motore reale.
6. Test HTTPS/HTML/CSS/JS, SMP e regressioni USB/password/installazione; sorgenti salvati prima di ogni build. Solo dopo build release e upload Drive.

Stato: Safari non completo, nessuna nuova ISO di release costruita o caricata in questo blocco. Non promettere compatibilità universale né completamento in background.

Manutenzione 2.4.3 completata: vedi ../maintenance-2.4.3/CONSEGNA.md. Sviluppo SMP avviato dopo upload: ../smp-next/STATUS.md.
