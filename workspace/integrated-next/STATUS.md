> **Revisione input 21B7 pronta alla sostituzione autorizzata della 2.4.5:** driver PS/2 tastiera/mouse, EHCI HID e UHCI; 16 profili nativi, 117 verifiche input, installer/Recovery e 29/29 regressioni PASS. SHA `2d164de30042f026f107d573d485398e1930a96b692de11a9934976abd62e17a`; note e receipt in `../release-2.4.5-input2/`. OHCI e hotplug restano fuori; nessuna prova sul PC fisico. Le voci sotto descrivono revisioni precedenti.

> **Deploy completato:** [v2.4.5](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.5), release `453129451`, tag sorgenti `b18c4737301f5ba360d14f9e075c0a0b004246ca`. ISO riscaricata dopo pubblicazione, 221489152 byte e SHA-256 verificati. Receipt `../release-2.4.5/github-release-verification.json`. Prossimo lavoro: riprendere i grandi blocchi allocator/libc/C++ verso WTF/JSC, non ancora iniziati.

> **Collaudo finale 2.4.5:** ripristino locale BIOS con preservazione documenti, login installato via mouse su BIOS/UEFI e regressioni kernel/runtime **29/29 PASS**. ISO e ricostruzione indipendente identiche: SHA-256 `8988c9ce7627d191edc2225ea36ee6f834a49a89bceeabe39ed9c65874a79da7`. Limite non passato: selezione con R del Recovery ottico BIOS; ripristino locale e Recovery ottico UEFI separatamente verificati. Vedi `../release-2.4.5/final-qa.json`.

> **Priorità aggiornata:** manutenzione input **2.4.5 Levante / 21B6** prima di riprendere WebKit. Implementati cursore-only al login, accorpamento eventi con barriere, recupero xHCI halt/EP0, fallback Report verificato e F11 diagnostica. Note/receipt in `../release-2.4.5/`. Le indicazioni storiche sotto che riservavano 2.4.5 al browser sono superate. Nessuna verifica sul PC fisico.

> **Ultimo stato:** manutenzione [v2.4.4-oc2](https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4-oc2) pubblicata e riscaricata/checksum verificato: live ottica BIOS+UEFI, vero installer grafico BIOS, ISO rimossa, disco autonomo BIOS/OpenDuet/OpenCore e UEFI/OpenCore, login/riavvio/documenti, Recupero offline e ESP sigillata identica PASS. Mouse USB e tablet verificati. Tag `9c21b15b93b6c36464a0c25f9c394cef1c9f3ca2`; note/receipt in `../release-2.4.4/installer-media/`. OC2 include S_TLS/FS per task. **Dopo OC2** è ripreso WebKit: target errno ora nel TCB per-thread, niente simbolo globale, guest 1/4/8 vCPU con errori reali VM/descriptor e 600 controlli post-yield per profilo PASS; regressione estesa **29/29 PASS** con S_WORDWAIT, mutex/condition, nuovo runtime startup/join/TSD e consumatore C++ freestanding su 1/4/8 CPU. Queste primitive, il runtime thread e il nuovo collegamento errno è su main, non nella ISO OC2. Vedi `../webkit-tmc/RUNTIME-THREADS.md` e `THREADING.md` per contratti e confronto con gli header WTF reali. Startup TLS automatica per i figli gestiti, bootstrap iniziale ancora esplicito; descriptor condivisi non sincronizzati, nessun pthread completo o JSC/WebCore/Safari compilato.

> **Aggiornamento release:** l'utente ha autorizzato una nuova ISO intermedia dopo il backup. È stata preparata, collaudata e pubblicata su GitHub **2.4.4 Levante (21B5)**; note/QA/hash in `../release-2.4.4/RELEASE.md`. Il vecchio paragrafo sottostante che indicava nessun nuovo deploy descriveva lo stato precedente. La futura patch Safari resta 2.4.5 Levante e richiede browser reale e temi.

# Sviluppo integrato — stato di ripresa

## Backup verificato

Repository privata: https://github.com/ESPNex/tinymacOS . Primo commit effettivamente caricato e verificato: `7da0b6c7eee2f59226915d2bf19b66602e72ef91` (1.418 file; albero remoto confrontato per percorso, mode e Git blob hash). Le modifiche successive e i nuovi risultati vengono conservati nei commit seguenti di `main`.

Il database Git Arena si trova in `/var/tmp/tmc-github-git/tinymacOS.git`, la CLI ufficiale in `/var/tmp/tmc-github-cli/gh` e l'autenticazione in una directory separata sotto `/var/tmp`. **Non sono sorgenti persistenti né il backup**. Non copiare credenziali nel checkout. Per riprendere da un altro ambiente, usare un normale clone GitHub privato e nuova autenticazione. `../github_sync.py` permette il backup incrementale senza `.git` nel workspace; controlla repository privata/commit remoto e blocca cancellazioni implicite da snapshot incompleti.

## Risultati correnti

