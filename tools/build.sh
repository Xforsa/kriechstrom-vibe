#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Builds KRIECHSTROM for VIBE FM:
#   sh tools/build.sh            ->  build/kriechstrom.wasm + build/kriechstrom.zip
# 1. the generated tables and fonts of the KRIECHSTROM core (core/kriechstrom/tools, Python 3 with Pillow and
#    fontTools), into build/gen; 2. the VIBE FM SDK's build (tools/vibe-build.sh, Zig 0.13.0).
set -e
cd "$(dirname "$0")/.."
DIR="${1:-kriechstrom}"
K=core/kriechstrom
[ -f "$K/firmware/src/core.h" ] || { echo "core/kriechstrom is empty: git submodule update --init"; exit 1; }
PY="${PYTHON:-python3}"
G=build/gen
mkdir -p "$G"
stamp="$G/.core-$(git -C "$K" rev-parse HEAD 2>/dev/null || echo none)"
if [ ! -f "$stamp" ]; then
    rm -f "$G"/*.h "$G"/.core-*
    "$PY" "$K/tools/gen_tables.py" "$G/felucca_tables.h" >/dev/null
    "$PY" "$K/tools/gen_fm6_patches.py" "$G/felucca_fm6.h" >/dev/null
    "$PY" "$K/tools/gen_adpcm.py" "$G/felucca_adpcm.h" >/dev/null
    "$PY" "$K/tools/gen_aa_font.py" "$G/ui_fonts.h" --preset inter-tight >/dev/null
    "$PY" "$K/tools/gen_aa_icons.py" "$G/ui_icons.h" >/dev/null
    "$PY" "$K/tools/gen_aa_keycaps.py" "$G/ui_keycaps.h" >/dev/null
    "$PY" "$K/tools/gen_ui_palettes.py" "$G/ui_palettes.h" >/dev/null
    "$PY" "$K/tools/gen_words.py" "$K/firmware/words.tsv" build/words "$G/ann_words.h" >/dev/null
    touch "$stamp"
fi
if [ -z "$ZIG" ] && ! command -v zig >/dev/null 2>&1 && [ -x "$HOME/.local/zig/zig-macos-aarch64-0.13.0/zig" ]; then
    ZIG="$HOME/.local/zig/zig-macos-aarch64-0.13.0/zig"; export ZIG
fi
exec sh tools/vibe-build.sh "$DIR"
