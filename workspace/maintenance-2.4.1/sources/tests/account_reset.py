"""Levante account recovery: an unreadable account fails closed on boot, the
labeled two-step button re-opens enrollment, and user files stay untouched.
The fixture is a crafted TMCBFS1 disk with garbage in both account sectors."""
import json, os, shutil, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from retail_guest import Retail

Retail.ocr = lambda self, box=None, psm=11: '(ocr non disponibile)'

iso = Path(os.environ.get('TMC_BUILD', '/var/tmp/tmc210/debug')) / 'tinymacOS-2.4.1-x86_64.iso'
assert iso.exists(), iso
work = Path('/var/tmp/tmc210/accreset')
if work.exists():
    shutil.rmtree(work)
work.mkdir(parents=True)
ev = work / 'evidence'
ev.mkdir()
results = []


def ok(s):
    results.append(s)
    (ev / 'accreset.json').write_text(json.dumps(results, indent=2, ensure_ascii=False))
    print('OK:', s, flush=True)


def swait(g, pat, t=30):
    end = time.monotonic() + t
    while time.monotonic() < end:
        if pat in g.text():
            return
        time.sleep(.12)
    raise AssertionError(f'missing {pat!r}\n' + g.text()[-900:])


# --- CRC32C (Castagnoli, reflected) exactly as the kernel computes it -------
def crc32c(data):
    c = 0xFFFFFFFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ (0x82F63B78 & -(c & 1))
    return c ^ 0xFFFFFFFF


NSLOTS, DATA, SUPER, SNAPCAT, NODE0 = 384, 144, 128, 136, 64
LESP, LDATA, Z = 2048, 51200, 196608          # 96 MiB target, type-1 layout (ESP 24M)
WELCOME = ("Benvenuti in tinymacOS 2.4.1 Levante.\n\nQuesta cartella personale è condivisa dalle app. "
           "Conserva i file in Documents. Finder mostra il sistema realmente avviato in /System/Library/KernelCollections.\n\n"
           "Ctrl+Spazio apre Cerca. Dal Centro di Controllo puoi regolare vetro e movimento.\n")
SYSTEM_PLIST = ('<?xml version="1.0"?><plist version="1.0"><dict><key>ComputerName</key>'
                '<string>Levante</string></dict></plist>\n')
ESP_TYPE = bytes([0x28,0x73,0x2a,0xc1,0x1f,0xf8,0xd2,0x11,0xba,0x4b,0,0xa0,0xc9,0x3e,0xc9,0x3b])
DAT_TYPE = bytes([0x34,0xbc,0xe9,0x28,0x9c,0x36,0x43,0x48,0xb5,0xae,0x18,0x9c,0x51,0x32,0x70,0xd0])


def node(kind, ro, parent, size, name):
    return struct.pack('<BBHII48sI', kind, ro, parent, size, 1, name.encode().ljust(48, b'\0'), 0)


def guid(tag):
    import hashlib
    g = bytearray(hashlib.sha256(b'Orizzonte partition ID' + bytes([tag])).digest()[:16])
    g[7] = (g[7] & 15) | 64
    g[8] = (g[8] & 63) | 128
    return bytes(g)


def part(utype, uid, start, end, name):
    e = bytearray(128)
    e[0:16] = utype
    e[16:32] = uid
    struct.pack_into('<QQ', e, 32, start, end)
    for j, ch in enumerate(name[:35]):
        struct.pack_into('<H', e, 56 + j * 2, ord(ch))
    return bytes(e)


def crc32(data):  # standard CRC-32, as storage/maintenance.c (GPT)
    import zlib
    return zlib.crc32(data) & 0xffffffff


def crc32c(data):  # Castagnoli, as fs/btrfs.c super_ok
    c = 0xFFFFFFFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ (0x82F63B78 & -(c & 1))
    return c ^ 0xFFFFFFFF


