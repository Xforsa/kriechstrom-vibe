#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The site's pictures, drawn with the FM-1 site's own tools (core/kriechstrom/tools/krst_art.py: the Lichtenberg figure
# burnt into a worn ground):
#   site/media/hero.jpg        1920 x 900   the figure from the right, a second one rising from the lower left
#   site/media/hero-flash.jpg  1920 x 900   the same figure white-hot (the strike: style.css blends it in now and then)
#   site/media/band.jpg        1600 x 360   the strip at the foot of the page
#   site/media/figure.jpg      900 x 900    the figure alone, from its centre
#   python3 tools/make_hero.py   (Pillow)
import math, random, sys
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
A = type(sys)("krst_art")
A.__file__ = str(ROOT / "core" / "kriechstrom" / "tools" / "krst_art.py")
# (its functions only: the script itself would redraw the FM-1's art)
_src = (ROOT / "core" / "kriechstrom" / "tools" / "krst_art.py").read_text()
exec(compile(_src.rsplit("\nmain()", 1)[0], "krst_art.py", "exec"), A.__dict__)

SEED = 2611
out = ROOT / "site" / "media"
w, h = 1920, 900
rnd = random.Random(SEED)
m = A.scar(w, h, [(1420, 420, 0, 128, 9, 7), (260, 940, -math.pi / 2 + 0.25, 150, 7, 2)], rnd)
ground = A.ground(w, h, SEED)
A.scanlines(A.burn(ground, m, 4)).save(out / "hero.jpg", quality=84, optimize=True)
hot = Image.eval(m, lambda v: min(255, int(v * 1.9)))
flash = A.burn(Image.new("RGB", (w, h)), hot, 7)
A.scanlines(flash).save(out / "hero-flash.jpg", quality=80, optimize=True)
A.band(SEED + 1).save(out / "band.jpg", quality=84, optimize=True)
A.tracking(SEED + 5).resize((900, 900), Image.LANCZOS).save(out / "figure.jpg", quality=82, optimize=True)
print("site/media: hero.jpg hero-flash.jpg band.jpg figure.jpg")
