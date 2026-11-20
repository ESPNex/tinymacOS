#!/usr/bin/env python3
"""Compile actual kernel stream sections with host lwIP/lock mocks, under ASan/UBSan."""
import hashlib
import json
import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path('/home/user/tinymacOS/kernel/net.c')
src = SOURCE.read_text()
parts = []
for name in ['STREAM_CALLBACKS', 'STREAM_READ']:
    a = src.index('/* ' + name + '_BEGIN')
    a = src.index('*/', a) + 2
    b = src.index('/* ' + name + '_END', a)
    parts.append(src[a:b])
fixture = (ROOT/'tests/kernel_stream_fixture.c').read_text()
fixture = fixture.replace('/* @KERNEL_STREAM_IMPLEMENTATION@ */', '\n'.join(parts))
out = Path('/var/tmp/tmc-webkit-stream-tests')
out.mkdir(exist_ok=True)
(out/'stream_test.c').write_text(fixture)
subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-g',
                '-fsanitize=address,undefined','-I'+str(ROOT/'native-abi'),
                str(out/'stream_test.c'),'-o',str(out/'stream_test')],check=True)
r = subprocess.run([str(out/'stream_test')], text=True, capture_output=True,check=True)
print(r.stdout,end='')
(ROOT/'test-results/kernel-stream.json').write_text(json.dumps({
    'kernel_net_sha256':hashlib.sha256(src.encode()).hexdigest(),
    'kind':'host tests compiling actual kernel function bodies; lwIP and IRQ locks mocked',
    'sanitizers':['address','undefined'],'stdout':r.stdout,'exit_code':r.returncode},indent=2))