def craft(path):
    img = bytearray(Z * 512)
    # --- GPT: primary LBA1, backup Z-1, entries LBA2 / Z-33 (type-1 layout) --
    entries = bytearray(128 * 128)
    entries[0:128] = part(ESP_TYPE, guid(1), LESP, LDATA - 1, 'tinymacOS EFI')
    entries[128:256] = part(DAT_TYPE, guid(2), LDATA, Z - 34, 'Macintosh HD - Dati')
    ecrc = crc32(bytes(entries))

    def header(at, other, ent_lba):
        h = bytearray(92)
        h[0:8] = b'EFI PART'
        struct.pack_into('<IIII', h, 8, 0x10000, 92, 0, 0)
        struct.pack_into('<QQQQ', h, 24, at, other, 34, Z - 34)
        h[56:72] = guid(0)
        struct.pack_into('<QII', h, 72, ent_lba, 128, 128)
        struct.pack_into('<I', h, 88, ecrc)
        c = crc32(bytes(h))
        struct.pack_into('<I', h, 16, c)
        return bytes(h)

    _h0 = header(1, Z - 1, 2)
    img[512:512 + len(_h0)] = _h0
    img[2 * 512:2 * 512 + len(entries)] = entries
    _h1 = header(Z - 1, 1, Z - 33)
    img[(Z - 1) * 512:(Z - 1) * 512 + len(_h1)] = _h1
    img[(Z - 33) * 512:(Z - 33) * 512 + len(entries)] = entries
    mbr = bytearray(512)
    mbr[450] = 0xee
    mbr[448] = 2
    mbr[451] = mbr[452] = mbr[453] = 255
    struct.pack_into('<I', mbr, 454, 1)
    struct.pack_into('<I', mbr, 458, Z - 1)
    mbr[510] = 85
    mbr[511] = 170
    img[0:512] = mbr

    # --- volume content at LDATA (block offset for non-install mounts) ------
    base = LDATA * 512

    def put(sec, blob):
        off = base + sec * 512
        img[off:off + len(blob)] = blob

    hdr = bytearray(512)
    hdr[0:8] = b'TMCBFS1\0'
    # u32[0..1] overlap the magic; header() reads [2]=SNAPREV [3]=NSLOTS [4]=DATA [5]=active
    hdr[8:24] = struct.pack('<IIII', 2, NSLOTS, DATA, 1)
    put(0, bytes(hdr))
    nodes = [node(1, 1, 0, 1, '')]
    nodes.append(node(1, 1, 0, 0, 'Users'))
    nodes.append(node(1, 0, 1, 0, 'User'))
    for n in ('Desktop', 'Documents', 'Downloads', 'Pictures'):
        nodes.append(node(1, 0, 2, 0, n))
    nodes.append(node(2, 0, 2, len(WELCOME.encode()), 'Welcome.txt'))
    nodes.append(node(1, 1, 0, 0, 'Volumes'))
    nodes.append(node(1, 0, 0, 0, 'Library'))
    nodes.append(node(1, 0, 8, 0, 'Application Support'))
    nodes.append(node(1, 0, 8, 0, 'Preferences'))
    nodes.append(node(1, 0, 2, 0, 'Library'))
    nodes.append(node(1, 0, 11, 0, 'Preferences'))
    nodes.append(node(1, 0, 11, 0, 'Application Support'))
    nodes.append(node(2, 1, 10, len(SYSTEM_PLIST), 'com.tinymacos.system.plist'))
    nodes.append(node(2, 1, 12, 5, 'com.tinymacos.appearance'))
    assert len(nodes) <= NSLOTS
    blob = b''.join(nodes).ljust(NSLOTS * 64, b'\0')
    for sec in range(NSLOTS // 8):
        put(NODE0 + sec, blob[sec * 512:(sec + 1) * 512])
    put(DATA + 7 * 16, WELCOME.encode())
    put(DATA + 15 * 16, SYSTEM_PLIST.encode())
    put(DATA + 16 * 16, b'\0' * 5)
    sb = bytearray(4096)
    sb[0x40:0x48] = b'_BHRfS_M'
    struct.pack_into('<I', sb, 0, crc32c(bytes(sb[32:4096])))
    put(SUPER, bytes(sb[:512]))
    put(SUPER + 1, bytes(sb[512:]))
    # ... remaining super sectors are zeros already
    cat = bytearray(512)
    cat[0:8] = b'SNAPCAT1'
    struct.pack_into('<II', cat, 8, 2, 0)   # word[2]=SNAPREV, word[3]=count
    put(SNAPCAT, bytes(cat))
    put(13, b'\xa5' * 512)
    put(14, b'\x5a' * 512)
    path.write_bytes(bytes(img))


class User(Retail):
    def wait(self, pat=None, mark=0, timeout=55, **k):
        pat = pat or '[gop] first userspace frame presented'
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if pat in self.text()[mark:]:
                return
            time.sleep(.1)
        raise AssertionError(f'serial missing {pat!r}\n' + self.text()[-900:])


disk = work / 'corrupt' / 'system.raw'
disk.parent.mkdir(parents=True, exist_ok=True)
craft(disk)
before = disk.read_bytes()

with User(iso, work, ev, 'accreset', disk='corrupt') as g:
    swait(g, '[session] unreadable account; fail closed')
    g.visible('Account non dispensibile'.replace('dispensibile', 'disponibile'))
    assert '[window] open' not in g.text(), 'fail closed must not start the desktop'
    g.shot('00-account-rotto')
    ok('Disco con account illeggibile: fail closed, nessuna finestra, schermo «Account non disponibile».')

    g.click(640, 656)                      # first click arms
    g.visible('Conferma: azzera l’account')
    g.shot('01-conferma-armata')
    ok('Primo clic sul pulsante: si arma la conferma inlinea.')

    g.click(640, 656)                      # second click resets
    swait(g, '[session] account reset; enrollment re-opened')
    g.visible('Crea un account')
    g.shot('02-crea-account')
    ok('Secondo clic: account azzerato, form di creazione account tornata disponibile.')

    g.enroll()
    swait(g, '[session] verifier enrolled')
    g.shot('03-desktop')
    ok('Nuovo account creato e desktop raggiunto (Preferiti visibile).')

    # installed-style volume is really mounted: real fs read + Disk Utility pair
    g.spawn('f2', 'Terminale')
    g.cmd('cat /Users/User/Welcome.txt', 'Benvenuti in tinymacOS 2.4.1 Levante')
    ok('Volume montato: Terminal legge Welcome.txt dal disco installato.')
    g.cmd('diskutil apfs list', 'Crittografia: disattiva')
    ok('WS-E: diskutil apfs list vede il volume con la coppia di stato crittografia.')
    g.cmd('open -a \"Utility Disco\"')
    swait(g, '[window] open Utility Disco')
    time.sleep(1.2)
    import re as _re
    x, y, w, h = map(int, _re.findall(r'\[user:Scrivania\] view \d+ (\d+) (\d+) (\d+) (\d+)', g.text())[-1])
    ok('Utility Disco su volume montato: ' + str((x, y, w, h)) + '.')
    assert x >= 0 and x + w <= 1280, f'finestra fuori schermo {x},{y},{w},{h}'
    assert y >= 0 and y + h + 3 <= 720, f'finestra fuori schermo {x},{y},{w},{h}'
    g.visible('Disco interno')
    g.click(x + 46, y + 43 + 129)   # riga «Dati» nella sidebar (view=1)
    time.sleep(.5)
    g.visible('Macintosh HD - Dati')
    g.shot('03b-utility-disco')
    ok('Utility Disco: volume «Dati» selezionato, header e barra renderizzati.')

# offline: accounts are a valid pair again; welcome file untouched
after = disk.read_bytes()
assert after != before
a, b = after[(LDATA + 13) * 512:(LDATA + 14) * 512], after[(LDATA + 14) * 512:(LDATA + 15) * 512]
magic, ver, rounds, name, salt, verifier = struct.unpack_from('<8sII32s16s32s', a)
assert magic == b'TMCUSER2' and ver == 1 and rounds == 120000
assert name.split(b'\0')[0] == b'Elena' and salt != bytes(16) and a == b
import hashlib
assert hashlib.pbkdf2_hmac('sha256', b'local-test-285', salt, rounds) == verifier
_wb = WELCOME.encode()
_w = after[(LDATA + DATA + 7 * 16) * 512:(LDATA + DATA + 7 * 16) * 512 + len(_wb)]
if _w != _wb:
    print('WELCOME MISMATCH len_got=%d len_want=%d got_tail=%r want_tail=%r' % (
        len(_w), len(_wb), _w[-20:], _wb[-20:]), flush=True)
assert _w == _wb
assert after[LDATA * 512:LDATA * 512 + 8] == b'TMCBFS1\0'
ok('Verifica offline: record account validi (PBKDF2 verificato), Welcome.txt e header intatti.')

print(f'ACCOUNT RESET PASSED: {len(results)}')
