#!/usr/bin/env python3
"""SH4DER-PVR: the chip bench, build script.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/bench/build.py [--tag h1] [--src bench_h1.c] [--out <dir>] [--cdi-dir <dir>] [--mkdcdisc <exe>]
                                   [--sh4zam <checkout>] [--elf-only]

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH, KOS_BASE set). It fetches nothing
and touches no file outside --out (default build/psm_bench) and --cdi-dir (default demo/psm/out/cdi, the one place
the images are kept; git ignores it). Steps, each refusing to go on when it fails:

  1 elf        kos-cc <src> -> <out>/psm-bench-<tag>.elf
               bench_h2.c is built as the library is (its Dreamcast flags, SH4ZAM from --sh4zam, default
               third_party/sh4zam, used where it lies: its headers, and its source/sh4/shz_mem_sh4.s assembled into
               the program), with src/sh4der_warp.c compiled twice, a vertex at a time and a strip at a time, under
               two sets of names, so one program times both
  2 image      mkdcdisc -> <cdi-dir>/psm-bench-<tag>.cdi. An image that is there is never overwritten: pick a new --tag

There is no host test: the program is timings of a console's own parts, and nothing in it can be checked on a host.
--elf-only stops after step 1 (no mkdcdisc needed).
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))

# The library's Dreamcast flags (the Makefile's), for a program that times the library's own code.
LIB_FLAGS = ["-O3", "-Wall", "-Wextra", "-ffast-math", "-ffp-contract=fast", "-mfsrra", "-mfsca", "-fno-math-errno"]
WARP_NAMES = ["sh4der_warp_draw_mesh", "sh4der_warp_draw_blur", "sh4der_warp_draw_pixel", "sh4der_warp_draw_dither"]


def run(step, argv):
    print("== %s: %s" % (step, " ".join(argv)), flush=True)
    r = subprocess.run(argv, cwd=ROOT)
    if r.returncode:
        raise SystemExit("%s failed (rc %d)" % (step, r.returncode))


def find_kos_cc():
    """kos-cc is a shell script: a Windows Python does not find or start it by name, so look for the file and run it with sh."""
    for d in os.environ.get("PATH", "").split(os.pathsep):
        p = os.path.join(d, "kos-cc")
        if os.path.isfile(p):
            return [shutil.which("sh") or "sh", p] if os.name == "nt" else [p]
    return None


def build_h2(kos_cc, a, out, elf):
    """bench_h2.c: the warp twice (pvr_prim a vertex; a strip a store-queue run), then the program with SH4ZAM."""
    if not os.path.isfile(os.path.join(a.sh4zam, "include", "sh4zam", "shz_sh4zam.h")):
        raise SystemExit("no SH4ZAM at %s (see AGENTS.md for the clone line, or give --sh4zam)" % a.sh4zam)
    inc = ["-I" + os.path.join(ROOT, "include"), "-I" + os.path.join(ROOT, "sh4dow-dc", "include"),
           "-I" + os.path.join(a.sh4zam, "include"), "-I" + os.path.join(ROOT, "third_party", "sh4zam-dce", "include")]
    objs = []
    for prim, way in ((1, "prim"), (0, "sq")):
        obj = os.path.join(out, "h2_warp_%s.o" % way)
        names = ["-D%s=h2_warp_%s_%s" % (n, n.rsplit("_", 1)[1], way) for n in WARP_NAMES]
        run("1 warp, %s" % way, kos_cc + LIB_FLAGS + inc + ["-DSH4DER_WARP_SEND_PRIM=%d" % prim] + names +
            ["-c", os.path.join(ROOT, "src", "sh4der_warp.c"), "-o", obj])
        objs.append(obj)
    run("1 elf", kos_cc + LIB_FLAGS + inc + [os.path.join(HERE, a.src)] + objs +
        [os.path.join(a.sh4zam, "source", "sh4", "shz_mem_sh4.s"), "-o", elf])


def main():
    ap = argparse.ArgumentParser(description="Build the chip bench image.")
    ap.add_argument("--tag", default="h1")
    ap.add_argument("--src", default="bench_h1.c")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "psm_bench"))
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "demo", "psm", "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--sh4zam", default=os.path.join(ROOT, "third_party", "sh4zam"))
    ap.add_argument("--elf-only", action="store_true")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    image = os.path.join(os.path.abspath(a.cdi_dir), "psm-bench-%s.cdi" % a.tag)
    elf = os.path.join(out, "psm-bench-%s.elf" % a.tag)
    if not a.elf_only and os.path.exists(image):
        raise SystemExit("refuse existing image: %s (pick a new --tag)" % image)

    kos_cc = find_kos_cc()
    if not os.environ.get("KOS_BASE") or not kos_cc:
        raise SystemExit("kos-cc is not on the PATH or KOS_BASE is not set: source the KallistiOS environment first")
    if os.path.basename(a.src) == "bench_h2.c":
        build_h2(kos_cc, a, out, elf)
    else:
        run("1 elf", kos_cc + ["-O2", "-Wall", "-Wextra", os.path.join(HERE, a.src), "-o", elf])
    if a.elf_only:
        print("ELF: %s" % elf)
        return 0

    os.makedirs(os.path.dirname(image), exist_ok=True)
    run("2 image", [a.mkdcdisc, "-e", elf, "-o", image, "-N", "-n", "PSM-BENCH"])
    print("IMAGE: %s" % image)
    return 0


if __name__ == "__main__":
    sys.exit(main())
