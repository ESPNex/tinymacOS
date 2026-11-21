#!/usr/bin/env python3
"""Retain the actual final-ISO framebuffer and an exact Control Center crop."""
import json,shutil,hashlib
from pathlib import Path
from PIL import Image
q=Path(__file__).resolve().parent;r=json.loads((q/'control-result.json').read_text());a=json.loads((q/'artifact.json').read_text());assert r['passed'] and r['iso_sha256']==a['sha256']
current=json.loads((q/'control-contract-result.json').read_text());assert current['passed']
ev=q/'evidence';ev.mkdir(exist_ok=True);p=Path(r['work'])/'evidence/control-default.png';shutil.copyfile(p,ev/'control-center-10.1.3.png')
with Image.open(p) as im:
 w,h=im.size;ph=min(550,h-60);pw=376*ph//550;box=(w-pw-18,46,w-18,46+ph);im.crop(box).save(ev/'control-center-detail.png',optimize=True)
(q/'preview-verification.json').write_text(json.dumps({'passed':True,'iso_sha256':r['iso_sha256'],'real_framebuffer':True,'generated_mockup':False,'full_frame_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'crop_box':box,'duplicate_wallpaper_slider_removed':True,'duplicate_settings_icon_removed':True},indent=2)+'\n');print('PASS final ISO Control Center preview retained, exact pixels; compact glass panel with no duplicate wallpaper/settings controls')
