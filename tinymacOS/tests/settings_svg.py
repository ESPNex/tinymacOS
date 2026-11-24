#!/usr/bin/env python3
"""Host contract for the six SVG-backed Settings sidebar symbols."""
from pathlib import Path
import importlib.util, tempfile, struct, re
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('settings_svg', ROOT/'tools/settings-svg.py')
svg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(svg)
with tempfile.TemporaryDirectory(prefix='tmc-settings-svg-', dir='/var/tmp') as tmp:
    dst = Path(tmp)
    svg.build(dst)
    first = (dst/'settings-svg.alpha').read_bytes()
    svg.build(dst)
    assert first == (dst/'settings-svg.alpha').read_bytes()
    assert len(first) == 6*4096
    masks = [first[i*4096:(i+1)*4096] for i in range(6)]
    assert len(set(masks)) == 6
    for name, mask in zip(svg.NAMES, masks):
        assert any(0 < a < 255 for a in mask), name
        assert 255 in mask and 0 in mask, name
        assert mask == svg.renderer.render(ROOT/'assets/settings'/(name+'.svg')).tobytes()
        small = svg.renderer.render(ROOT/'assets/settings'/(name+'.svg'), 18)
        assert small.getbbox(), name
abi = (ROOT/'include/abi.h').read_text()
assert [int(n) for n in re.findall(r'ICON_SETTINGS_\w+\s*=\s*(-\d+)', abi)] == list(range(-100,-106,-1))
assert all(struct.unpack('b', struct.pack('B', n & 255))[0] == n for n in range(-100,-106,-1))
settings = (ROOT/'userspace/settings.c').read_text()
assert 'Icon(14, y + 2, 18, pglyph[i])' in settings
assert '-pglyph[i]' not in settings
print('PASS six unique SVG sources; reproducible antialiased masks; signed icon ABI; no old sprite path in Settings')
