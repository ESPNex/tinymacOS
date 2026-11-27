# Ritorno a M0 — pulizia, baseline e qualità dell’interfaccia

**Intervento richiesto dopo M2. M3 non avanzato. Nessuna release 10.3.**

## 1. Pulizia eseguita prima delle modifiche

Rimossi **15.499.376 byte** di archivi storici duplicati 10.2.5/10.2.6, solo dopo download dal commit GitHub immutabile `3453dc8a874be85197d7118df996d31788deb23d` e confronto SHA-256/dimensione. Conservati ricevute e screenshot storici utili. Percorsi, hash, URL e comandi di recupero in `cleanup.json`; i documenti storici indicano ora il recupero remoto anziché un file locale inesistente.

Rimossi anche build M1 obsolete, dischi virtuali e cache di prove terminate e oggetti temporanei. Non cancellati sorgenti attivi, foto originali, font, Safari/WebKit, risorse vendor, copie necessarie degli originali o credenziali CLI. Gli artefatti prodotti per le nuove verifiche sono esterni al workspace; la pulizia finale conserva una sola build M0 utile.

## 2. Baseline: lacuna riconosciuta e rimediata

Il primo M0 aveva solo quattro hash dei file iniziali: **non era un inventario esteso né una verifica di ripetibilità**. Non viene presentato retroattivamente come completo.

Ora `baseline-manifest.json` registra 1306 file della baseline pubblicata `2d31862200898711817ab9c7decb724953728d25`, con hash Git, SHA-256 e dimensioni. I quattro hash originali coincidono. Le versioni originali dei file esistenti modificati sono state ricostruite dai commit immutabili 10.2.6 e pre-M0 e archiviate localmente, compresse; percorso e hash dell’archivio sono nel manifesto. È una ricostruzione effettuata ora, non una cattura retrodatata. La storia Git di lavoro rimane nel bundle locale; il riferimento remoto permette di recuperare anche la baseline storica completa senza conservarne inutilmente tutta la storia nel bundle.

Lettura GitHub senza scritture: tag 10.2.6 ancora sul commit originale, sei asset con nomi, dimensioni e digest SHA-256 uguali alle ricevute della pubblicazione. `release-10.2.6-readback.json` distingue la verifica attuale dei metadati dai download storici: non si afferma di aver riscaricato l’intera vecchia ISO in questo passaggio.

## 3. Inventario e revisione effettiva dei testi

**2337 letterali statici in 135 file**, includendo app, desktop/menu, setup/login, Recupero, header condivisi, boot e messaggi disco/filesystem. Inventario CSV, hash sorgenti e limiti in `tinymacOS/docs/audit-10.3/`.

**20 revisioni testuali, 21 occorrenze:** gergo `enrollment`/GOP/scheduler/algoritmi tolto dalle descrizioni ordinarie dove non necessario; formato nativo non spacciato per APFS/Btrfs Linux; snapshot locali esplicitamente non sostitutivi di una copia esterna contro il guasto del disco. Nessuna rimozione degli avvisi su perdita dati, chiavi, irreversibilità, interruzioni o consenso PMBR. Gli alias CLI storici restano compatibili. Diff testuale in `ui-changes.json`.

Il conteggio statico non è una matrice visuale completa e non risolve da solo concatenazioni dinamiche o messaggi di librerie terze. La revisione non consiste nel cancellare ogni termine tecnico: istruzioni BIOS/UEFI necessarie, diagnostiche esplicite e SHA-256 nell’ispettore rimangono.

## 4. Ripetibilità: trovato e corretto un problema reale

Il primo confronto di due build pulite **è fallito**: 20 byte diversi nel kernel corrispondevano ai nomi delle cartelle esterne nelle diagnostiche NTFS `__FILE__`. La differenza si propagava a Recovery, loader, payload e ISO. Ricevuta iniziale conservata in `build-repeatability-first-failure.json`, diagnosi in `repeatability-diagnosis.json`.

Corretto `tools/build-ntfs.py` con mapping dei percorsi del compilatore. Non è stato sostituito il driver, alterato il motore upstream o cancellato un errore: nei kernel restano i nomi relativi utili dei sorgenti NTFS, senza il percorso della macchina locale.

Dopo la correzione: **due build pulite separate, con cartelle di lunghezza diversa, producono 32 artefatti byte-identici, compresa la ISO completa**. Ricevute in `build-repeatability.json`, `build-environment.json` e `ntfs-diagnostic-paths.json`.

ISO sviluppo SHA-256:

`e313565922d75e7b6be079f9dce0cb653a40bc8e7bb41fd9626c614dbd07ebd6`

La ripetibilità qui vale sullo stesso ambiente/toolchain. Non è una prova cross-machine/cross-compiler e non certifica la correttezza funzionale da sola. Le stringhe pubbliche 10.2.6/30C1 sono mantenute intenzionalmente nella build d’integrazione.

## 5. Verifiche effettive

- **PASS M0 contract:** lexer dell’inventario; deriva sorgenti rilevata; testi di sicurezza richiesti; test negativo che rimuove un avviso e deve fallire; 568 input OS protetti e 177 file WebKit di sorgenti/risorse conservati.
- **PASS font:** ricostruzione esatta TFN2 e panic font esclusivamente dagli OTF forniti, nessun fallback, Mono 8 px e 207 glifi.
- **PASS foundation:** normali/sanitizzati per testo, streaming, dimensioni SI, CPU helper, gap Windows/GPT/PMBR, caret, selettore file e geometria/rendering M2; compilazione freestanding. Non è un nuovo PASS hardware o di tutta la matrice FS2/4Kn.
- **PASS NTFS normale e ASan/UBSan:** vero shrink/grow, file da 307200 byte riletto con `ntfscat`, verifica indipendente `ntfsresize`, volumi ibernati/corrotti rifiutati senza scritture, errore di scrittura iniettato senza fingere rollback. Solo fixture usa-e-getta.
- **PASS build:** quattro esecuzioni complessive (due iniziali non identiche, due corrette identiche); guardia binaria USB live/Recovery e rilettura FAT 4096 a 27+3 percorsi nelle build complete.
- **PASS guest sulla ISO finale ripetibile:** veri eventi USB virtuali e confronto dei pixel per setup/home col nome account, avviso snapshot sullo stesso disco, Monitor, Schermo e Recupero con versione su richiesta. Cinque verifiche mirate, non una matrice di ogni campo/app. Risultato `ui-guest-result.json`, screenshot in `evidence/`, log sotto `../logs/m0-*`.

## Stato corretto e limiti

M0 è ora consolidato nei suoi controlli di sorgenti, baseline, inventario e ripetibilità sul presente ambiente. Non viene certificata l’intera 10.3: restano audit e prove fisiche, matrice visuale completa, limiti di capacità/4Kn, contabilità RAM M3 e macroblocchi successivi. Il limite disco preesistente di 1 TiB non viene nascosto né rimosso tramite una modifica cosmetica. Safari resta escluso e i suoi sorgenti/risorse sono preservati. Nessuna release, nessun tag, nessuna operazione sui dischi reali dell’utente.
