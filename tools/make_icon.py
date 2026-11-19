#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# kriechstrom/icon.png (960 x 540, its tile on the VIBE and its card on the hub): the creep, grown as the firmware grows
# it (kriechstrom/kx_creep.h: the same seeded discharge), white hot at the trunk, on black, with the display's scanlines.
#   python3 tools/make_icon.py   (Pillow)
import math, os
from PIL import Image, ImageDraw, ImageFilter

M = 0xFFFFFFFF
rng = 0x4B525354
def rand():
    global rng
    rng ^= (rng << 13) & M; rng ^= rng >> 17; rng ^= (rng << 5) & M
    return rng
def randf(): return (rand() & 0xFFFF) / 65535.0
def vsin(t): return math.sin(t * 2 * math.pi)

seg, queue = [], []
def fork(x, y, head, ln, depth, order):
    if len(queue) < 1024 and ln > 6.0:
        queue.append((x, y, head, ln, depth, min(order, 255)))
def branch(x, y, head, ln, depth, order):
    gone, i = 0.0, 0
    while gone < ln and len(seg) < 3000:
        step = (7.0 if depth < 2 else 4.0) + randf() * (9.0 if depth < 2 else 6.0)
        a = head + (randf() - 0.5) * (0.16 if depth < 1 else 0.24)
        head += (randf() - 0.5) * 0.03
        nx, ny = x + vsin(a + 0.25) * step, y - vsin(a) * step
        if nx < 2 or nx > 1022 or ny < 2 or ny > 1022:
            return
        seg.append((int(x), int(y), int(nx), int(ny), depth, min(order + i, 255)))
        x, y = nx, ny
        gone += step
        i += 1
        if depth < 7 and randf() < (0.30 if depth < 2 else 0.22):
            side = -1.0 if randf() < 0.5 else 1.0
            fork(x, y, head + side * (0.05 + randf() * 0.12), (ln - gone) * (0.35 + randf() * 0.35), depth + 1, order + i)

fork(512, 1020, 0.25, 900, 0, 0)
fork(500, 1020, 0.25 + 0.09, 420, 1, 10)
fork(524, 1020, 0.25 - 0.09, 420, 1, 10)
while queue and len(seg) < 3000:
    branch(*queue.pop(0))

W, H, S = 960, 540, 3                              # drawn 3x, scaled down: smooth lines
img = Image.new('RGB', (W * S, H * S), (9, 9, 10))
glow = Image.new('RGB', (W * S, H * S), (0, 0, 0))
dg, di = ImageDraw.Draw(glow), ImageDraw.Draw(img)
box = (W * S * 0.5 - H * S * 0.56, H * S * 0.02, H * S * 1.12)   # x0, y0, size: the figure, centred, a little cut at the top
def P(x, y): return (box[0] + x / 1024 * box[2], box[1] + y / 1024 * box[2])
for x0, y0, x1, y1, d, o in seg:
    if d < 2:
        dg.line([P(x0, y0), P(x1, y1)], fill=(200, 30, 20) if d == 0 else (120, 20, 12), width=int((6 - d * 3) * S * 2))
glow = glow.filter(ImageFilter.GaussianBlur(14 * S))
img = Image.blend(img, glow, 0.55)
di = ImageDraw.Draw(img)
for x0, y0, x1, y1, d, o in seg:
    w = max(1, int((3.4 if d == 0 else 2.0 if d == 1 else 1.3 if d == 2 else 0.8) * S))
    c = (255, 244, 234) if d == 0 else (245, 90, 60) if d == 1 else (224, 32, 24) if d < 4 else (150, 32, 20)
    di.line([P(x0, y0), P(x1, y1)], fill=c, width=w)
img = img.resize((W, H), Image.LANCZOS)
px = img.load()
for y in range(0, H, 3):                           # the display's scanlines
    for x in range(W):
        r, g, b = px[x, y]
        px[x, y] = (r * 7 // 8, g * 7 // 8, b * 7 // 8)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'kriechstrom', 'icon.png')
img.save(out, optimize=True)
print(out, len(seg), 'segments', os.path.getsize(out) // 1024, 'KB')
