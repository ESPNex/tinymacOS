"""Black-box observations of the boot mark and radial auth activity in actual GOP frames.
No guest counters, injected images, sleeps to extend auth, or overlays in the captures.
"""
import hashlib, math, time
from xml.etree import ElementTree
from PIL import Image, ImageChops
from guest import ROOT


def boot_mark(samples, check, metrics):
    src = ElementTree.parse(ROOT/'assets/apple.svg').getroot()
    path = src.find('{http://www.w3.org/2000/svg}path')
    assert src.get('viewBox') == '0 0 814 1000' and path.get('fill') == '#fff'
    mask = Image.open(ROOT/'assets/fruit-mask.png').convert('RGB')
    # At intermediate loading milestones the initial paint is finished and the
    # compositor has not yet begun the final hand-off to login.
    stable=[q for q in samples if 0<q[0]<192]
    assert len(stable)>=3
    for _, _, im in stable:
        # The 94×108 silhouette is vertically centered on the 1280×800 GOP (y=346),
        # matching the layout asserted by the retail-image runtime test.
        x, y = im.width//2 - 47, im.height//2 - 54
        assert ImageChops.difference(mask, im.crop((x,y,x+94,y+108)).convert('RGB')).getbbox() is None
    metrics['pixel_exact_boot_marks']=len(stable)
    metrics['apple_path_sha256'] = hashlib.sha256(path.get('d').encode('ascii')).hexdigest()
    check(True, 'At all sampled intermediate boot milestones the logo pixels match the antialiased raster of the supplied white SVG, with the boot bar separately underneath')


def read_phase(im, x, y):
    # At the middle of each capsule: independent polar photometry, clockwise from north.
    # Look for one pure-white spoke, a clear center, and twelve positive-contrast
    # strokes separated by gaps. A text field, horizontal bar or solid disc won't match.
    def ray(angle):
        return max(min(im.getpixel((math.floor(x+math.sin(angle)*r),
                                    math.floor(y-math.cos(angle)*r)))) for r in (9,10,11,12))
    ink = [ray(math.pi*k/6) for k in range(12)]
    gap = [ray(math.pi*(k+.5)/6) for k in range(12)]
    if ink.count(255) != 1 or min(i-j for i,j in zip(ink,gap)) < 4 or min(im.getpixel((x,y))) > 249:
        return None
    return ink.index(255)


def submit(g, E, tag, end, metrics, *, setup=False, reduced=False, save=False):
    x, y = 640, 449 if setup else 630  # 1280x800 reference GOP, enrollment vs returning login
    assert read_phase(g.frame(),x,y) is None, 'activity cannot be visible in idle field'
    m = g.mark(); g.key('ret',0); start = time.monotonic()
    seq=[]; crops=[]; seen=0; first=None; first_stamp=last_stamp=None
    while time.monotonic()-start < 30 and end not in g.text()[m:]:
        im=g.frame();phase=read_phase(im,x,y);now=time.monotonic()
        if phase is not None:
            seen+=1
            if first_stamp is None:first_stamp=now
            last_stamp=now
            if not seq or seq[-1]!=phase:
                seq.append(phase)
                if save:
                    if first is None:first=im.copy()
                    crops.append((now,im.crop((x-155,y-165,x+155,y+60))))
        time.sleep(.012)
    g.wait(end,mark=m,timeout=5)
    metrics.setdefault('login_activity',{})[tag]={'GOP_samples':seen,'peak_spokes_clockwise_from_top':seq,
        'observed_span_ms':round(1000*(last_stamp-first_stamp)) if seen else 0}
    if reduced:
        assert seq==[0] and seen>=3 and last_stamp-first_stamp>.12,(tag,seq,seen)
    else:
        deltas=[(b-a)%12 for a,b in zip(seq,seq[1:])]
        assert len(set(seq))>=3 and sum(d in (1,2,3) for d in deltas)>=2,(tag,seq)
    if save:
        first.save(E/'00i-spinner-login.png')
        # Lossless moving crop, original frame pixels and measured intervals; one-shot test evidence.
        imgs=[im for _,im in crops]; durations=[max(12,round(1000*(b[0]-a[0]))) for a,b in zip(crops,crops[1:])]+[60]
        imgs[0].save(E/'00i-spinner-login.webp',save_all=True,append_images=imgs[1:],duration=durations,lossless=True,loop=0)
    return m
