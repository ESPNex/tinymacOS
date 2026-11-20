# tinymacOS 2.4.4 Levante — build 21B5

Release intermedia sperimentale richiesta esplicitamente prima del completamento del browser. **Il nome resta Levante. Safari/WebKit non sono inclusi.**

## Download

Pagina release privata: https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4

- `tinymacOS-2.4.4-x86_64.iso` — **118.255.616 byte**
- `SHA256SUMS.txt` — checksum della ISO

```text
a01d95c4f33a68e11b002f37e3ec52472b9d927b3f8c4326ea01ac8d8bdf5a51  tinymacOS-2.4.4-x86_64.iso
```

Occorre accedere a GitHub con un account autorizzato alla repository privata. Non è un link pubblico e non è stato eseguito un nuovo upload su Drive.

## Cosa cambia

- **VM multi-CPU reale:** coordinamento delle mutazioni dello spazio utente, invalidazione TLB e attese degli ACK prima di liberare le pagine. Operazioni positive e fault intenzionali su MMU reale con 1/2 vCPU; stress concorrente con 4/8 vCPU.
- **Verbose nativo:** V durante la mela, oppure `bootverbose on/off` nel Terminale del sistema installato per il prossimo riavvio. `bootverbose status` distingue boot corrente e preferenza successiva; `bootverbose log [pagina]` consulta il log kernel dopo autenticazione. La live non salva questa preferenza e rifiuta correttamente la scrittura.
- **Versione coerente:** 2.4.4 / build 21B5 nelle app, plist, Shell, Recupero e documento iniziale. Etichette disegnate di Info e installer aggiornate a **LEVANTE**. I nomi degli sfondi/temi storici non vengono cancellati.
- Conservate le correzioni 2.4.3 per piccoli movimenti del mouse relativo, GOP e puntini password compatti.
- Sorgenti, vendor estratti, strumenti, QA e risultati conservati nella repository; niente sorgenti ZIP/TAR allegati. Il tag identifica i sorgenti da clonare.

## Verifiche della build 2.4.4

- Audit della ISO effettiva, dei volumi live/installato e del Recupero: versioni/build, digest KernelCache, bundle, assenza browser e diagnostica, identità disegnata.
- **15/15** verifiche backend/runtime: contratti host, compilazione target, rete/file guest e VM/MMU guest a 1/2 vCPU. Sette fault intenzionali per profilo, più ripresa positiva dopo i fault.
- Stress nativo a **4 e 8 vCPU**: quattro worker, 400 round, 1.600 syscall con buffer in gara con VM/allocazioni.
- Mouse USB normale e tablet QEMU: focus/clic dei campi account, login, cursore, trascinamento, clic destro, accesso al Recupero e versione 2.4.4 letta nella Shell Recovery.
- Installer su SATA virtuale vuoto → boot senza ISO → `bootverbose on` → riavvio verbose → `off` → riavvio silenzioso. ESP e sorgente di ripristino invariate; soltanto il settore BOOTCFG in Recovery viene aggiornato e verificato.
- Avvio UEFI diretto con V, log/stato e due CPU online.

Risultati JSON e screenshot selezionati: `workspace/release-2.4.4/`. Manifest dei sorgenti salvata prima della build; tutti i 439 file censiti ancora identici al termine delle prove. Le immagini diagnostiche con alias command sostituiti **non sono la ISO allegata** e non vengono distribuite.

## Requisiti e avvertenze

**Questa ISO e l’installer sono UEFI, non BIOS.** Usare x86-64/OVMF, Secure Boot disattivato, Q35/AHCI, 1 GiB RAM consigliato, USB su xHCI e mouse/tablet. Il bootstrap SeaBIOS/OpenDuet/OpenCore collaudato è un disco QA separato, non un boot BIOS integrato in questa ISO. L’installazione non produce un disco BIOS autonomo.

Per una chiavetta: Rufus **ISO mode**, GPT/UEFI senza CSM, FAT32, Secure Boot disattivato; non DD. La scrittura cancella la chiavetta. Non è stato eseguito Rufus su Windows o un test su hardware fisico. Per installare, usare un nuovo disco virtuale SATA vuoto dedicato, almeno 96 MiB consigliati, senza dati da conservare. La nuova ISO non aggiorna automaticamente un’installazione esistente.

R o F12 durante la mela apre il Recupero locale incluso; non serve un secondo download. Il mouse funzionante in QEMU non garantisce il supporto di tutti i controller/hub USB reali. Sistema sperimentale, non per dati importanti; non è macOS e non fornisce compatibilità generale con software Apple.

## Passo successivo

Proseguire con **Safari e WebKit reali**, non con un renderer fittizio: runtime libc/C++, TLS/errno di thread e pthread/attese, WTF/JavaScriptCore, WebCore e grafica/font/event loop, HTTPS con validazione, UI classica e temi. La futura ISO sarà **2.4.5 Levante**, soltanto dopo i test rappresentativi di browser e integrazione temi. Nessuna compatibilità universale con ogni sito è garantita.