| Area | Esito e prova | Limite |
|---|---|---|
| Backend/runtime | Suite completa 15/15 PASS rieseguita sul codice corrente; `../webkit-tmc/test-results/milestone-tests.json` | Non è accettazione browser |
| VM/MMU | Operazioni positive reali con 1/2 vCPU, sette fault intenzionali per profilo | NX, arena 240 MiB, 32 riserve, massimo 64 MiB per riserva; non mmap POSIX completo |
| SMP concorrente | Quattro worker × 400 round; 1.600 syscall buffer in gara con VM/allocazioni; PASS 4 e 8 vCPU; `evidence/smp-stress-{4,8}/result.json` | Ripetute su build 2.4.4 dopo console/verbose/versione; non provano libc pthread/TLS |
| Verbose UEFI diretto | V durante la mela; framebuffer testuale osservato, stato/log Terminale, due CPU online; `evidence/verbose-direct/result.json` | Su live la scrittura persistente è correttamente rifiutata |
| BIOS/OpenDuet/OpenCore | Disco QA FAT32/MBR: SeaBIOS → OpenDuet 1.0.8 BlockIO → OpenCore 1.0.8 → loader nativo → account/desktop/Terminale; due CPU online; `evidence/bios-current/bios-duet.json` | Verbose verificato da BOOTCFG, **non** da `boot-args`/LoadOptions. Non è ancora ISO ibrida né installazione BIOS autonoma |
| Verbose su installato | Installer reale su SATA usa-e-getta, avvio senza ISO; off → Terminale on → riavvio verbose → Terminale off → riavvio quieto; `evidence/verbose-installed/result.json` | Provato UEFI/QEMU; nessuna prova hardware fisico |
| Integrità installato | ESP e copia di ripristino OS identiche; Recovery cambia soltanto nel singolo settore BOOTCFG e ritorna identica dopo off | Il checksum di BOOTCFG rileva corruzione accidentale, non è una firma crittografica |

### Comandi Terminale

```text
bootverbose status
bootverbose on
bootverbose off
bootverbose log
bootverbose log 1
```

`on/off` controllano il prossimo avvio di un sistema installato; il boot corrente non viene modificato retroattivamente. Il log è un log di avvio nativo, senza il tracing dettagliato del runtime retail. È leggibile dopo autenticazione, in pagine da 512 byte, da 0 a 31. I messaggi del loader precedenti al kernel sono visualizzati in verbose ma non copiati nel ring kernel. Sulla live usare V durante l'avvio oppure un BOOTCFG valido sul supporto di prova; il file ISO è in sola lettura. Le normali impostazioni non vanno scritte dentro l'ESP sigillata.

## Sorgenti e build

- Primari: `../../tinymacOS/`; `sources/` è il sottoinsieme modificato rispetto a `baseline-hashes.json`.
- `qa/sync_sources.py` aggiorna overlay/header e sottoinsieme prima delle build. I vendor restano anche nel primario con lock/licenze.
- Build e immagini soltanto sotto `/var/tmp`, mai nel checkout. Questi output possono sparire: ricostruirli dai sorgenti, non prometterne la persistenza.
- `../../tinymacOS/tools/build-legacy.py` crea solo un nuovo file disco QA esterno. Il fallback EFI punta a OpenCore, il loader nativo è separato in EFI/TMC. La voce nativa viene cercata tramite BlessOverride; i tentativi precedenti con entry custom non sono prove positive.
- Config OpenCore validata con ocvalidate ufficiale 1.0.8; artefatti di redistribuzione estratti in `vendor/opencore-1.0.8/`, non un checkout completo dei sorgenti upstream.
- Il parser ACPI del loader include ora fallback alla tabella ACPI 1.0 e ricerca legacy RSDP checksummed; il test BIOS corrente verifica effettivamente due CPU online.

## Blocchi ancora aperti

1. **Safari/WebKit reale assente.** Non sono compilati WTF, JavaScriptCore o WebCore. Nessun rendering reale HTML/CSS/JS o HTTPS nel browser, né shell Safari collegata ai temi. Restano startup automatica e runtime libc/C++, ELF/C++ TLS/DTV, pthread/attese, allocator WTF, grafica/font/event loop e TLS di rete con validazione completa.
2. Il bootstrap BIOS di prova non aggiorna l'installer GPT/FAT16 a un sistema BIOS installato autonomo. Non distribuire il disco QA come se fosse l'ISO integrata richiesta.
3. Verificare o correggere il percorso OpenCore `-v`: i tentativi precedenti senza BOOTCFG non hanno attivato il flag. Il risultato positivo corrente usa deliberatamente BOOTCFG, non nasconde questo limite.
4. Completare i test di regressione e hardware prima di una release. Lo stress nativo non equivale a un audit generale della sicurezza.
5. **Release intermedia 2.4.4 pubblicata su autorizzazione esplicita**, senza browser: https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.4 . Tag immutato `9e7c83c58f0452e6d2d5f00768862cab339de2ca`; checksum e download remoto verificati. Non chiamare questa ISO o le build diagnostiche una release Safari. La successiva patch 2.4.5 resta condizionata al browser reale e ai temi.

## Ripetizione QA

```sh
python3 workspace/integrated-next/qa/sync_sources.py
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-integrated-system bash tinymacOS/build.sh
python3 workspace/integrated-next/qa/verbose_installed.py
python3 workspace/integrated-next/qa/verbose_direct.py
python3 tinymacOS/tools/build-legacy.py --verbose-config /var/tmp/tmc-integrated-system/tinymacOS-2.4.3-x86_64.iso /var/tmp/tmc-legacy-config3.raw
python3 workspace/integrated-next/qa/bios_boot.py
TOOLROOT=/var/tmp/tmc/tools/root python3 workspace/webkit-tmc/tools/run-tests.py --guest
python3 workspace/github_sync.py --push -m "Aggiornamento sviluppo e prove"
```

La destinazione raw deve essere nuova: se esiste già, sceglierne una nuova e passare lo stesso percorso alla variabile `TMC_BIOS_DISK` del test. Alcuni percorsi QA conservano il workspace `/home/user`: adattarli in un clone diverso. Non lanciare test contro dischi reali o dati da conservare.
