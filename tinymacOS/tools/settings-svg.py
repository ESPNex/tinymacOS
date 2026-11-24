#!/usr/bin/env python3
"""Compile Settings' six original SVG sources using the strict SVG mask renderer.
SVG is the source of truth; no legacy glyph/sprite fallback. No runtime parser.
"""
from pathlib import Path
import hashlib, importlib.util, json, sys
ROOT = Path(__file__).resolve().parents[1]
NAMES = ('appearance', 'display', 'privacy', 'backup', 'clock', 'general')
spec = importlib.util.spec_from_file_location('control_svg', ROOT/'tools/control-svg.py')
renderer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(renderer)

def build(dst):
    dst = Path(dst)
    dst.mkdir(parents=True, exist_ok=True)
    masks, records = [], []
    for name in NAMES:
        path = ROOT/'assets/settings'/(name+'.svg')
        mask = renderer.render(path)
        assert mask.getbbox(), name
        masks.append(mask.tobytes())
        records.append({'name': name, 'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    data = b''.join(masks)
    assert len(data) == 6*64*64
    (dst/'settings-svg.alpha').write_bytes(data)
    (dst/'settings-svg.json').write_text(json.dumps({
        'source': 'assets/settings/*.svg', 'symbols': records,
        'rasterization': '8x supersampled alpha, 64px mask; native tinted rendering',
        'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
    }, indent=2)+'\n')
    print('Settings SVG: six original SVG sources, deterministic antialiased alpha masks')

if __name__ == '__main__':
    build(sys.argv[1])
