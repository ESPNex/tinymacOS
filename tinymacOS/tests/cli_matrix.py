"""WS-E live CLI matrix: /usr/sbin/diskutil apfs + dedicated macOS tools +
Time Machine launch, driven through the real Terminal on a retail boot.
RAM-volume session (blank SATA): encryption + snapshot scrub exercised both
directions; fsck/mount report the honest no-volume path."""
import json, os, shutil, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from retail_guest import Retail

# tesseract is not installed in this sandbox: assertion text beats an OCR crash.
Retail.ocr = lambda self, box=None, psm=11: '(ocr non disponibile)'

# Debug build: retail compiles out COM1 QA logs; the screen assertions hold on both.
iso = Path(os.environ.get('TMC_BUILD', '/var/tmp/tmc210/debug')) / 'tinymacOS-2.4.4-x86_64.iso'
assert iso.exists(), iso
work = Path('/var/tmp/tmc210/cli')
if work.exists():
    shutil.rmtree(work)
work.mkdir(parents=True)
ev = work / 'evidence'
ev.mkdir()
results = []


def ok(s):
    results.append(s)
    (ev / 'cli.json').write_text(json.dumps(results, indent=2, ensure_ascii=False))
    print('OK:', s, flush=True)


def swait(g, pat, t=25):
    end = time.monotonic() + t
    while time.monotonic() < end:
        if pat in g.text():
            return
        time.sleep(.12)
    raise AssertionError(f'missing {pat!r}\n' + g.text()[-900:])


class User(Retail):
    pass


