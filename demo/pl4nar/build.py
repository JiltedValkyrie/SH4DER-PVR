#!/usr/bin/env python3
"""SH4DER-PVR street driving demo: from a clean checkout to a disc image, one script.

    python demo/pl4nar/build.py --sh4zam <checkout> [--rtt] [--blender <exe>] [--mkdcdisc <exe>] [--out <dir>] [--pin]

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH). Steps, each refusing to go on
when it fails:

  1 credits   tools/check_credits.py: the fetch table and CREDITS.md agree
  2 fetch     tools/fetch.py: the CC0 packs into assets/ (sha256 checked; --pin accepts an unpinned pack once)
  3 coupe     tools/coupe.py: the hero car from numbers -> build/gen/coupe.obj
  4 lod       tools/car_lod.py in Blender, headless: traffic cars and props cut to the triangle budget -> build/lod/
  5 bake      tools/bake.py: models and textures -> build/gen/assets_gen.c, assets_gen.h
  6 make      the Dreamcast program -> build/pl4nar-demo.elf
  7 image     mkdcdisc -> <out>/pl4nar-demo-b<N>.cdi, N one more than the highest there (an image is never overwritten)

External tools, not part of this repository: KallistiOS (its own licence), SH4ZAM (MIT), Blender (GPL; used as a
tool, nothing of it is shipped), mkdcdisc, Python 3 with Pillow. --rtt: your KallistiOS has pvr_scene_begin_rtt.
Standard library only. Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
GEN = os.path.join(ROOT, "build", "gen")
LOD = os.path.join(ROOT, "build", "lod")
# source OBJ (under assets/), body budget, wheel budget: the triangle budget of a late 1990s console street
LODS = [("kenney_car-kit/taxi.obj", 186, 16), ("kenney_car-kit/police.obj", 186, 16), ("kenney_car-kit/van.obj", 186, 16),
        ("kenney_city-kit-roads/light-square.obj", 36, 36), ("kenney_city-kit-roads/electricity-pole-single.obj", 24, 24),
        ("kenney_city-kit-roads/traffic-light.obj", 60, 60), ("kenney_city-kit-roads/construction-cone.obj", 16, 16),
        ("kenney_city-kit-roads/dumpster.obj", 36, 36)]


def run(step, argv, cwd=ROOT):
    print("== %s" % step, flush=True)
    r = subprocess.run(argv, cwd=cwd)
    if r.returncode != 0:
        raise SystemExit("%s failed (exit %d)" % (step, r.returncode))


def fresh(path):
    """A generated file is rebuilt under a name that is free: move nothing, delete nothing."""
    return not os.path.exists(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--sh4zam", default=os.environ.get("SH4ZAM", ""))
    ap.add_argument("--blender", default=os.environ.get("BLENDER", "blender"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--out", default=os.path.join(ROOT, "out"))
    ap.add_argument("--rtt", action="store_true", help="the KallistiOS in use has pvr_scene_begin_rtt")
    ap.add_argument("--pin", action="store_true", help="accept packs that have no sha256 in the table yet")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    a = ap.parse_args()
    if not a.sh4zam or not os.path.isdir(os.path.join(a.sh4zam, "include", "sh4zam")):
        raise SystemExit("--sh4zam: a SH4ZAM checkout (it has include/sh4zam)")
    if not shutil.which("kos-cc"):
        raise SystemExit("kos-cc is not on the PATH: source the KallistiOS environment first")
    py = sys.executable
    os.makedirs(GEN, exist_ok=True)
    os.makedirs(LOD, exist_ok=True)
    run("credits", [py, os.path.join("tools", "check_credits.py")])
    run("fetch", [py, os.path.join("tools", "fetch.py")] + (["--pin"] if a.pin else []))
    if fresh(os.path.join(GEN, "coupe.obj")):
        run("coupe", [py, os.path.join("tools", "coupe.py"), os.path.join(GEN, "coupe.obj")])
    todo = [(s, b, w) for s, b, w in LODS if fresh(os.path.join(LOD, os.path.basename(s)))]
    if todo and not shutil.which(a.blender):
        raise SystemExit("Blender not found (--blender <exe> or BLENDER): the lod step cuts %d models with it" % len(todo))
    for src, body, wheel in todo:
        run("lod %s" % os.path.basename(src),
            [a.blender, "-b", "--factory-startup", "--python", os.path.join("tools", "car_lod.py"), "--",
             os.path.join("assets", *src.split("/")), os.path.join(LOD, os.path.basename(src)), "--body", str(body), "--wheel", str(wheel)])
    run("bake", [py, os.path.join("tools", "bake.py")])
    os.makedirs(a.out, exist_ok=True)
    n = 1 + max([int(m.group(1)) for m in (re.match(r"pl4nar-demo-b(\d+)\.cdi$", f) for f in os.listdir(a.out)) if m] or [0])
    image = os.path.join(a.out, "pl4nar-demo-b%d.cdi" % n)
    run("make", ["make", "-j%d" % a.jobs, "SH4ZAM=%s" % a.sh4zam.replace(os.sep, "/"), "DEMO_BUILD=%d" % n] + (["RTT=1"] if a.rtt else []))
    if os.path.exists(image):
        raise SystemExit("refuse existing image: %s" % image)
    run("image", [a.mkdcdisc, "-e", os.path.join(ROOT, "build", "pl4nar-demo.elf"), "-o", image, "-N", "-n", "PL4NAR-DEMO"])
    print("image: %s" % image)
    return 0


if __name__ == "__main__":
    sys.exit(main())
