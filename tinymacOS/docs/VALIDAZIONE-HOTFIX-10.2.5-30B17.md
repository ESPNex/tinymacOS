# tinymacOS 10.2.5

**Tramontana · revisione 30B17 · hotfix della 10.2.5**, non una pubblicazione 10.3.
Una sola ISO x86_64 avviabile BIOS+UEFI; il titolo della release resta senza descrizioni aggiunte.

## Correzioni selettive

- **Font 10.2.4 ripristinato dal prodotto distribuito**, non dal solo sorgente: 2.180.353 byte, SHA-256 `b2dc1065c12eb43378e02d824af58697369760c268f2dbc64542dcac04a514d2`. Audit del font nei cataloghi live/installato/Recovery. Nuovo lock del packager rifiuta il font errato anche se passato come override.
- **Mouse e drag-and-drop:** conservati driver USB/scaling della 10.2.4; code input/IPC del desktop limitate a 64/32 eventi per frame, senza eliminare drag-and-drop, copia/incolla o gli eventi restanti. Verificati mouse USB relativo BIOS e UEFI, clic/setup/cursore/trascinamento esatto di finestre; trasferimenti reali file/cartelle, Desktop, barra laterale, Ctrl-drag e percorsi nel Terminale, con persistenza dopo riavvio.
- **Spazio non allocato, non RAW:** inventario dei gap mantenuto anche quando la copia GPT validata ha peer degradato/discordante. Fixture EFI + Microsoft Reserved 16 MiB + due NTFS + 10 GiB realmente non allocati: il gap è visibile. Su tabella integra è selezionabile; su GPT discordante resta protetto e l'installer spiega il blocco. MSR/NTFS/RAW non sono riclassificati come spazio libero; nessuna riparazione automatica o scrittura forzata.
- **Basi 10.3 verificate conservate:** VM/MMU, thread/TLS, sincronizzazione, x87/SSE2, clock monotonic e runtime. Verificato anche il componente C++/WTF dtoa selezionato, con test normale e sanitizzato. **Non include Safari completo, WTF completo, JavaScriptCore, WebCore o HTTPS/TLS validato.**

## Verifica della revisione

Chiusura di **46 controlli distinti**, non un singolo lancio nuovo “46/46”:

| Gruppo | Esito | Ambito |
|---|---:|---|
| Runtime host | 18/18 | contratti backend e oggetti target |
| Regressioni OS host | 13/13 | input, gap Windows, partizioni, Finder, installer, grandi dischi, 4Kn, conversione, NVMe |
| Componente C++/WTF selezionato | 1/1 | normale + ASan/UBSan; regressione upstream negativa conservata |
| Suite guest | 13/13 | installazione/Recovery; trasferimenti usb-mouse; 4Kn; conversione 512 e 4Kn; helper NVMe; import esterno; install/reset cifrato; backup UI; reset; gap Windows; puntatore BIOS e UEFI |
| Audit ISO finale | 1/1 | build 30B17, font esatto nei tre cataloghi, KernelCache comune, tre cataloghi identici alla build nativa |

Installazione GUI e avvio a freddo, reinstallazione da Recovery interna, account/documenti/snapshot conservati. Installazione e avvio 4Kn; conversione e disattivazione cifratura in-place sia a 512 byte sia a 4Kn. Installazione cifrata, credenziale di recupero indipendente, backup/ripristino, reset con nuove chiavi e persistenza dopo riavvio. Helper NVMe provato con firmware compilato senza supporto NVMe e caso negativo senza helper. Il file helper pubblico resta byte-identico: non contiene il sistema operativo.

### Trasparenza sui tentativi

Il primo batch host parallelo ha avuto un processo terminato con exit 137 sotto pressione RAM; i sei controlli non chiusi sono stati ripetuti in serie e passati. Primo tentativo Windows: il firmware ha riparato la fixture con CRC errato prima del test; seconda VM interrotta per risorse. La prova finale usa due GPT singolarmente valide ma discordanti e verifica visibilità/protezione e hash invariati. Il primo avvio del test helper NVMe mancava del lock del firmware QA: errore di preparazione prima del guest, poi risolto e ripetuto. Ricevute originali e replay restano nell'archivio; non sono riscritti come successi.

Il test puntatore UEFI usa la build ottica nativa fresca: tutti e tre i cataloghi e il kernel live/installato corrispondono byte per byte alla ISO dual finale. BIOS e gli altri test sulla nuova ISO usano la ISO finale oppure il suo kernel installato verificato.

## Limiti importanti

- **Il blocco del mouse sul PC fisico segnalato non è stato riprodotto né la causa hardware dimostrata.** Le prove sono QEMU; occorre la conferma sul dispositivo reale. Non è una certificazione universale USB.
- Layout Windows sintetico, non prova di dual boot su Windows reale né diagnosi della tabella del PC dell'utente. Una GPT protetta resta non installabile: non aggirare il controllo e non cancellare MSR/RAW per creare spazio.
- Supporto ottico BIOS+UEFI, **non immagine USB-DD ibrida**. ABI disco fino a 1 TiB. Modifica tabelle su 4Kn già popolato e resize cifrato non supportati. Nessuna prova di cancellazione fisica sicura.
- Questo hotfix non annuncia un browser pronto. Il lavoro Safari resta separato.

## Download e integrità

`tinymacOS-10.2.5-x86_64.iso` — **329.519.104 byte**

SHA-256: `c88771e423fadd2080a2a27db0d406c6e99d61a96c814f1ec1c3e041edf41cf1`

`SHA256SUMS` copre ISO, sorgenti, helper, questo documento e `VERIFICA-FINALE-10.2.5.tar.gz`. L'archivio delle prove contiene manifest con hash per file, ricevute originali/replay e schermate non alterate. Nei sorgenti: `tinymacOS/docs/HOTFIX-10.2.5-30B17.md` e `workspace/local-10.2.5/hotfix/verification-30B17.json`.
