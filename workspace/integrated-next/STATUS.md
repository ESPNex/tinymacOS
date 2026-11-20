> **Aggiornamento release:** l'utente ha autorizzato una nuova ISO intermedia dopo il backup. È stata preparata e collaudata **2.4.4 Levante (21B5)**; note/QA/hash in `../release-2.4.4/RELEASE.md`. Il vecchio paragrafo sottostante che indicava nessun nuovo deploy descriveva lo stato precedente. La futura patch Safari resta 2.4.5 Levante e richiede browser reale e temi.

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

1. **Safari/WebKit reale assente.** Non sono compilati WTF, JavaScriptCore o WebCore. Nessun rendering reale HTML/CSS/JS o HTTPS nel browser, né shell Safari collegata ai temi. Restano runtime libc/C++, TLS/errno di thread, pthread/attese, allocator WTF, grafica/font/event loop e TLS di rete con validazione completa.
2. Il bootstrap BIOS di prova non aggiorna l'installer GPT/FAT16 a un sistema BIOS installato autonomo. Non distribuire il disco QA come se fosse l'ISO integrata richiesta.
3. Verificare o correggere il percorso OpenCore `-v`: i tentativi precedenti senza BOOTCFG non hanno attivato il flag. Il risultato positivo corrente usa deliberatamente BOOTCFG, non nasconde questo limite.
4. Completare i test di regressione e hardware prima di una release. Lo stress nativo non equivale a un audit generale della sicurezza.
5. **Nessun nuovo deploy ISO.** Ultima release distribuita resta la manutenzione 2.4.3, senza browser; non chiamare le build diagnostiche una nuova release Safari.

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
