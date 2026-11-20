# Aggiornamento: Q1900M / revisione 21B8

**Riscontro fisico dell’utente sulla 21B7:** tastiera funzionante, mouse USB ancora fermo; scheda madre indicata ASRock Q1900M. Non considerare la release 21B7 una soluzione hardware confermata per il mouse.

Implementati hub USB2 xHCI con route/TT e figli annidati, routing PCI per Intel switchable incluso Bay Trail 8086:0f35, attese native temporizzate, reset porte più robusti. Nessun driver legato al nome della motherboard. GUI: chiave cache chrome include oscuramento Mission Control, mela spostata di due pixel verso l’alto.

Vecchia ISO 21B7: riprodotto mouse USB dietro hub fermo con tastiera PS/2 funzionante. QEMU può scegliere il fallback PS/2 e nascondere questo difetto; il nuovo harness forza e verifica il dispositivo puntatore, spegne i8042 nei profili tastiera USB. I receipt input precedenti a tale irrigidimento sono stati esclusi e la matrice rieseguita. Vecchia GUI: riprodotto alone, nuova GUI zero pixel diversi in sei cicli Dock orizzontale/verticale; mela/File centro 14,5 px. Regressione kernel/runtime 29/29 PASS sul kernel finale; build dualboot riprodotta byte-identica.

Candidato: 2.4.5/21B8, SHA `fd24fc89e28e4cbb20a05738e2ccfc29ad7597ddb742afe26ab85f1375cf47e2`. **Gate finale PASS: 27 profili input (17 live + 10 installati), 197 verifiche raggruppate, installer/Recovery, GUI e 29/29 regressioni.** **Sostituzione completata e download verificato**, stessa release ID 453129451, tag sorgenti f4e499bd2dcea87bbf2eae86e81e714b1b7c0e8b; consultare `release-2.4.5-input3/final-qa.json` e `github-release-verification.json` per gate/deploy completati. Nessuna nuova verifica sul PC fisico; OHCI, hotplug e hub SuperSpeed non implementati. Browser genuino Safari/WebKit ancora da riprendere dopo la manutenzione.

---

## Storico 21B7 e revisioni precedenti

# Aggiornamento: revisione input 21B7

Sono ora implementati i driver nativi PS/2 tastiera/mouse, EHCI HID e UHCI companion. I dieci profili live del candidato finale su BIOS/UEFI sono passati, incluse configurazioni senza alcuna tastiera USB e mouse a 480/12 Mb/s; anche la suite kernel/runtime è 29/29 PASS. Installazione, sei profili login installati e ripristino locale sono passati; gate completo finale PASS. Release online sostituita con 21B7 e riscaricata/verificata: gate e receipt definitivi in `release-2.4.5-input2/`. Nessuna conferma sul PC fisico. OHCI, hotplug e general-purpose USB bulk/isochronous restano fuori dal supporto implementato.

---

# Follow-up dopo la prova fisica della 2.4.5

L’utente conferma: tastiera PS/2 e mouse USB funzionano in Recovery, non al login. I test di accettazione precedenti usavano tastiera USB su xHCI e NON coprivano la sua tastiera PS/2. Il driver nativo PS/2/i8042 manca: questa è una lacuna confermata del sistema, non un errore di configurazione dimostrato dell’utente. La causa esatta del mouse USB sul PC resta sconosciuta; i driver firmware del Recovery e quello xHCI nativo sono distinti.

Su richiesta rimossi dal sorgente corrente pannello, pulsante, disegno, aggiornamento periodico e scorciatoia F11 del login. Mantenuto il danno limitato del cursore. I contatori interni e il numero ABI aggiunto restano, senza UI diagnostica al login. Aggiornati i test di sviluppo per non dipendere dal pannello e la guida corrente. Compilazione target di login.c con -Werror PASS; sintassi Python PASS. Non eseguita nuova suite completa su questa modifica. Non costruita né pubblicata un’altra ISO: la 2.4.5 pubblicata resta byte-identica e contiene ancora F11.

Prossimo lavoro necessario: driver nativo tastiera PS/2 con test realmente PS/2, identificazione controller/percorso del mouse USB sul PC e test nativi pertinenti, prima di dichiarare risolto il problema fisico. Richiedere modello PC/scheda madre e collegamento del mouse (porta diretta/hub) per non inventare una diagnosi EHCI/xHCI. Non proporre F11 come requisito per diagnosticare una tastiera che il kernel non gestisce. WebKit rimane sospeso.
