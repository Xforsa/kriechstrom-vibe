#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The site (GitHub Pages): site/ with the screens from docs/ and the version from kriechstrom/firmware.json -> build/site
set -e
cd "$(dirname "$0")/.."
V=$(python3 -c "import json; print(json.load(open('kriechstrom/firmware.json'))['version'])")
rm -rf build/site && mkdir -p build/site/shots build/site/media
cp site/style.css site/vibe.js build/site/
cp site/media/* build/site/media/
for s in sound seq seq-drum master stage pedals sampler mixer env fm6 song start home delay reverb foot clock damage projects outputs; do cp "docs/$s.png" build/site/shots/; done
for p in index.html ru.html; do sed "s/@VERSION@/$V/g" "site/$p" > "build/site/$p"; done
echo "build/site ($V)"
