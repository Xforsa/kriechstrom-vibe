#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# docs/card.png, the Firmware Hub's card (512 x 512): the figure from the FM-1's own art (krst_art.py), the name across
# its middle (it reads when the player's list crops it wide), VIBE FM under it. Inter Tight from the core's fonts.
#   python3 tools/make_card.py   (Pillow)
import sys, random, math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
A = type(sys)("krst_art"); A.__file__ = str(ROOT/"core/kriechstrom/tools/krst_art.py")
_src = (ROOT/"core/kriechstrom/tools/krst_art.py").read_text()
exec(compile(_src.rsplit("\nmain()", 1)[0], "krst_art.py", "exec"), A.__dict__)
FONT = str(ROOT/"core/kriechstrom/assets/fonts/InterTight[wght].ttf")
def font(sz, w=800):
    f = ImageFont.truetype(FONT, sz)
    try: f.set_variation_by_axes([w])
    except Exception: pass
    return f
OUT = ROOT / 'docs'; S = 512
def figure(seed, cx, cy, ln, wd, dep, size=S):
    rnd = random.Random(seed)
    m = A.scar(size*2, size*2, [(cx*2, cy*2, 0, ln, wd, dep)], rnd).resize((size, size), Image.LANCZOS)
    return A.burn(Image.new("RGB", (size, size), (0,0,0)), m, 5)
def word(d, y, sz, cx=S//2):
    f = font(sz); a = "KRIECH"; b = "STROM"
    wa = d.textlength(a, font=f); wb = d.textlength(b, font=f); x = cx - (wa+wb)/2
    d.text((x, y), a, font=f, fill=(245,245,245)); d.text((x+wa, y), b, font=f, fill=(230,36,20))
im = figure(2617, 256, 256, 90, 10, 7); d = ImageDraw.Draw(im)
d.rectangle((0, 214, S, 300), fill=(0,0,0))
word(d, 218, 70)
f = font(26, 600); t = "VIBE FM"; tw = d.textlength(t, font=f)
d.rectangle((S/2-tw/2-12, 312, S/2+tw/2+12, 348), fill=(230,36,20)); d.text((S/2-tw/2, 314), t, font=f, fill=(0,0,0))
im.save(OUT/"card.png", optimize=True)