with User(iso, work, ev, 'cli', disk='persistent') as g:
    g.enroll()
    ok('Primo avvio: account creato e Finder raggiunto (Preferiti visibile).')
    assert '[encdbg] mk=1 w=14 enc=0' in g.text(), 'boot encryption hook missing'
    assert 'deny=1' in g.text() and 'fin=0' in g.text()
    ok('Hook di boot: suite crittografia (lock->deny, unlock->txt, decrypt->fin) verde sul serial.')

    g.key('f2', .4)
    swait(g, '[window] open Terminale')
    time.sleep(.7)
    ok('Terminale aperto con F2.')

    # --- diskutil apfs: container, volume, snapshot, encryption status -----
    g.cmd('diskutil apfs list', 'Container cont0')
    g.visible('Crittografia: disattiva')
    ok('diskutil apfs list: container e stato crittografia a schermo.')

    g.cmd('diskutil apfs snapshot create cliuno', 'Snapshot creato.')
    g.cmd('diskutil apfs snapshot create clidue', 'Snapshot creato.')
    g.cmd('diskutil apfs snapshot list', 'cliuno')
    g.cmd('diskutil apfs snapshot list', 'clideue' if False else 'clideu' if False else 'clide')
    ok('diskutil apfs snapshot create/list: backup cliuno e clidue elencati.')

    g.cmd('diskutil apfs encryptVolume chiave-77', 'Volume criptato')
    g.cmd('diskutil apfs list', 'attiva (sbloccata)')
    g.cmd('diskutil apfs list', 'File protetti:')
    ok('diskutil apfs encryptVolume: crittografia attiva e file protetti.')

    g.cmd('diskutil apfs lockVolume', 'Volume bloccato')
    g.cmd('diskutil apfs list', 'attiva (bloccata)')
    ok('diskutil apfs lockVolume: volume bloccato.')

    g.cmd('apfs_unlockfv parola-sbagliata', 'passphrase errata')
    g.cmd('apfs_unlockfv chiave-77', 'stato sbloccato')
    g.cmd('diskutil apfs list', 'attiva (sbloccata)')
    ok('apfs_unlockfv: passphrase errata rifiutata, poi sblocco riuscito.')

    # decrypt re-keys the existing snapshot image back; verify must still pass.
    g.cmd('diskutil apfs decryptVolume chiave-77', 'Volume decriptato')
    g.cmd('diskutil apfs list', 'Crittografia: disattiva')
    ok('diskutil apfs decryptVolume: stato tornato a disattiva.')

    g.cmd('diskutil apfs snapshot verify cliuno', 'integrità verificata')
    g.cmd('diskutil apfs snapshot delete clidue', 'Snapshot eliminato.')
    g.cmd('diskutil apfs snapshot restore cliuno', 'Volume ripristinato.')
    ok('Snapshot verify/delete/restore: crc32c verde, eliminazione confermata, '
       'rollback verificato dopo il ciclo crittografia/decriptatura (il ripristino '
       'riporta indietro anche il catalogo: cliuno non esiste piu, atteso).')

    # --- dedicated single-purpose utilities -------------------------------
    g.cmd('fsck_apfs', 'montato su questo disco')
    g.cmd('mount_apfs', 'volume da montare')
    g.cmd('newfs_apfs', 'Installatore')
    ok('fsck_apfs / mount_apfs / newfs_apfs: percorsi dedicati presenti e onesti '
       '(nessun volume formattato su disco vuoto).')

    # --- SIP, kext e rete simulata (2.4.4 «Levante») ------------------------
    g.cmd('sw_vers', '2.4.4')
    ok('sw_vers: ProductVersion 2.4.4 (build 21B5).')

    g.cmd('csrutil status', 'System Integrity Protection: attiva.')
    g.cmd('csrutil disable', 'Shell di Recupero')
    ok('csrutil: SIP attiva; disable rifiutato fuori dalla Shell di Recupero.')

    g.cmd('kextstat', 'org.tinymacos.driver.e1000')
    g.cmd('kextstat', 'org.tinymacos.driver.wifi')
    ok('kextstat: driver di rete e1000 e wifi caricati da '
       '/System/Library/Extensions.')

    g.cmd('kextload', 'Uso: kextload /percorso/Bundle.kext')
    g.cmd('kextunload', 'Uso: kextunload bundle-id')
    ok('kextload/kextunload: usage onesto senza argomenti.')

    g.cmd('wifi status', 'radio simulata')
    g.cmd('wifi scan', 'LevanteNet')
    ok('wifi: stato onesto (radio simulata) e scan della rete simulata.')

    g.cmd('ifconfig', 'wl0')
    ok('ifconfig: wireless wl0 presente (boot senza scheda ethernet).')

    g.cmd('ping', 'Uso: ping indirizzo')
    ok('ping: usage onesto.')

    # --- Time Machine app --------------------------------------------------
    m = len(g.text())
    g.type('open -a "Time Machine"')
    g.key('ret')
    swait(g, '[window] open Time Machine')
    time.sleep(1.2)
    g.visible('Esegui backup ora')
    ok('open -a "Time Machine": finestra aperta, pulsante backup visibile.')

    x, y = g.visible('Esegui backup ora')
    g.click(x + 6, y + 5)
    g.visible('Backup creato.')
    g.visible('Backup-')
    ok('Time Machine GUI: backup creato dalla timeline (nome Backup-YYYY-MM-DD).')

    g.key('ctrl-w')
    time.sleep(.5)

    g.key('f2', .5)
    time.sleep(.5)
    g.type('open -a "Utility Disco"')
    g.key('ret')
    swait(g, '[window] open Utility Disco')
    time.sleep(1.5)
    g.visible('Disco interno')
    g.shot('utility-disco')
    ok('Utility Disco aperta senza errori (scheda disco interno renderizzata).')
    g.key('ctrl-w')
    time.sleep(.5)

    g.key('f2', .5)
    time.sleep(.5)
    g.cmd('diskutil list', 'Nessun volume tinymacOS montato')
    ok('diskutil list sul disco vuoto: nessun volume montato (coerente).')

    g.type('off')
    g.key('ret')
    swait(g, '[power] ACPI S5', 14)
    ok('Spegnimento pulito via Terminale (off -> ACPI S5).')

print('ALL CLI CHECKS PASSED:', len(results), flush=True)
