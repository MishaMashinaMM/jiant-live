#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# The front page's animations: tools/landing_anim/capture.c on a patched copy of the firmware's sources (the drawing
# primitives note what they draw), then pack.py -> web/landing/anim/*.js. Run after one ./build.sh (build/gen).
#   tools/landing_anim/build.sh [GEN_DIR]
set -e
cd "$(dirname "$0")/../.."
GEN=${1:-build/gen}
T=$(mktemp -d)
mkdir -p "$T/web/emu" "$T/tests" "$T/firmware"
cp -r firmware/src "$T/firmware/src"; for d in firmware/*/; do [ "$d" = firmware/src/ ] || cp -r "$d" "$T/$d"; done
cp web/emu/felucca_web.c "$T/web/emu/"
cp tests/hostsim.c "$T/tests/"
python3 tools/landing_anim/patch.py "$T/firmware/src"
cc -O1 -w -std=gnu99 "-DFELUCCA_VERSION=\"LANDING\"" -I"$GEN" -I"$T/firmware/src" -I"$T" -o "$T/capture" tools/landing_anim/capture.c -lm
"$T/capture" "$T/capture.txt"
python3 tools/landing_anim/pack.py "$T/capture.txt" web/landing/anim $LA_PACK_ARGS
[ -n "$LA_KEEP" ] && cp "$T/capture.txt" "$LA_KEEP"
rm -rf "$T"
