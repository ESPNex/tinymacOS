# 10.2.5 Tramontana — revisione 30B17

Revisione selettiva della 30B16 dopo le segnalazioni di mouse USB fermo,
font cambiato e spazio non allocato non mostrato accanto a Windows.
Non è un ritorno completo al codice 10.2.4 e non introduce un browser incompleto.

## Font: ripristino dal prodotto effettivamente distribuito

Il font presente nei sorgenti 10.2.4 e 10.2.5 era identico, ma **quello dentro le
ISO non lo era**. Il controllo precedente dei soli sorgenti era insufficiente.
Ripristinato byte per byte il font accettato, recuperato dalla ISO 10.2.4 di cui
è stato verificato il digest GitHub. Ora il file di produzione `assets/type.tfn`
è quello accettato, non un override esterno necessario per ricostruirlo.

- Font accettato: 2.180.353 byte, SHA-256
  `b2dc1065c12eb43378e02d824af58697369760c268f2dbc64542dcac04a514d2`.
- Font errato della 30B16: 2.453.335 byte, SHA-256
  `17b8004b11b20bd9d7151e3d4cb5fba9608dea94be1a35f33114880462929cc6`.
- `assets/interface-font-lock.json` blocca sostituzioni silenziose nel packager,
  anche tramite `TMC_FONT`. Il test negativo richiede il rifiuto del vecchio font.
- Audit indipendente della ISO finale: stesso font accettato nei tre cataloghi
  live/installato/Recovery, versione 30B17, kernel live/installato identico.

## Mouse, eventi del desktop e drag-and-drop

Conservati i driver/adattatori USB e lo scaling del puntatore già presenti nella
10.2.4. Modificata la gestione del frame del desktop: le code di input e di IPC
(non più soltanto menu contestuali, ma anche drag/selection) non possono tenere
indefinitamente occupato il ciclo prima della presentazione del cursore.

Budget di 64 eventi input e 32 messaggi per frame; gli eventi restanti rimangono
in coda, non vengono scartati dal nuovo budget. La coalescenza di movimento e le
transizioni di pulsante restano gestite dal livello input esistente. Copia/incolla,
selezione del Terminale e drag-and-drop non sono stati eliminati.

Test della candidata: mouse USB relativo emulato, setup a clic, movimento e
confronto pixel del cursore, trascinamento preciso finestre e menu contestuali.
Nella ISO finale sono verificati anche i trasferimenti reali con **usb-mouse**,
non soltanto tablet: file/cartelle, barra laterale, Desktop, Ctrl-drag di copia,
drop del percorso nel Terminale, appunti, lock e persistenza dopo riavvio.

**Limite esplicito:** queste prove QEMU e il modello di fairness non dimostrano
che sia stata identificata la causa esatta sul PC fisico segnalato. Modello mouse,
VID/PID e prova sul dispositivo reale non sono ancora disponibili. Non è una
certificazione universale USB né una ricostruzione provata del blocco hardware.

## Spazio davvero non allocato vicino a Windows, non RAW

Una GPT con copia valida ma peer degradato/discordante era riconosciuta per le
partizioni, poi il lettore usciva prima di calcolare i gap. Ora conserva
l'inventario di lettura e mostra anche i gap derivati dalla copia validata.
La protezione dalle scritture su GPT non sicura **non è stata rimossa**.

Fixture dedicata: EFI, vera Microsoft Reserved da 16 MiB, due NTFS, gap reale
da 10 GiB. Prove host normale+ASan/UBSan e guest con Terminale/installer reali:

- tabella integra: 10.240 MiB non allocati visibili, destinazione selezionabile;
- copie GPT valide ma discordanti: medesimo gap visibile, installazione bloccata
  con il motivo di protezione;
- MSR identificata dal GUID, mantenuta occupata; **RAW/NTFS/MSR non diventano
  spazio libero**. La sola dimensione 16 MiB non viene usata per classificare;
- preflight guest senza installazione: hash di metadati e confini degli extent
  occupati invariati. Nessuna riparazione automatica della GPT Windows.

La fixture non è Windows reale e non ricostruisce la tabella del PC dell'utente.
Se la tabella effettiva è protetta, occorre prima conoscerne il motivo; questa
revisione non forza l'installazione su metadata discordanti.

## Merge selettivo delle basi 10.3

Mantenute le basi native già sviluppate e verificate: VM/MMU, thread/TLS,
sincronizzazione, salvataggio contesto x87/SSE2, clock monotonic e runtime.
Contratti runtime ricompilati e rieseguiti; il workspace dei consumer C++ e del
componente WTF dtoa resta nei sorgenti, non diventa una finta app Safari.

**Non inclusi:** Safari completo, WTF completo, JavaScriptCore, WebCore, HTTPS/TLS
validato o un renderer sostitutivo. Il browser non viene annunciato come pronto.

## Distribuzione e limiti invariati

La versione resta 10.2.5, build distinguibile **30B17**. Una sola ISO BIOS+UEFI;
nessun tag alias aggiuntivo o seconda ISO UEFI separata. Ricevute della revisione
sotto `workspace/local-10.2.5/hotfix`; le vecchie prove 30B16 non sono attribuite
alla nuova ISO. I test interrotti/falliti e i replay sono distinti nelle prove.

Restano i limiti documentati: supporto ottico non USB-DD ibrido, ABI disco fino a
1 TiB, 4Kn popolato/modifica tabelle e resize cifrato non supportati, nessuna
certificazione hardware fisico o dual boot Windows reale.
