#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The site's hero (site/media/hero.jpg) and its flash (hero-flash.jpg): discharges grown as the firmware grows them
# (kriechstrom/kx_creep.h), drawn with a real glow: a white-hot core, red forks, filaments that fade to rust.
#   python3 tools/make_hero.py   (Pillow)
import math, os
from PIL import Image, ImageDraw, ImageFilter, ImageChops

def vsin(t): return math.sin(t * 2 * math.pi)

def grow(seed, rx, ry, head, ln, mx):
    rng = [seed & 0xFFFFFFFF]
    def randf():
        r = rng[0]; r ^= (r << 13) & 0xFFFFFFFF; r ^= r >> 17; r ^= (r << 5) & 0xFFFFFFFF; rng[0] = r
        return (r & 0xFFFF) / 65535.0
    seg, q = [], [(rx, ry, head, ln, 0, 0)]
    while q and len(seg) < mx:
        x, y, hd, l, depth, order = q.pop(0)
        gone, i = 0.0, 0
        while gone < l and len(seg) < mx:
            step = (7.0 if depth < 2 else 4.0) + randf() * (9.0 if depth < 2 else 6.0)
            a = hd + (randf() - 0.5) * (0.16 if depth < 1 else 0.24)
            hd += (randf() - 0.5) * 0.03
            nx, ny = x + vsin(a + 0.25) * step, y - vsin(a) * step
            if nx < -300 or nx > 1324 or ny < -300 or ny > 900: break
            seg.append((x, y, nx, ny, depth))
            x, y = nx, ny; gone += step; i += 1
            if depth < 7 and randf() < (0.30 if depth < 2 else 0.22) and len(q) < 1024:
                q.append((x, y, hd + (-1 if randf() < 0.5 else 1) * (0.05 + randf() * 0.12),
                          (l - gone) * (0.35 + randf() * 0.35), depth + 1, order + i))
    return seg

W, H, S = 1920, 900, 2                            # the field is 1024 x 480 units: 1 unit = W / 1024 px
FH = 480
figs = [(grow(0x4B525354, 640, FH + 6, 0.25, 520, 2600), 1.0),
        (grow(0x5354524F, -8, -8, 0.90, 420, 1500), 0.8),
        (grow(0x4D4B5249, 1032, 140, 0.53, 360, 1000), 0.7)]
k = W * S / 1024.0
def P(x, y): return (x * k, y * k)

def layer(widths, colours, alpha_of):
    im = Image.new('RGB', (W * S, H * S), (0, 0, 0))
    d = ImageDraw.Draw(im)
    for seg, w in figs:
        for x0, y0, x1, y1, dep in seg:
            c = colours(dep)
            a = alpha_of(dep) * w
            c = tuple(int(v * a) for v in c)
            d.line([P(x0, y0), P(x1, y1)], fill=c, width=max(1, int(widths(dep) * S)))
    return im

# the glow: the strong discharges, wide and blurred twice (near and far)
glow_src = layer(lambda d: 10 - 3 * min(d, 2), lambda d: (255, 40, 24), lambda d: 1.0 if d == 0 else 0.6 if d == 1 else 0.25 if d == 2 else 0.0)
far = glow_src.filter(ImageFilter.GaussianBlur(28 * S))
near = glow_src.filter(ImageFilter.GaussianBlur(7 * S))
core = layer(lambda d: 3.2 if d == 0 else 2.0 if d == 1 else 1.3 if d == 2 else 0.8 if d < 5 else 0.6,
             lambda d: (255, 246, 238) if d == 0 else (255, 120, 90) if d == 1 else (232, 40, 28) if d < 4 else (150, 34, 20),
             lambda d: 1.0 if d < 4 else 0.85 if d < 6 else 0.6)
def add(*ims):
    out = ims[0]
    for im in ims[1:]: out = ImageChops.add(out, im)
    return out
base = add(ImageChops.multiply(far, Image.new('RGB', far.size, (150, 150, 150))), ImageChops.multiply(near, Image.new('RGB', near.size, (120, 120, 120))), core)
flash = add(far, far, near, core.filter(ImageFilter.GaussianBlur(1.5 * S)), core)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'site', 'media')
base.resize((W, H), Image.LANCZOS).save(os.path.join(out, 'hero.jpg'), quality=86, optimize=True, progressive=True)
flash.resize((W, H), Image.LANCZOS).save(os.path.join(out, 'hero-flash.jpg'), quality=82, optimize=True, progressive=True)
print('hero', sum(len(s) for s, _ in figs), 'segments')
