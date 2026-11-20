# tinymacOS 2.4.3 · Levante 21B4

[**Scarica la nuova ISO da Drive**](https://drive.google.com/file/d/1xpSCzs3WpJqzVbWzxp3C5XnfOJ_bjCJ4/view)

Ultima ISO intermedia richiesta dall'utente, prima del lavoro SMP/Safari. **Safari non incluso, fix SMP della VM non incluso.**

- File: `tinymacOS-2.4.3-x86_64.iso`
- Dimensione: **118.243.328 byte**
- SHA-256: `65b0191f0932e2ba3bdf815bac5da1a9d962b45a8baa2515b52bab90a21fefd1`
- MD5 remoto: `a87c91b14b6d9c3f37a7205d70415610`
- Upload finalizzato e metadati riletti da Drive: dimensione e SHA-256 identici al file locale. ACL non modificate.

## Diagnosi aggiornata

Il mouse funziona sul PC dell'utente in Recovery, con qualche ritardo. La riga bianca si sgretola muovendo il cursore. La precedente attribuzione del problema hardware all'enumerazione USB **non era dimostrata**. Recovery usa protocolli UEFI, il desktop il kernel/compositor propri: il successo in Recovery non prova che questi percorsi siano equivalenti.

Questa manutenzione corregge due problemi concreti del percorso desktop, senza modificare ulteriormente l'enumerazione xHCI della 2.4.2.

### Mouse relativo lento su desktop scalato

Nel vecchio desktop, a 1920×1080 ogni delta fisico veniva convertito separatamente al canvas logico da 1280 pixel: `1*1280/1920 == 0`. Gli spostamenti lenti di +/-1 si perdevano, perciò il cursore poteva sembrare fermo. Ora il compositor conserva i resti frazionari firmati su entrambi gli assi e li azzera al cambio modalità. Gli eventi di rotella non vengono più erroneamente trattati come coordinate pixel. Coordinate assolute mappate con estremi inclusivi.

**Confronto guest reale**, ISO di produzione, 2 vCPU e unico `usb-mouse`, 1920×1080 selezionato dalla vera UI Impostazioni:

| 30 report USB separati di (+1,0) | 2.4.2 | 2.4.3 |
|---|---:|---:|
| Movimento logico misurato dello sprite | 0 pixel | **20 pixel** |

Il test confronta i pixel opachi dello sprite originale, non soltanto un log o il puntatore QEMU. I report sono distanziati per evitare che QEMU li aggreghi. Harness: `qa/scaled_mouse.py`; risultati e screenshot in `evidence/scaled-mouse.json` e `*-1080p-*.png`.

Questo difetto è riprodotto e corretto; **non dimostra da solo che fosse l'unica causa del mouse sul PC dell'utente**, del quale non sono noti risoluzione/controller/VID:PID.

### Artefatti bianchi persistenti nel framebuffer

Il vecchio scanout confrontava il canvas solo con la copia RAM `shadow`: se il framebuffer fisico conteneva pixel vecchi/corrotti, ma lo shadow era corretto, questi non venivano riscritti finché un'altra variazione, ad esempio il cursore, non interessava il tile.

Correzioni:
- mapping della regione GOP come memoria device **UC**, anziché lasciare il mapping RAM write-back generico;
- barriera `sfence` dopo le scritture scanout;
- aggiornamento completo forzato ogni 100 tick nominali (circa un secondo), anche da fermo; la riparazione promuove una richiesta parziale a frame completo;
- mantenuta la cache dei pixel per gli altri frame, evitando di riscrivere sempre tutto il framebuffer.

**Confronto guest con iniezione controllata:** scritta una banda bianca di 224×4 pixel nella vera memoria GOP emulata via debugger QEMU, senza alterare canvas/shadow né la ISO. Dopo la ripresa, nessun mouse o tasto:

| Banda iniettata nel campo password | 2.4.2 | 2.4.3 |
|---|---:|---:|
| Pixel bianchi rimasti dopo l'attesa | 896 | **0** |
| Regione identica allo screenshot pulito | No | **Sì** |

Harness `qa/scanout_injection.py`, confronto e schermate `evidence/scanout-injection.json`, `before/after-{clean-setup,injected-band,after-idle-no-mouse}.png`. È una **riproduzione sintetica di un artefatto persistente**, non la prova della causa originaria sulla GPU fisica. Non viene promessa l'eliminazione di qualunque artefatto hardware.

### Password compatta

Ripristinato il passo precedente: **9 pixel, non 12**, con fino a 19 puntini visibili. Nessun caret nei campi segreti. Non viene confuso questo cambiamento estetico con la riparazione dello scanout.

## Altre verifiche

- ASan/UBSan: vero `video.c` con scanout RAM simulato, RGB/BGR, stride con padding, scalato/non scalato, riparazione di corruzione e promozione di rettangoli parziali. La baseline fallisce la prova di riparazione.
- Conservazione dei delta firmati: 10.000 report per direzione su 5 geometrie, ritorno all'origine senza deriva.
- QEMU: mouse e tablet, controller standard e secondo xHCI, setup mediante clic, trascinamento Finder, menu contestuale e shell Recovery: PASS.
- Password 0/1/16/64 caratteri, più fasi temporali, creazione account, blocco e riautenticazione: PASS.
- Nuova USB GPT/FAT32 da ISO estratta, sola lettura, 2 vCPU, **nessun CD** e framebuffer setup verificato: PASS.
- Installer reale su SATA sacrificabile, spegnimento, avvio/accesso senza ISO e `sw_vers 2.4.3`: PASS. ESP e Recovery installate identiche byte per byte al supporto. Prova con NVRAM riutilizzata, non NVRAM nuova.
- Suite backend/runtime WebKit: **15/15 PASS**, inclusi guest rete e VM su profili 1/2 vCPU. La VM continua a rifiutare le mutazioni con più CPU: **non è una prova che SMP VM sia già risolto**. Nessun JSC/WebCore/HTTPS browser test completato.

## Chiavetta Rufus

**La scrittura cancella la chiavetta. Controllare con attenzione la destinazione.**

1. Seleziona la nuova ISO 2.4.3.
2. **GPT / UEFI senza CSM / FAT32**.
3. Se richiesto scegli modalità **ISO, non DD**: questa ISO non è un disco hybrid.
4. **Secure Boot disattivato**, poi avvio dal menu UEFI della nuova chiavetta.

Un'installazione vecchia non si aggiorna automaticamente avviando il disco interno. Nessun test Windows/Rufus o hardware fisico è stato eseguito. Restano i limiti USB documentati: xHCI/root ports, niente hub/hotplug generali o EHCI/OHCI/UHCI, layout HID complessi non tutti supportati.

## Sorgenti conservati prima della build

Tutti i sorgenti erano già sotto `/home/user` prima della compilazione; copia integrale estratta della release in `sources/`, manifest `source-sync.json`. Non dipende dalla conservazione della build in /var/tmp. Il codice di sviluppo può proseguire separatamente senza cambiare l'ISO già caricata.

```sh
cd /home/user/workspace/maintenance-2.4.3/sources
python3 tools/local-tools.py /var/tmp/tmc/tools/root
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc-rebuild-243 bash build.sh
```

Serve rete per ripristinare i pacchetti della toolchain bloccata; vendor/asset OS sono estratti. Nessuna ISO o archivio sorgenti sotto `/home/user`. Il controllo del budget locale non conferma da solo la persistenza dello snapshot della piattaforma.

**Dopo questa consegna:** sviluppo SMP reale e runtime/WebKit/Safari, senza nuove ISO di manutenzione salvo nuova autorizzazione. Browser pronto soltanto dopo motore target e test reali HTTPS/HTML/CSS/JavaScript, UI classica e tutti i temi.
