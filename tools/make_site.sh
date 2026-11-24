#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The site (GitHub Pages): site/ with the screens made anew into docs/ (tools/make_shots.py) and the version from kriechstrom/firmware.json -> build/site
set -e
cd "$(dirname "$0")/.."
V=$(python3 -c "import json; print(json.load(open('kriechstrom/firmware.json'))['version'])")
rm -rf build/site && mkdir -p build/site/shots build/site/media
cp site/style.css site/vibe.js build/site/
cp site/media/* build/site/media/
python3 tools/make_shots.py >/dev/null           # (the screens from the firmware as it is now: never behind it)
for f in docs/*.png; do [ "$f" = docs/card.png ] || cp "$f" build/site/shots/; done
mkdir -p build/site/manual
for p in index.html ru.html manual/en.html manual/ru.html; do sed "s/@VERSION@/$V/g" "site/$p" > "build/site/$p"; done
echo "build/site ($V)"
