#!/usr/bin/env python3
"""Validate active documentation and canonical release notes, without building media.
Use --github to also compare the eleven published bodies (requires authenticated gh).
Historical snapshots, license texts and frozen evidence are deliberately not rewritten.
"""
from pathlib import Path
import argparse, collections, json, os, re, shutil, subprocess
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = Path(__file__).with_name('releases.json')
ACTIVE = ['README.md', 'CONTRIBUTING.md', 'tinymacOS/README.md',
          'tinymacOS/docs/ARCHITECTURE.md', 'tinymacOS/docs/TINYFS.md',
          'tinymacOS/docs/VALIDATION.md', 'tinymacOS/docs/QUICKSTART.txt',
          'workspace/webkit-tmc/README.md']


def check(root=ROOT, manifest=None):
    manifest = manifest if manifest is not None else json.loads((root / 'workspace/documentation/releases.json').read_text())
    errors = []
    if len({r['tag'] for r in manifest}) != len(manifest):
        errors.append('Duplicate release tag in manifest')
    if len({r['release_id'] for r in manifest}) != len(manifest):
        errors.append('Duplicate release ID in manifest')
    for name in ACTIVE + [r['path'] for r in manifest]:
        p = root / name
        if not p.is_file():
            errors.append(f'Missing document: {name}')
            continue
        text = p.read_text()
        plain = re.sub(r'```.*?```', '', text, flags=re.S)
        release = name in [r['path'] for r in manifest]
        headings = re.findall(r'^# (.+)$', plain, re.M)
        if name.endswith('.md') and len(headings) != (0 if release else 1):
            errors.append(f'{name}: require one H1, or none in release bodies')
        if re.search(r'repository privat[ao]|download privato|release privata|corrente e pubblicata:.*2\.4', plain, re.I):
            errors.append(f'{name}: obsolete visibility/version claim')
        paragraphs = [p.strip() for p in re.split(r'\n\s*\n', plain) if len(p.strip()) >= 120]
        if any(n > 1 for n in collections.Counter(paragraphs).values()):
            errors.append(f'{name}: repeated paragraph')
        links = re.findall(r'!?\[[^\]]*\]\(([^)]+)\)', plain)
        if len(set(links)) != len(links):
            errors.append(f'{name}: repeated link target')
        for link in links:
            parsed = urlsplit(link)
            if parsed.scheme or link.startswith('#'):
                continue
            if not (p.parent / unquote(parsed.path)).resolve().exists():
                errors.append(f'{name}: missing relative link {link}')
        if release:
            if len(text) > 2000:
                errors.append(f'{name}: release body exceeds 2000 characters')
            if 'SHA256SUMS.txt' not in text:
                errors.append(f'{name}: checksum asset not named')
            if re.search(r'\b[0-9a-f]{64}\b', text):
                errors.append(f'{name}: checksum copied instead of using checksum asset')
            if 'Latest' in text:
                errors.append(f'{name}: transient Latest claim in version-specific notes')
    return errors


def remote_errors():
    env = os.environ.copy()
    fallback = '/var/tmp/tmc-github-cli/gh'
    gh = os.environ.get('GH_BIN') or shutil.which('gh') or fallback
    if 'GH_CONFIG_DIR' not in env and Path('/var/tmp/tmc-github-auth').is_dir():
        env['GH_CONFIG_DIR'] = '/var/tmp/tmc-github-auth'
    data = subprocess.check_output([gh, 'api', 'repos/ESPNex/tinymacOS/releases?per_page=100'], env=env)
    releases = {r['id']: r for r in json.loads(data)}
    errors = []
    for row in json.loads(MANIFEST.read_text()):
        r = releases.get(row['release_id'])
        if r is None or r['tag_name'] != row['tag']:
            errors.append(f"Release ID/tag mismatch: {row['tag']}")
        elif r['body'] != (ROOT / row['path']).read_text():
            errors.append(f"Remote notes differ: {row['tag']}")
        elif not all(a['name'] in r['body'] for a in r['assets']):
            errors.append(f"Asset name missing from notes: {row['tag']}")
    return errors


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--github', action='store_true')
    args = parser.parse_args()
    problems = check()
    if args.github:
        problems += remote_errors()
    for problem in problems:
        print('FAIL:', problem)
    if problems:
        raise SystemExit(1)
    print('PASS: 8 active guides, canonical release notes, local links and duplicate/stale-content checks' + ('; GitHub bodies match' if args.github else ''))
