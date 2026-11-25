#!/usr/bin/env python3
"""SH4DER-PVR street driving demo: from a clean checkout to a disc image, one script.

    python demo/pl4nar/build.py --sh4zam <checkout> [--rtt] [--blender <exe>] [--mkdcdisc <exe>] [--out <dir>] [--pin]
    python demo/pl4nar/build.py --dry-run [the same options]      say what would run and what is missing; nothing is
                                                                 fetched, written or built (tools/test_build_dry.py)

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH). Steps, each refusing to go on
when it fails:

  1 credits   tools/check_credits.py: the fetch table and CREDITS.md agree
  2 fetch     tools/fetch.py: the CC0 packs into assets/ (sha256 checked; --pin accepts an unpinned pack once)
  3 coupe     tools/coupe.py: the hero car from numbers -> build/gen/coupe.obj; tools/props.py: the street furniture
              from numbers -> build/gen/props.obj
  4 lod       tools/car_lod.py in Blender, headless: traffic cars and the cone cut to the triangle budget -> build/lod/
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
        ("kenney_car-kit/cone.obj", 16, 16)]


def run(step, argv, cwd=ROOT):
    print("== %s" % step, flush=True)
    r = subprocess.run(argv, cwd=cwd)
    if r.returncode != 0:
        raise SystemExit("%s failed (exit %d)" % (step, r.returncode))


def dry_run(a):
    """Print the seven steps and the state of everything they need. Reads only: no network, no file written, no build.
    Exit code 0 when the credits check passes (a missing tool is reported, not an error: this is the list to fix)."""
    py = sys.executable
    have = lambda exe: "found" if shutil.which(exe) else "MISSING"   # noqa: E731
    try:
        import PIL  # noqa: F401
        pillow = "found"
    except ImportError:
        pillow = "MISSING"
    zam = "found" if a.sh4zam and os.path.isdir(os.path.join(a.sh4zam, "include", "sh4zam")) else "MISSING (--sh4zam)"
    print("DRY_RUN tools: kos-cc %s, make %s, SH4ZAM %s, Blender %s, mkdcdisc %s, Pillow %s"
          % (have("kos-cc"), have("make"), zam, have(a.blender), have(a.mkdcdisc), pillow))
    r = subprocess.run([py, os.path.join("tools", "check_credits.py")], cwd=ROOT, capture_output=True, text=True)
    print("DRY_RUN 1 credits: %s" % (r.stdout.strip().splitlines() or ["no output"])[-1])
    f = subprocess.run([py, os.path.join("tools", "fetch.py"), "--check"], cwd=ROOT, capture_output=True, text=True)
    rows = [ln.split() for ln in f.stdout.splitlines() if ln.strip()]
    print("DRY_RUN 2 fetch: %d packs, %d present, %d not pinned%s"
          % (len(rows), sum(1 for w in rows if w[0] == "have"), sum(1 for w in rows if w[-2:] == ["NOT", "pinned"]), " (would need --pin)" if not a.pin else ""))
    print("DRY_RUN 3 coupe: %s" % ("build/gen/coupe.obj is there, kept" if os.path.exists(os.path.join(GEN, "coupe.obj")) else "would run tools/coupe.py"))
    todo = [os.path.basename(s) for s, _b, _w in LODS if not os.path.exists(os.path.join(LOD, os.path.basename(s)))]
    print("DRY_RUN 4 lod: %d of %d models to cut in Blender%s" % (len(todo), len(LODS), ": " + ", ".join(todo) if todo else ""))
    print("DRY_RUN 5 bake: would run tools/bake.py -> build/gen/assets_gen.c, assets_gen.h")
    print("DRY_RUN 6 make: would run make SH4ZAM=... DEMO_BUILD=<n>%s -> build/pl4nar-demo.elf" % (" RTT=1" if a.rtt else " (no RTT: a 1024 wide target)"))
    n = 1
    if os.path.isdir(a.out):
        n = 1 + max([int(m.group(1)) for m in (re.match(r"pl4nar-demo-b(\d+)\.cdi$", x) for x in os.listdir(a.out)) if m] or [0])
    print("DRY_RUN 7 image: would write %s" % os.path.join(a.out, "pl4nar-demo-b%d.cdi" % n))
    print("DRY_RUN_OK" if r.returncode == 0 else "DRY_RUN_FAILED credits")
    return 0 if r.returncode == 0 else 1


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
    ap.add_argument("--dry-run", action="store_true", help="print the steps and what is missing; fetch, write and build nothing")
    a = ap.parse_args()
    if a.dry_run:
        return dry_run(a)
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
    if fresh(os.path.join(GEN, "props.obj")):
        run("props", [py, os.path.join("tools", "props.py"), os.path.join(GEN, "props.obj")])
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
