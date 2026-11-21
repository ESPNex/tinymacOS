# 10.1.2 Tramontana / 30B3 — confronto con Levante 21B9

## Riscontro hardware dell’utente
L’utente identifica esplicitamente **tinymacOS 2.4.5 Levante — revisione 21B9** come riferimento: Recovery con mouse funzionante; al login il puntatore si muoveva soltanto in verticale. Nella 10.1.1 il mouse non funziona più in nessuno dei due ambienti. Questo nuovo riscontro sostituisce la precedente incertezza sul nome «mouse fix». Non è una nuova prova remota dell’hardware.

Riferimento sorgente verificato: tag `v2.4.5`, commit `bdf6837e3cc6a1ebfa56c4f7647795aceb1fadd7`. Nessun tag storico è stato spostato. Confrontati driver, Recovery, boot, firmware vendor e strumenti ISO. La ISO 21B9 non è stata sostituita o ricompilata.

## Correzioni effettive
1. **Routing nativo USB2 all’avvio:** ripristinata la politica 21B9 per le famiglie Intel switchable già supportate: abilitazione USB3 e instradamento dei soli bit USB2 documentati verso xHCI già avviato, prima dell’enumerazione. La sola presenza di EHCI non blocca più il routing iniziale. Nessuna regola basata sul nome ASRock, nessun remux di endpoint HID già attivi. Il percorso EHCI/UHCI e PS/2 resta implementato e collaudato; controller sconosciuti e registri invalidi non ricevono scritture.
2. **Modalità HID coerente con i pacchetti:** xHCI, USB2 e Recovery controllano GET_PROTOCOL anche dopo SET_PROTOCOL riuscito. Una risposta esplicita Boot non può più essere ignorata per decodificare i byte con offset Report/Report ID. Conservati il parser descriptor-first, X/Y indipendenti, Report ID, 16 bit, pulsanti e rotella. Se GET_PROTOCOL non è supportato, resta valido un SET riuscito; risposte brevi, contraddittorie o non confermate non vengono assunte come formato valido.
3. **Recovery e puntatori firmware inattivi:** un AbsolutePointer che restituisce continuamente EFI_SUCCESS con le stesse coordinate/pulsanti non esclude più gli eventi freschi del SimplePointer USB. Il confronto è con lo stato di quella sorgente, non con il cursore condiviso. Conservati il fallback da interfaccia diretta inattiva e l’individuazione dei protocolli dopo l’assegnazione delle tastiere.
4. **Conservazione UI:** Centro di Controllo completo 10.1, titolo Recovery centrato, tastiera solo italiana e assenza del pannello diagnostico F11 al login. Nessuna modifica a `desktop/control.c` o `desktop/control.h` rispetto alla 10.1.1.

## Cosa dimostrano i test
- Eseguiti il vero codice dei driver e della Recovery con ASan/UBSan, sostituendo solo trasporti/firmware/MMIO nei contratti host.
- Il codice precedente 10.1.1 è ricompilato con le nuove asserzioni e fallisce in quattro controlli negativi: routing con EHCI presente e protocollo SET riconosciuto ma GET ancora Boot nei tre trasporti.
- Un quinto controllo negativo usa la vera funzione di arbitraggio Recovery 10.1.1 e riproduce il mascheramento da AbsolutePointer inattivo. Il codice nuovo supera gli stessi casi.
- Questi sono scenari software riproducibili, **non la dimostrazione che il firmware o il mouse dell’utente presentino esattamente questi difetti**. Compatibilità elettrica, routing reale e funzionamento fisico ASRock Q1900M/J1900 restano da verificare sul PC.

## Artefatto e pubblicazione
10.1.2 / 30B3, ISO BIOS/UEFI con nome `tinymacOS-10.1.2-x86_64.iso`. Due build indipendenti e confronto byte per byte. 575 file sorgente tracciati, escluso lo stato NVRAM generato di QEMU. QA completo prima della pubblicazione, sync sorgenti prima degli upload e verifica indipendente degli asset scaricati. La conferma finale è solo nella ricevuta di pubblicazione, non in queste note preparatorie.

Repository ora **pubblico** per richiesta dell’utente. Credenziali cifrate soltanto nel workspace, chiave separata; nessun token, chiave o archivio credenziali nel repository o nella ISO. Safari/WebKit non incluso, nessun sostituto fittizio.

## Test supplementare PS/2 + USB diretto e correzione firmware
La prima candidata (SHA 2ddcdb8bc8118e5a80c419383314f6d02e0c74fc2df943efd2342d87e8e9e201) non è pubblicata: in BIOS, senza alcuna tastiera USB, R non raggiungeva la Recovery. Le prove UEFI equivalenti passavano. Verificato nell’upstream OpenDuet 1.0.8 che il driver Ps2KeyboardDxe non è incluso: `https://raw.githubusercontent.com/acidanthera/OpenCorePkg/1.0.8/OpenDuetPkgDefines.fdf.inc`.

Aggiunto **Ps2KeyboardDxe.efi ufficiale OpenCore 1.0.8**, estratto dalla release ZIP già bloccata con SHA-256 e senza modificarne il codice; abilitato nella configurazione OpenCore e copiato in tutti i percorsi BIOS/ESP installata. Serve solo alla tastiera firmware di picker/loader; la Recovery continua a prendere il solo figlio tastiera per decodificare i tasti fisici in italiano. Nessun fallback alla mappa firmware per il testo.

Una prova separata abilitando anche UsbMouseDxe ha raggiunto Recovery ma fallito il controllo esatto del cursore (fissato al bordo): quella configurazione è stata scartata, senza dichiararla un successo. **UsbMouseDxe non viene abilitato nella configurazione finale.** Restano il mouse firmware già pubblicato dalla piattaforma e il percorso USB_IO diretto non conteso. Con il solo driver PS/2 aggiunto, la stessa prova BIOS + tastiera PS/2 + mouse USB EHCI supera selezione R, movimento indipendente X/Y, clic e comandi della shell. La matrice completa viene ripetuta sulla ISO finale con nuovo hash; nessun PASS precedente viene rietichettato.
