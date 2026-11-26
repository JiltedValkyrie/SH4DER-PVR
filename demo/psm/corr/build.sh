#!/bin/bash
# corr-a: bake the textures and build the Dreamcast ELF into a NEW folder (refuses one that exists).
#   bash demo/psm/corr/build.sh <tag> <out-folder>
# Needs: the art fetched (python demo/pl4nar/tools/fetch.py --root demo/psm/corr), Pillow, a KallistiOS environment
# (KOS_ENV= its environ.sh), its SH4ZAM (SH4ZAM= the tree with include/). The host test: see test/corr_test.c.
# Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.
set -e
TAG=${1:?tag}
B=${2:?out folder}
R=$(cd "$(dirname "$0")/../../.." && pwd)
PY=("${PYTHON:-python}")
if [ -e "$B" ]; then echo "build folder taken: $B"; exit 1; fi
mkdir -p "$B"
"${PY[@]}" "$R/tools/check_credits.py" --root "$R/demo/psm/corr"
"${PY[@]}" "$R/demo/psm/corr/tools/corr_bake.py" --out "$B/gen"
. "${KOS_ENV:?KOS_ENV= the KallistiOS environ.sh}"
Z=${SH4ZAM:?SH4ZAM= the SH4ZAM tree}
LIB=""; [ -f "$Z/build/libsh4zam.a" ] && LIB="$Z/build/libsh4zam.a"
kos-cc -O3 -Wall -Wextra -ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno -I"$R/include" \
  -I"$Z/include" -I"$R/third_party/sh4zam-dce/include" -I"$B/gen" -DCORR_TAG=$TAG \
  "$R/demo/psm/corr/corr_spike.c" $LIB -o "$B/sh4der-corr-$TAG.elf" > "$B/elf.log" 2>&1 || { cat "$B/elf.log"; exit 1; }
echo "warnings: $(grep -c 'warning:' "$B/elf.log" || true)"
sha256sum "$B/sh4der-corr-$TAG.elf"
