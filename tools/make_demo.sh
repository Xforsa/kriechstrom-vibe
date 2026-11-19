#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The demo song (tools/make_demo.c) -> kriechstrom/gen/kx_demo.h. Run after tools/build.sh (it needs build/gen).
set -e
cd "$(dirname "$0")/.."
cc -O2 -w -D_FORTIFY_SOURCE=0 -Ibuild/gen -Ikriechstrom/gen -Ikriechstrom -o build/make_demo tools/make_demo.c -lm
build/make_demo kriechstrom/gen/kx_demo.h
