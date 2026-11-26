#!/bin/sh
# Host preview: the same demo code over a software rasteriser, scripted frames to build/preview/*.png (off | on pairs).
# Needs the baked assets (build.py as far as its bake step, or tools/bake.py after fetch, coupe and lod), a host C
# compiler, python with Pillow. Run from demo/pl4nar:  SH4ZAM=<checkout> sh tools/preview.sh
set -e
test -n "$SH4ZAM" || { echo "set SH4ZAM to your SH4ZAM checkout"; exit 1; }
test -f build/gen/assets_gen.c || { echo "no baked assets: run build.py (or tools/bake.py) first"; exit 1; }
mkdir -p build/host build/preview
${HOST_CC:-cc} -O2 -std=gnu11 -Wall -Wextra -Isrc -Ibuild/gen -I../../include \
  -I"$SH4ZAM/include" src/demo.c src/gfx.c src/hud.c host/gfx_host.c host/main_host.c build/gen/assets_gen.c \
  "$SH4ZAM/source/sw/shz_xmtrx_sw.c" -lm -o build/host/preview 2> build/host/cc.log
./build/host/preview build/preview
python tools/preview_sheets.py build/preview
