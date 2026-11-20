# Aggiornamento: revisione input 21B7

Sono ora implementati i driver nativi PS/2 tastiera/mouse, EHCI HID e UHCI companion. I dieci profili live del candidato finale su BIOS/UEFI sono passati, incluse configurazioni senza alcuna tastiera USB e mouse a 480/12 Mb/s; anche la suite kernel/runtime è 29/29 PASS. Installazione, sei profili login installati e ripristino locale sono passati; gate completo finale PASS. Non ancora sostituita la release online: il gate e il receipt definitivi sono in `release-2.4.5-input2/`. Nessuna conferma sul PC fisico. OHCI, hotplug e general-purpose USB bulk/isochronous restano fuori dal supporto implementato.

---

# Follow-up dopo la prova fisica della 2.4.5

L’utente conferma: tastiera PS/2 e mouse USB funzionano in Recovery, non al login. I test di accettazione precedenti usavano tastiera USB su xHCI e NON coprivano la sua tastiera PS/2. Il driver nativo PS/2/i8042 manca: questa è una lacuna confermata del sistema, non un errore di configurazione dimostrato dell’utente. La causa esatta del mouse USB sul PC resta sconosciuta; i driver firmware del Recovery e quello xHCI nativo sono distinti.

Su richiesta rimossi dal sorgente corrente pannello, pulsante, disegno, aggiornamento periodico e scorciatoia F11 del login. Mantenuto il danno limitato del cursore. I contatori interni e il numero ABI aggiunto restano, senza UI diagnostica al login. Aggiornati i test di sviluppo per non dipendere dal pannello e la guida corrente. Compilazione target di login.c con -Werror PASS; sintassi Python PASS. Non eseguita nuova suite completa su questa modifica. Non costruita né pubblicata un’altra ISO: la 2.4.5 pubblicata resta byte-identica e contiene ancora F11.

Prossimo lavoro necessario: driver nativo tastiera PS/2 con test realmente PS/2, identificazione controller/percorso del mouse USB sul PC e test nativi pertinenti, prima di dichiarare risolto il problema fisico. Richiedere modello PC/scheda madre e collegamento del mouse (porta diretta/hub) per non inventare una diagnosi EHCI/xHCI. Non proporre F11 come requisito per diagnosticare una tastiera che il kernel non gestisce. WebKit rimane sospeso.
