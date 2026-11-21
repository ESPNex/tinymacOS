#!/usr/bin/env python3
"""Verify actual SVG sources, deterministic compilation and linked native masks."""
from pathlib import Path
import hashlib,json,subprocess,tempfile
from PIL import Image
q=Path(__file__).resolve().parent;home=q.parents[1];root=home/'tinymacOS';b=Path('/var/tmp/tmc-10.1.4/build')
with tempfile.TemporaryDirectory(prefix='tmc1014-svg-',dir='/var/tmp') as d:
 w=Path(d);subprocess.run(['python3',str(root/'tools/control-svg.py'),str(w)],check=True)
 masks=(w/'control-svg.alpha').read_bytes();assert masks==(b/'control-svg.alpha').read_bytes()
 assert masks in (b/'desktop.elf').read_bytes(),'Actual linked desktop must contain the SVG-derived masks'
 records=json.loads((w/'control-svg.json').read_text());assert len(records['symbols'])==13
 assert len(masks)==13*4096
 for j,item in enumerate(records['symbols']):
  m=Image.frombytes('L',(64,64),masks[j*4096:(j+1)*4096]);assert m.getbbox() and any(0<p<255 for p in m.tobytes())
  source=root/'assets/control-center'/(item['name']+'.svg');assert hashlib.sha256(source.read_bytes()).hexdigest()==item['sha256']
 c=(root/'desktop/control.c').read_text();assert 'alpha_symbol(control_svg_masks+id*4096,64,b,c)' in c
 assert 'static void seg(' not in c and 'static void frame(' not in c and 'switch(id)' in c
 assert 'void alpha_symbol' in (root/'desktop/gfx.c').read_text()
 r={'passed':True,'original_svg_sources':13,'source_is_svg':True,'native_runtime':'SVG-derived antialiased alpha masks tinted at runtime; not a runtime XML/SVG interpreter','masks_sha256':hashlib.sha256(masks).hexdigest(),'linked_actual_desktop_contains_masks':True,'independent_svg_compilation_identical':True,'symbols':records['symbols']}
 (q/'svg-contract-result.json').write_text(json.dumps(r,indent=2)+'\n');print('PASS 13 original SVGs, antialiased alpha, deterministic compiler, actual linked desktop uses masks, old C icon geometry removed')
