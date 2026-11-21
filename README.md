# tinymacOS — sorgenti e workspace

**tinymacOS 10.1.2 Tramontana / 30B3 — manutenzione corrente.** Pubblicazione normale/Latest prevista dopo QA e sync sorgenti. Lo stato effettivo è attestato dalla ricevuta `workspace/release-10.1.2-30B3/github-release-verification.json`, presente solo a pubblicazione completata.

Riferimento fisico precisato dall’utente: **2.4.5 Levante / 21B9**, con mouse funzionante in Recovery e solo verticale al login. La 10.1.2 ripristina il routing USB2 iniziale di quella revisione sui controller già supportati, verifica il readback della modalità HID e impedisce che un AbsolutePointer inattivo mascheri il mouse USB in Recovery. Centro di Controllo 10.1 completo e titolo Recovery centrato conservati. **La nuova ISO non è ancora verificata sul PC fisico dell’utente.**

Repository **pubblico** per richiesta dell’utente. Le ISO sono negli asset delle release, non nella cronologia Git. Credenziali cifrate solo nel workspace escluso da Git, chiave separata.

Artwork importato solo per le app; simboli di sistema procedurali e Recovery procedurale salvo Terminale. Sfondo non procedurale, semibold nel wordmark, temi e associazioni sfondi conservati. Safari/WebKit non incluso.

- [Sistema, build e limiti](tinymacOS/README.md)
- [Note 10.1.2, QA e limiti](workspace/release-10.1.2-30B3/RELEASE.md)
- [Preview reale Centro di Controllo 10.1.2](workspace/release-10.1.2-30B3/evidence/control-center-detail.png)
- [Archivio precedente 10.1.1](workspace/release-10.1.1-30B2/RELEASE.md)
- [Confronto 21B9 e correzioni](workspace/release-10.1.2-30B3/HISTORY-AND-CHANGES.md)
- [Ripristino accesso cifrato, chiave separata](workspace/ACCESSO-CIFRATO.md)
- [Archivio precedente 10.1 / 30B1](workspace/release-10.1-30B1/RELEASE.md)
- [Archivio 10 / 30A4](workspace/release-10-30A4/RELEASE.md)
- [Backend sperimentale WebKit, non browser funzionante](workspace/webkit-tmc/README.md)

Il repository pubblico non contiene credenziali né ISO nel checkout. Output e toolchain vanno
fuori dal workspace. L'archivio lossless dei vecchi asset duplicati è documentato
in `workspace/maintenance-2.4.3/sources/HISTORICAL-ASSETS.md`.
Le evidenze storiche 2.4.2 e integrated-next sono consolidate in archivi lossless con manifest SHA-256: vedere i rispettivi `EVIDENCE-ARCHIVE.md` per ripristinare tutti i percorsi originali. Nessun sorgente o risultato storico è perso.

## Documentazione storica

Le sezioni seguenti sono conservate come storico e non identificano la candidata corrente.

> **Corrente e pubblicata: [2.4.5 / 21B8](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.5):** hub USB2 nativi xHCI, routing Intel switchable/Bay Trail, alone Dock dopo Mission Control e allineamento mela corretti. [Note, limiti e gate](workspace/release-2.4.5-input3/RELEASE.md). 27 profili input e 29/29 regressioni PASS, con test anti-fallback. Sostituzione nella stessa release completata; ISO riscaricata e SHA-256 verificato, receipt incluso. Le voci 21B7 e 21B6 sotto sono storiche; la 21B7 non risolveva il mouse sul PC dell’utente.

> **Storico, revisione precedente: [tinymacOS 2.4.5 Levante / 21B7](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.5).** Sostituita la ISO nella stessa release su richiesta. PS/2 tastiera/mouse, EHCI HID e UHCI companion; diagnostica F11 rimossa. 16 profili input, 117 verifiche e 29/29 regressioni PASS; download verificato. [Note e limiti](workspace/release-2.4.5-input2/RELEASE.md).

> **Revisione input 2.4.5 / 21B7:** driver nativi PS/2 tastiera/mouse, EHCI HID e UHCI companion; nessuna diagnostica F11 al login. 16 profili input e 29/29 regressioni PASS. [Note e limiti](workspace/release-2.4.5-input2/RELEASE.md); il receipt di sostituzione conferma l’avvenuto deploy nella stessa release. Le indicazioni 21B6 sotto sono storiche.

> **Manutenzione input 2.4.5 Levante / 21B6:** [Pubblicata su GitHub](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.5), riscaricata e verificata; manutenzione richiesta per mouse fermo al login e lag. Note, collaudi e limiti: [release 2.4.5](workspace/release-2.4.5/RELEASE.md). Nessuna prova sul PC fisico dell’utente; Safari/WebKit ancora non incluso.

> **Precedente manutenzione OpenCore:** [v2.4.4-oc2](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-oc2): live ottica BIOS+UEFI e **disco installato autonomo BIOS/OpenDuet/OpenCore+UEFI**. Prove con ISO rimossa, riavvio e Recupero locale. Non immagine USB DD. [Note e QA](workspace/release-2.4.4/installer-media/RELEASE.md).

# tinymacOS — sorgenti e workspace di sviluppo

Repository privata: https://github.com/ESPNex/tinymacOS

Questo repository conserva i file del workspace necessari a riprendere lo sviluppo senza dipendere dal limite di 128 MiB degli snapshot della conversazione. Non contiene credenziali, ISO, archivi di sorgenti o directory di build. I file vendor selezionati sono conservati estratti, con licenze e lock disponibili nei rispettivi alberi.

