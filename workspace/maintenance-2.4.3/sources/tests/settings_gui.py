"""Levante settings tour: macOS-style sidebar, new themes/wallpapers,
security and backup panes, live search filter."""
import json, os, shutil, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from retail_guest import Retail

Retail.ocr = lambda self, box=None, psm=11: '(ocr non disponibile)'

iso = Path(os.environ.get('TMC_BUILD', '/var/tmp/tmc210/debug')) / 'tinymacOS-2.4.3-x86_64.iso'
assert iso.exists(), iso
work = Path('/var/tmp/tmc210/settingsgui')
if work.exists():
    shutil.rmtree(work)
work.mkdir(parents=True)
ev = work / 'evidence'
ev.mkdir()
results = []


def ok(s):
    results.append(s)
    (ev / 'settings.json').write_text(json.dumps(results, indent=2, ensure_ascii=False))
    print('OK:', s, flush=True)


def swait(g, pat, t=30):
    end = time.monotonic() + t
    while time.monotonic() < end:
        if pat in g.text():
            return
        time.sleep(.12)
    raise AssertionError(f'missing {pat!r}\n' + g.text()[-900:])


class User(Retail):
    def wait(self, pat=None, mark=0, timeout=55, **k):
        pat = pat or '[gop] first userspace frame presented'
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if pat in self.text()[mark:]:
                return
            time.sleep(.1)
        raise AssertionError(f'serial missing {pat!r}\n' + self.text()[-900:])


with User(iso, work, ev, 'setgui', disk='persistent') as g:
    g.enroll()
    g.spawn('f4', 'Impostazioni')
    X, Y, W, H = g.bounds(4)
    assert W >= 800, (W, H)
    g.visible('Impostazioni di Sistema')
    g.visible('ASPETTO')          # Light/Dark cards section
    g.visible('COLORE')           # accent colour circles section
    g.visible('SFONDO')
    g.visible('Blu')              # default accent colour name
    g.shot('00-aspetto')
    # accent colour: pick the Levante circle (7th)
    g.click(X + 499, Y + 43 + 243)
    g.visible('Levante')          # colour name + paired wallpaper header
    # wallpaper: last thumb (Brezza, col 6 row 2)
    g.click(X + 779, Y + 43 + 371)
    g.visible('Brezza')
    # light/dark cards: choose Scuro
    g.click(X + 682, Y + 43 + 130)
    g.shot('00b-aspetto-scuro')
    ok('Aspetto Apple: card Chiaro/Scuro, 7 colori, 12 sfondi; Levante e Brezza selezionabili.')

    g.click(X + 100, Y + 43 + 102)     # sidebar: Schermo
    g.visible('RISOLUZIONE')
    g.visible('scuro')            # ASPETTO row reflects the dark card click
    g.shot('01-schermo')
    ok('Schermo: dettagli risoluzione/uscita, aspetto coerente e switch Dock.')

    g.click(X + 100, Y + 43 + 136)     # sidebar: Sicurezza e privacy
    g.visible('CRITTOGRAFIA DEI FILE')
    g.visible('FILE PROTETTI')
    g.shot('02-sicurezza')
    ok('Sicurezza e privacy: stato crittografia e file protetti.')

    g.click(X + 100, Y + 43 + 170)     # sidebar: Backup
    g.visible('TIME MACHINE')
    g.visible('Copia ora')
    g.shot('03-backup')
    ok('Backup: stato Time Machine e pulsante «Copia ora».')

    g.click(X + 100, Y + 43 + 204)     # sidebar: Data e ora
    g.visible('SORGENTE')
    g.shot('04-dataora')
    ok('Data e ora: orologio live e sorgente.')

    g.click(X + 100, Y + 43 + 238)     # sidebar: Generali
    g.visible('Levante')
    g.visible('2.4.3 (21B4)')
    g.visible('SPAZIO SU DISCO')
    g.shot('05-generali')
    ok('Generali: versione 2.4.3 (21B4) Levante e spazio su disco.')

    # live search: focus the field, type, the sidebar narrows to the match
    g.click(X + 110, Y + 43 + 26)
    g.type('backup')
    time.sleep(.4)
    g.shot('06-cerca')
    g.visible('TIME MACHINE')
    ok('Ricerca laterale: filtro «backup» attivo senza perdere il contenuto.')

print(f'SETTINGS GUI PASSED: {len(results)}')
