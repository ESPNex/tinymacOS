#!/usr/bin/env python3
"""Read-only, temporary download endpoint for a verified ISO kept outside workspace."""
from http.server import SimpleHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
import hashlib,json,os
ROOT=Path('/var/tmp/tmc-10.1/download');ROOT.mkdir(exist_ok=True)
ISO=Path('/var/tmp/tmc-10.1/tinymacOS-10.1-x86_64.iso')
report=json.loads((Path(__file__).parent/'final-qa.json').read_text())
assert report['passed'] and hashlib.file_digest(ISO.open('rb'),'sha256').hexdigest()==report['sha256']
link=ROOT/ISO.name
if not link.exists():link.symlink_to(ISO)
(ROOT/'SHA256SUMS.txt').write_text(report['sha256']+'  '+ISO.name+'\n')
preview=Path(__file__).parent/'centro-di-controllo.jpg'
if preview.exists():(ROOT/preview.name).write_bytes(preview.read_bytes())
(ROOT/'index.html').write_text('''<!doctype html><html lang="it"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>tinymacOS 10.1 Tramontana</title><style>*{box-sizing:border-box}body{margin:0;background:#081c29;color:#eef8fc;font:16px/1.6 system-ui,sans-serif}main{max-width:980px;margin:auto;padding:44px 24px}small{color:#9cdde6;letter-spacing:.1em}h1{font-size:clamp(32px,5vw,54px);line-height:1.13;margin:16px 0}p{max-width:760px;color:#c6d9e1}.actions{display:flex;gap:14px;flex-wrap:wrap;margin:28px 0}a{color:#8fd9e9}a.button{padding:13px 20px;border:1px solid #88d4e7;border-radius:30px;text-decoration:none;background:#16495b;color:white;font-weight:650}img{width:100%;border-radius:18px;border:1px solid #385768}code{font-size:12px;overflow-wrap:anywhere}footer{margin-top:28px;font-size:13px;color:#9db8c6}</style><main><small>BUILD 30B1 · x86-64 · BIOS / UEFI</small><h1>tinymacOS 10.1<br>Tramontana</h1><p>Centro di Controllo ridisegnato: moduli separati in vetro rifrattivo, pulsanti circolari, cursori per tema e sfondo e preferenze reali. Nessun comando audio o luminosità fittizio.</p><div class="actions"><a class="button" href="tinymacOS-10.1-x86_64.iso" download>Scarica la ISO · 215 MiB</a><a href="SHA256SUMS.txt" download>Checksum SHA-256</a></div><img src="centro-di-controllo.jpg" alt="Schermata reale del Centro di Controllo 10.1"><p><strong>SHA-256</strong><br><code>'''+report['sha256']+'''</code></p><footer>Download temporaneo di questa sessione, non una release pubblicata su GitHub. Collaudata in QEMU; nessuna certificazione del mouse sul computer fisico. Safari/WebKit non incluso. Usare un disco virtuale vuoto per le prove di installazione.</footer></main></html>''')
class Handler(SimpleHTTPRequestHandler):
    def __init__(self,*a,**kw):super().__init__(*a,directory=str(ROOT),**kw)
    def end_headers(self):
        if self.path.split('?')[0].endswith(('.iso','.txt')):self.send_header('Content-Disposition','attachment; filename="'+Path(self.path.split('?')[0]).name+'"')
        super().end_headers()
ThreadingHTTPServer(('0.0.0.0',8080),Handler).serve_forever()
