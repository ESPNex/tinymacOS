#!/usr/bin/env python3
"""Verify exact pinned WTF dtoa source, emit a dependency-isolated port externally.
Only includes of four heavyweight WTF helper headers are redirected to our
explicit bounded support header. Two signed-arithmetic boundary fixes in strtod.cc are explicit below;
assertions retained. No claim that the generated numeric source is byte-equal.
This is a component port, never a complete WTF/JSC/WebCore build.
"""
from pathlib import Path
import sys,json,hashlib,difflib
r=Path(__file__).resolve().parents[1];source=r/'vendor/webkit-dtoa';out=Path(sys.argv[1]).resolve()
assert Path('/var/tmp') in out.parents,'Generated source stays outside workspace'
lock=json.loads((source/'UPSTREAM-LOCK.json').read_text());out.mkdir(exist_ok=True,parents=True)
patch=[];records={}
for name,expected in lock['sha256'].items():
 p=source/name;data=p.read_bytes();assert hashlib.sha256(data).hexdigest()==expected,name
 if p.suffix not in ('.cc','.h'):continue
 text=data.decode();ported=text
 for header in ('wtf/Assertions.h','wtf/StdLibExtras.h','wtf/text/StringCommon.h','wtf/ASCIICType.h'):
  ported=ported.replace('#include <'+header+'>','#include <tmc_dtoa_support.h>')
 if name=='strtod.cc':
  for before,after in [
   ('ASSERT(buffer.length() + exponent <= kMaxDecimalPower + 1);','ASSERT(static_cast<int>(buffer.length()) + exponent <= kMaxDecimalPower + 1);'),
   ('if (exponent + trimmed.length() - 1 >= kMaxDecimalPower) {','if (exponent + static_cast<int>(trimmed.length()) - 1 >= kMaxDecimalPower) {')]:
   assert ported.count(before)==1,before
   ported=ported.replace(before,after)
 dest=out/'wtf/dtoa'/name;dest.parent.mkdir(exist_ok=True,parents=True);dest.write_text(ported)
 patch.extend(difflib.unified_diff(text.splitlines(True),ported.splitlines(True),fromfile='upstream/'+name,tofile='native-port/'+name))
 records[name]={'upstream_sha256':expected,'ported_sha256':hashlib.sha256(ported.encode()).hexdigest()}
(out/'config.h').write_text('#pragma once\n#include <tmc_dtoa_support.h>\n')
(out/'dependency-isolation.patch').write_text(''.join(patch))
(out/'port-manifest.json').write_text(json.dumps({'commit':lock['commit'],'scope':'dependency-isolated WTF dtoa algorithms only','numeric_source_changes':'two explicit signed-boundary casts in strtod.cc, independently failing 0.1 regression before fix','assertions_retained':True,'files':records},indent=2)+'\n')
print('PASS pinned WTF dtoa preparation: dependency isolation + two explicit signed-boundary casts; upstream originals unchanged')
