#!/usr/bin/env python3
"""Generate the four Levante-era wallpapers: levante, meriggio, notturno, brezza."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

D = Path(__file__).resolve().parents[1] / 'assets'
W, H = 1280, 800


def vgrad(top, bottom):
    im = Image.new('RGB', (W, H))
    d = ImageDraw.Draw(im)
    for y in range(H):
        t = y / (H - 1)
        d.line([(0, y), (W, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)))
    return im


def blur(im, r):
    return im.filter(ImageFilter.GaussianBlur(r))


# --- Levante: vento caldo dal sud, dune e foschia ambra ---
im = vgrad((250, 214, 158), (232, 160, 96))
d = ImageDraw.Draw(im, 'RGBA')
d.ellipse((760, 90, 1060, 390), fill=(255, 236, 199, 150))       # sun halo
d.ellipse((838, 168, 982, 312), fill=(255, 244, 216, 235))       # hazy sun
for i, (yy, col) in enumerate([(520, (214, 138, 79, 255)), (600, (196, 118, 66, 255)),
                                (676, (172, 98, 56, 255)), (744, (146, 80, 46, 255))]):
    pts = [(0, H)] + [(x, yy + 46 * ((x / 260 + i) % 1) - 18 * ((x / 180 + i * 2) % 1))
                      for x in range(0, W + 80, 80)] + [(W, H)]
    d.polygon(pts, fill=col)
for k in range(5):                                                  # wind streaks
    y = 120 + k * 58
    d.line([(60 + k * 40, y), (520 + k * 70, y - 14)], fill=(255, 238, 205, 60), width=6)
im.save(D / 'levante.qoi')

# --- Meriggio: luce di mezzogiorno sul mare ---
im = vgrad((108, 178, 236), (188, 226, 246))
d = ImageDraw.Draw(im, 'RGBA')
d.ellipse((548, 60, 732, 244), fill=(255, 255, 246, 255))
d.ellipse((520, 32, 760, 272), fill=(255, 255, 246, 90))
sea = vgrad((44, 122, 190), (24, 84, 150)).crop((0, 430, W, H))
im.paste(sea, (0, 430))
d = ImageDraw.Draw(im, 'RGBA')
d.rectangle((0, 424, W, 436), fill=(236, 250, 255, 230))
for k in range(9):                                                  # glitter bands
    y = 470 + k * 36
    off = (k % 3) * 90
    for x in range(60 + off, W - 60, 240):
        d.line([(x, y), (x + 90, y)], fill=(255, 255, 255, 46 + (k % 2) * 30), width=4)
im.save(D / 'meriggio.qoi')

# --- Notturno: mare notturno, luna e riflessa ---
im = vgrad((16, 24, 52), (46, 62, 98))
d = ImageDraw.Draw(im, 'RGBA')
import random
random.seed(7)
for _ in range(160):                                                # stars
    x, y = random.randrange(W), random.randrange(430)
    a = random.randrange(70, 220)
    r = random.choice((1, 1, 2))
    d.ellipse((x, y, x + r, y + r), fill=(255, 255, 255, a))
d.ellipse((872, 96, 1016, 240), fill=(240, 244, 255, 255))         # moon
d.ellipse((852, 76, 1036, 260), fill=(240, 244, 255, 60))
night = vgrad((10, 16, 38), (20, 32, 64)).crop((0, 448, W, H))
im.paste(night, (0, 448))
d = ImageDraw.Draw(im, 'RGBA')
d.rectangle((0, 442, W, 454), fill=(140, 160, 210, 160))
for k in range(14):                                                 # moon path
    y = 470 + k * 24
    wdt = 26 + k * 7
    cx = 944 + (k % 2 * 10 - 5)
    d.line([(cx - wdt, y), (cx + wdt, y)], fill=(226, 234, 255, 150 - k * 7), width=5)
im.save(D / 'notturno.qoi')

# --- Brezza: aria di mare, orizzonte pallido ---
im = vgrad((216, 240, 236), (246, 250, 244))
d = ImageDraw.Draw(im, 'RGBA')
for i, (yy, hh, a) in enumerate([(150, 44, 110), (236, 34, 90), (322, 26, 70)]):
    d.rounded_rectangle((90 + i * 130, yy, 760 + i * 150, yy + hh), radius=hh // 2,
                        fill=(255, 255, 255, a))
calm = vgrad((128, 196, 196), (86, 158, 166)).crop((0, 456, W, H))
im.paste(calm, (0, 456))
d = ImageDraw.Draw(im, 'RGBA')
d.rectangle((0, 450, W, 462), fill=(250, 253, 250, 220))
for k in range(7):
    y = 500 + k * 40
    x0 = 80 + (k % 3) * 140
    d.line([(x0, y), (x0 + 380, y)], fill=(255, 255, 255, 70), width=5)
im.save(D / 'brezza.qoi')
print('wallpapers:', [p.name for p in sorted(D.glob('*.qoi'))])
