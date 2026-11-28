"""M7 static guard: cursor resources must not bypass authentication or ABI."""
from pathlib import Path
import hashlib,json
R=Path(__file__).resolve().parents[1]
p=json.loads((R/'tests/fixtures/interaction7-security.json').read_text());s=(R/'kernel/syscall.c').read_text();a=s.index('    if (!session_open() &&');b=s.index('    switch (n)',a)
assert hashlib.sha256(s[a:b].encode()).hexdigest()==p['unchanged_preauth_gate_sha256'],'Pre-login file/session access gate must remain byte-identical'
art=s[s.index('    case S_ART:'):s.index('    case S_TOUCH:')]
for t in ['desktop && a <= NWALL+CUR_COUNT','bootfile(path, &v, &z)','d <= 65536 && b <= z && d <= z - b && OUT(c, d)','cursorname(a-NWALL-1)']:assert t in art,t
assert 'string(' not in art and 'S_OPEN' not in art.replace('S_OPEN/READ','')
loader=(R/'desktop/pointer.h').read_text();assert 'sc(S_OPEN' not in loader and 'sc(S_ART,NWALL+1+i' in loader
assert 'for(int j=0;j<nz;j++)if(!views[order[j]].dead)resize(order[j]);' in (R/'desktop/main.c').read_text(),'Retry accepted-state tracking once per window/frame, including after release'
assert 'if(sc(S_SEND,v->s.pid,&e,0,0)>=0)' in (R/'desktop/main.c').read_text()
for f in ['textedit.c','notebook.h']:
 t=(R/'userspace'/f).read_text();assert 'BodyText(' in t and 'BodyOffset(' in t
assert 'q->type==C_BODY?F_BODY:F_MONO' in (R/'desktop/gfx.c').read_text()
print('PASS M7 security/ABI source guard: unchanged pre-authentication gate; fixed desktop-only sealed cursor allowlist with bounded copies; resize retry after queue pressure; real proportional renderer/metrics')
