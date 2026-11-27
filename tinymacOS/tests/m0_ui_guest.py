"""M0 targeted UI wording smoke test on the development ISO.
Real virtual USB input and rendered pixels only. Not the exhaustive app matrix.
"""
from pathlib import Path
import hashlib
import json
import os
import tempfile
from retail_guest import Retail
from boot_recovery import RecoveryKey

iso = Path(os.environ['TMC_STAGE_ISO'])
qa = Path(os.environ['TMC_QA_DIR'])
qa.mkdir(parents=True, exist_ok=True)
w = Path(tempfile.mkdtemp(prefix='tmc-m0-ui-', dir='/var/tmp'))
ev = w / 'evidence'
ev.mkdir()
os.environ.update(TMC_MEM='512', TMC_SMP='2', TMC_POINTER='tablet')
r = {'passed': False, 'development_only': True, 'physical_hardware_tested': False,
     'exhaustive_app_matrix': False, 'work': str(w), 'checks': [],
     'iso_sha256': hashlib.file_digest(iso.open('rb'), 'sha256').hexdigest()}


def ok(text):
    r['checks'].append(text)
    print('PASS', text, flush=True)


class Recovery(RecoveryKey, Retail):
    recovery_text = 'Ripristina da una copia locale'


try:
    with Retail(iso, w, ev, 'm0-live', live=True) as g:
        g.enroll('AuditM0')
        g.visible('AuditM0')
        g.shot('account-home-label')
        ok('Setup with real USB input and Finder account home label still work')
        g.key('f2')
        g.visible('Sessione locale')
        g.cmd('open -a "Time Machine"')
        g.visible('Snapshot sullo stesso disco')
        g.visible('Non protegge dai guasti del disco.')
        g.visible('Nessun backup presente.')
        g.shot('local-snapshot-safety-wording')
        ok('Time Machine renders the local-only scope and disk-failure warning, not a false external backup promise')
        g.key('f8')
        g.visible('Campioni per intervallo')
        g.shot('monitor-interval-wording')
        ok('Monitor renders concise interval sampling text; this is not CPU/RAM accounting certification')
        g.key('f4')
        x, y = g.visible('Schermo')
        g.click(x + 15, y + 8)
        g.visible('Cambio risoluzione immediato disponibile.')
        g.shot('settings-display')
        ok('Display settings remain usable with actual emulated capabilities; no physical GOP claim')
    with Recovery(iso, w, ev, 'm0-recovery', live=True) as g:
        g.shot('recovery-home')
        x, y = g.visible('Aiuto', box=(0, 0, 1100, 34))
        g.click(x + 12, y + 8)
        x, y = g.visible('Info sul Recupero', box=(0, 34, 1200, 260))
        g.click(x + 20, y + 8)
        g.visible('Versione 10.2.6 · 30C1')
        g.shot('recovery-version-on-request')
        g.key('esc')
        g.visible('Ripristina da una copia locale')
        ok('Recovery USB access, clean home and explicit About menu work; public baseline version not prematurely changed')
    r['passed'] = True
except Exception as e:
    r['failure'] = str(e)
    raise
finally:
    (qa / 'm0-ui-result.json').write_text(json.dumps(r, indent=2) + '\n')