**Release 2.4.4 Levante (21B5):** [download privato e note](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4). [QA e checksum](workspace/release-2.4.4/RELEASE.md). La ISO originale era UEFI; OC2 aggiunge live ottica e installazione BIOS, sempre senza browser.

## Mappa

- `tinymacOS/`: albero primario di sviluppo del sistema nativo x86-64, build e strumenti di test. Release manutenzione input esplicitamente autorizzata: **2.4.5 Levante, build 21B8**, senza browser. La richiesta di anticipare questa manutenzione sostituisce la precedente riserva del numero 2.4.5 al browser.
- `workspace/webkit-tmc/`: backend nativo e lavoro preparatorio al port WebKit. **Non è ancora Safari né un motore WebKit/JSC funzionante.**
- `workspace/integrated-next/`: sorgenti modificati sincronizzati, QA ed evidenze per SMP, verbose e bootstrap BIOS/OpenDuet/OpenCore.
- `workspace/maintenance-2.4.3/`: consegna precedente, evidenze e copia congelata dei sorgenti distribuiti; da non confondere con lo sviluppo corrente.
- Gli altri alberi `workspace/maintenance-*`, `workspace/smp-next` e i documenti alla radice conservano contesto, test e revisioni precedenti.
- `uploads/`: allegati originali della conversazione, conservati in questa repository privata.

## Stato e limiti reali

- VM/MMU nativa: verifiche positive su 1/2 vCPU e stress concorrente su 4/8 vCPU documentati. Non sono prove di pthread/POSIX completo o WebKit.
- Supporto OC2: CD BIOS via MEMDISK/OpenDuet BlockIO/OpenCore; installato BIOS via ponte MBR → OpenDuet completo USB/AHCI → OpenCore → loader/kernel nativi. Il medesimo disco avvia anche in UEFI tramite il launcher OpenCore ufficiale. GPT/FAT16 invariata; Recupero e sorgente sigillata verificati. OC1 resta una revisione precedente solo ottica.
- Verbose UEFI diretto selezionato con `V`, comandi `bootverbose status`, `on`, `off`, `log [pagina]` implementati. Verificati selezione live, stato, log e rifiuto di scrittura persistente sulla live. Il test completo della preferenza installata in `workspace/integrated-next/qa/verbose_installed.py` è passato: off → on da Terminale → riavvio verbose → off → riavvio quieto; ESP sigillata e payload Recovery invariati. Evidenze in `workspace/integrated-next/evidence/verbose-installed/`.
- `workspace/webkit-tmc/test-results/milestone-tests.json` riporta la suite backend/runtime. Date e commit delle prove contano: un risultato precedente non valida automaticamente modifiche successive.
- **Safari/WebKit reale non è completato. La 2.4.5 è una release manutenzione senza browser, non la futura ISO Safari.**
- Nessuna validazione su hardware fisico; i risultati QEMU non garantiscono compatibilità BIOS/USB universale.

## Ricostruzione

Clonare normalmente la repository. Consultare prima `tinymacOS/README.md`, `workspace/webkit-tmc/README.md`, i lock vendor e i relativi strumenti di provisioning. Il checkout è portabile: non richiede il vecchio contenuto di `/var/tmp`; toolchain e output temporanei vanno ricreati seguendo gli strumenti presenti. Il repository non include un checkout completo di WebKit: il sottoinsieme `upstream-reference/` è esplicitamente solo di riferimento.

Esempio per il sistema, dalla radice del clone, una volta installati/ripristinati i prerequisiti:

```sh
python3 tinymacOS/tools/local-tools.py /var/tmp/tmc/tools/root
python3 workspace/integrated-next/qa/sync_sources.py
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-integrated-system bash tinymacOS/build.sh
```

Gli script QA storici possono contenere `/home/user` come percorso del workspace originale: adattarlo se si clona altrove. Le ISO restano **fuori dal checkout**, anche temporaneamente. Non eseguire comandi di scrittura su dischi fisici usando gli strumenti QA: essi sono pensati per file nuovi e dischi virtuali usa-e-getta.

## Git e autenticazione

Nell'ambiente Arena il database Git locale è mantenuto sotto `/var/tmp/tmc-github-git/` per non raddoppiare il consumo degli snapshot. Questo database e l'autenticazione CLI non sono il backup: **il backup è il commit effettivamente inviato e verificato su GitHub**. Fuori da Arena usare un normale `git clone`.

Le credenziali OAuth in uso sono separate sotto `/var/tmp`. Il backup autenticato cifrato resta soltanto nel workspace escluso da Git, con chiave fornita separatamente: [guida al ripristino](workspace/ACCESSO-CIFRATO.md). Possono richiedere nuova autorizzazione se il token viene revocato o scade. Non inserire token o chiavi in URL, script, documenti, repository o ISO.

Per i successivi backup nell'ambiente Arena:

```sh
python3 workspace/github_sync.py                 # stato, senza commit/push
python3 workspace/github_sync.py --push --allow-public -m "Descrizione delle modifiche"
```

Lo script verifica visibilità e commit remoto, richiede `--allow-public` per il repository pubblico, non fa force-push, rifiuta cancellazioni implicite da snapshot incompleti e cerca pattern comuni di credenziali prima di pubblicare. Non è un secret scanner esaustivo: serve comunque revisione. Se CLI/autenticazione sono scadute o perse, autorizzarle nuovamente senza salvare token nel checkout.
