#!/usr/bin/env python3
"""Rasterize the supplied SVG once on the host; no SVG renderer in firmware/the guest.
Regeneration only: Pillow + rsvg-convert (librsvg2-bin). The normal build uses checked-in masks.
"""
from pathlib import Path
import io, subprocess
from PIL import Image

D = Path(__file__).resolve().parents[1] / 'assets'
svg = D / 'apple.svg'
# Render the SVG with its own viewBox/viewport and transparent background. Rasterize
# all sizes from this high-resolution alpha, never from the tiny menu glyph.
buf = subprocess.check_output(['rsvg-convert', '--zoom=8', str(svg)])
alpha = Image.open(io.BytesIO(buf)).convert('RGBA').getchannel('A')

def mask(size, height):
    w, h = size
    # Work on a 16x canvas so centering and aspect-ratio rounding are subpixel.
    s = 16
    a = alpha.resize((round(height*s*alpha.width/alpha.height),height*s),Image.Resampling.LANCZOS)
    page = Image.new('L',(w*s,h*s));page.paste(a,((w*s-a.width)//2,(h*s-a.height)//2))
    return page.resize(size,Image.Resampling.LANCZOS)

def include(path,im):
    px=im.tobytes(); w=im.width
    path.write_text(',\n'.join(','.join(map(str,px[j:j+w])) for j in range(0,len(px),w))+'\n')

boot = mask((94,108),108); boot.save(D/'fruit-mask.png');include(D/'fruit.inc',boot)
icon = Image.new('RGBA',(64,64),'white');icon.putalpha(mask((64,64),52));icon.save(D/'apple-icon.png')
include(D/'apple-menu.inc',mask((24,24),16))
