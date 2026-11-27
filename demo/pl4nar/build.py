#!/usr/bin/env python3
"""SH4DER-PVR street driving demo: from a clean checkout to a disc image, one script.

    python demo/pl4nar/build.py --sh4zam <checkout> [--tech] [--hills] [--street] [--rtt] [--blender <exe>] [--mkdcdisc <exe>] [--out <dir>] [--pin]
    python demo/pl4nar/build.py --dry-run [the same options]      say what would run and what is missing; nothing is
                                                                 fetched, written or built (tools/test_build_dry.py)

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH). Two builds that never mix:

  the map (the default)  the closed loop of tools/map.py: generated files in build/gen_map
  --street               the first street layout: generated files in build/gen

Two programs from one source, each with its own objects folder and image name (none is shared, none overwritten):

  (default)  DRIVE: free driving, the game HUD (time, laps on the map, one line of keys); START opens the tech overlay
             and its keys (PL4NAR_DRIVE). Objects build/<layout>_drive, image <out>/pl4nar-drive-<layout>-b<N>.cdi
  --tech     TECH DEMO: the measuring autorun, its stages then its result pages (PL4NAR_AUTORUN, ../psm's headers).
             Objects build/<layout>_tech, image <out>/pl4nar-tech-<layout>-b<N>.cdi
  --hills    (the map) the hills (PL4NAR_HILLS): <layout> is map_hills

Steps, each refusing to go on when it fails:

  1  credits    tools/check_credits.py: the fetch table and CREDITS.md agree
  1b controls   tools/check_controls.py: the README's Controls table and src/controls.h agree
  2  fetch      tools/fetch.py: the CC0 packs into assets/ (sha256 checked; --pin accepts an unpinned pack once)
  3  coupe      tools/coupe.py: the hero car from numbers -> <gen>/coupe.obj; tools/props.py: the street furniture
                from numbers -> build/gen/props.obj (both builds read it)
  3b structures (map) tools/structures.py: the overpass, footbridge, gantry, passage -> build/gen_map/structures.obj
  4  lod        tools/car_lod.py in Blender, headless: traffic cars and the cone cut to the triangle budget -> build/lod/
  5  bake       tools/bake.py: models and textures -> <gen>/assets_gen.c, assets_gen.h
  5b map        (map) tools/map.py: the loop, its instances, lamps, strips and traffic -> build/gen_map/map_gen.c, .h
                (the kit buildings are cut in Blender into build/lod_map_d/ the first time)
  5c map_ao     (map) tools/map_ao.c built with the HOST C compiler (HOST_CC, else gcc, cc or clang on the PATH) and
                run: the baked occlusion -> build/gen_map/map_ao_gen.c, .h
  6  make       the Dreamcast program -> build/<layout>_<drive|tech>/pl4nar-demo.elf
  7  image      mkdcdisc -> the image above, N one more than the highest there (an image is never overwritten)

External tools, not part of this repository: KallistiOS (its own licence), SH4ZAM (MIT), Blender (GPL; used as a
tool, nothing of it is shipped), mkdcdisc, Python 3 with Pillow, and for the map a host C compiler. --rtt: your
KallistiOS has pvr_scene_begin_rtt. Standard library only. Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.
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
GEN_MAP = os.path.join(ROOT, "build", "gen_map")   # the map's generated files: never build/gen's
BUILD_MAP = os.path.join(ROOT, "build", "map")     # the map's host tool (map_ao); the programs' objects: program()
LOD_MAP = os.path.join(ROOT, "build", "lod_map_d")  # tools/map.py's cut kit buildings
# source OBJ (under assets/), body budget, wheel budget: the triangle budget of a late 1990s console street
LODS = [("kenney_car-kit/taxi.obj", 186, 16), ("kenney_car-kit/police.obj", 186, 16), ("kenney_car-kit/van.obj", 186, 16),
        ("kenney_car-kit/cone.obj", 16, 16)]


def run(step, argv, cwd=ROOT):
    print("== %s" % step, flush=True)
    try:
        r = subprocess.run(argv, cwd=cwd)
    except FileNotFoundError:
        raise SystemExit("%s: no such program: %s" % (step, argv[0]))
    if r.returncode != 0:
        raise SystemExit("%s failed (exit %d)" % (step, r.returncode))


def kos_cc():
    """kos-cc on the PATH, or KallistiOS's own wrapper under KOS_BASE: a Windows Python does not see a shell script with
    no extension on the PATH, and make runs it through the shell either way."""
    w = os.path.join(os.environ.get("KOS_BASE", ""), "utils", "build_wrappers", "kos-cc")
    return shutil.which("kos-cc") or (w if os.environ.get("KOS_BASE") and os.path.isfile(w) else None)


def host_cc():
    """The host C compiler for tools/map_ao.c: HOST_CC, else gcc, cc or clang on the PATH; None when there is none.
    Never kos-cc or sh-elf-gcc: that builds for the Dreamcast, and map_ao runs here."""
    env = os.environ.get("HOST_CC", "")
    if env:
        return env if shutil.which(env) else None
    for exe in ("gcc", "cc", "clang"):
        if shutil.which(exe):
            return exe
    return None


def shz_sw(sh4zam):
    """SH4ZAM's software XMTRX (src/map.c uses it through the headers on the host), or None."""
    p = os.path.join(sh4zam or "", "source", "sw", "shz_xmtrx_sw.c")
    return p if sh4zam and os.path.isfile(p) else None


def map_ao_cmd(cc, sh4zam, exe):
    lib = os.path.dirname(os.path.dirname(ROOT))
    j = os.path.join
    return [cc, "-O2", "-std=gnu11", "-Wall", "-Wextra", "-I" + j(ROOT, "src"), "-I" + GEN_MAP, "-I" + j(lib, "include"),
            "-I" + j(sh4zam, "include"),
            j(ROOT, "tools", "map_ao.c"), j(ROOT, "src", "map.c"), j(GEN_MAP, "assets_gen.c"), j(GEN_MAP, "map_gen.c"),
            shz_sw(sh4zam), "-lm", "-o", exe]


def program(a):
    """The program and the layout: (objects folder relative to ROOT, image stem, volume name, make arguments)."""
    layout = "street" if a.street else ("map_hills" if a.hills else "map")
    kind = "tech" if a.tech else "drive"
    stem = "pl4nar-%s-%s" % (kind, layout.replace("_", "-"))
    vol = "PL4NAR-%s-%s" % (kind.upper(), {"street": "S", "map": "M", "map_hills": "H"}[layout])
    extra = (["-DPL4NAR_HILLS"] if a.hills else []) + ([] if a.tech else ["-DPL4NAR_DRIVE"])
    args = ["BUILD=build/%s_%s" % (layout, kind)] + ([] if a.street else ["MAP=1", "GEN=build/gen_map"])
    if extra:
        args.append("EXTRA=" + " ".join(extra))
    return "build/%s_%s" % (layout, kind), stem, vol, args


def next_image(out, stem):
    n = 1
    if os.path.isdir(out):
        n = 1 + max([int(m.group(1)) for m in (re.match(re.escape(stem) + r"-b(\d+)\.cdi$", x) for x in os.listdir(out)) if m] or [0])
    return n, os.path.join(out, "%s-b%d.cdi" % (stem, n))


def dry_run(a):
    """Print the steps of the build chosen (the map, or --street) and the state of everything they need. Reads only: no
    network, no file written, no build. Exit code 0 when the credits check passes (a missing tool is reported, not an
    error: this is the list to fix)."""
    py = sys.executable
    street = a.street
    gen = GEN if street else GEN_MAP
    rel = lambda p: os.path.relpath(p, ROOT).replace(os.sep, "/")   # noqa: E731
    have = lambda exe: "found" if shutil.which(exe) else "MISSING"   # noqa: E731
    try:
        import PIL  # noqa: F401
        pillow = "found"
    except ImportError:
        pillow = "MISSING"
    zam = "found" if a.sh4zam and os.path.isdir(os.path.join(a.sh4zam, "include", "sh4zam")) else "MISSING (--sh4zam)"
    cc = host_cc()
    print("DRY_RUN build: %s, %s" % ("the street (--street): build/gen" if street else "the map (default): build/gen_map",
                                    "TECH DEMO (--tech)" if a.tech else "DRIVE (the default)"))
    print("DRY_RUN tools: kos-cc %s, make %s, SH4ZAM %s, Blender %s, mkdcdisc %s, Pillow %s%s"
          % ("found" if kos_cc() else "MISSING", have("make"), zam, have(a.blender), have(a.mkdcdisc), pillow,
             "" if street else ", host cc %s" % (cc if cc else "MISSING (HOST_CC, gcc, cc or clang)")))
    r = subprocess.run([py, os.path.join("tools", "check_credits.py")], cwd=ROOT, capture_output=True, text=True)
    print("DRY_RUN 1 credits: %s" % (r.stdout.strip().splitlines() or ["no output"])[-1])
    r = subprocess.run([py, os.path.join("tools", "check_controls.py")], cwd=ROOT, capture_output=True, text=True)
    print("DRY_RUN 1b controls: %s" % (r.stdout.strip().splitlines() or ["no output"])[-1])
    f = subprocess.run([py, os.path.join("tools", "fetch.py"), "--check"], cwd=ROOT, capture_output=True, text=True)
    rows = [ln.split() for ln in f.stdout.splitlines() if ln.strip()]
    print("DRY_RUN 2 fetch: %d packs, %d present, %d not pinned%s"
          % (len(rows), sum(1 for w in rows if w[0] == "have"), sum(1 for w in rows if w[-2:] == ["NOT", "pinned"]), " (would need --pin)" if not a.pin else ""))
    coupe = coupe_obj(gen)
    print("DRY_RUN 3 coupe: %s" % ("%s is there, kept" % rel(coupe) if os.path.exists(coupe) else "would run tools/coupe.py -> %s" % rel(coupe)))
    if not street:
        st = os.path.join(GEN_MAP, "structures.obj")
        print("DRY_RUN 3b structures: %s" % ("%s is there, kept" % rel(st) if os.path.exists(st) else "would run tools/structures.py -> %s" % rel(st)))
    todo = [os.path.basename(s) for s, _b, _w in LODS if not os.path.exists(os.path.join(LOD, os.path.basename(s)))]
    print("DRY_RUN 4 lod: %d of %d models to cut in Blender%s" % (len(todo), len(LODS), ": " + ", ".join(todo) if todo else ""))
    print("DRY_RUN 5 bake: would run tools/bake.py -> %s/assets_gen.c, assets_gen.h" % rel(gen))
    if not street:
        cut = len(os.listdir(LOD_MAP)) if os.path.isdir(LOD_MAP) else 0
        print("DRY_RUN 5b map: would run tools/map.py -> build/gen_map/map_gen.c, map_gen.h (%d kit models already cut in build/lod_map_d)" % cut)
        print("DRY_RUN 5c map_ao: %s" % ("would build tools/map_ao.c with %s and run it -> build/gen_map/map_ao_gen.c, map_ao_gen.h" % cc if cc
                                         else "REFUSES: no host C compiler (HOST_CC, gcc, cc or clang on the PATH)"))
    folder, stem, vol, args = program(a)
    print("DRY_RUN 6 make: would run make SH4ZAM=... DEMO_BUILD=<n> %s%s%s -> %s/pl4nar-demo.elf"
          % (" ".join(args), " AUTORUN=1 TAG=<image name>" if a.tech else "", " RTT=1" if a.rtt else " (no RTT: a 1024 wide target)", folder))
    print("DRY_RUN 7 image: would write %s (volume %s)" % (next_image(a.out, stem)[1], vol))
    print("DRY_RUN_OK" if r.returncode == 0 else "DRY_RUN_FAILED credits")
    return 0 if r.returncode == 0 else 1


def coupe_obj(gen):
    """The hero car's OBJ, named by the hash of tools/coupe.py: a changed coupe gets a new file and an old one is never
    baked again (a kept coupe.obj from an earlier coupe.py once put a cracked boot in the map builds)."""
    import hashlib
    with open(os.path.join(ROOT, "tools", "coupe.py"), "rb") as fh:
        return os.path.join(gen, "coupe-%s.obj" % hashlib.sha256(fh.read()).hexdigest()[:8])


def fresh(path):
    """A generated file is rebuilt under a name that is free: move nothing, delete nothing."""
    return not os.path.exists(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--sh4zam", default=os.environ.get("SH4ZAM", ""))
    ap.add_argument("--blender", default=os.environ.get("BLENDER", "blender"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--out", default=os.path.normpath(os.path.join(ROOT, "..", "..", "out", "cdi")))
    ap.add_argument("--rtt", action="store_true", help="the KallistiOS in use has pvr_scene_begin_rtt")
    ap.add_argument("--pin", action="store_true", help="accept packs that have no sha256 in the table yet")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--dry-run", action="store_true", help="print the steps and what is missing; fetch, write and build nothing")
    ap.add_argument("--street", action="store_true", help="the first street layout (build/gen) in place of the map")
    ap.add_argument("--tech", action="store_true", help="TECH DEMO, the measuring autorun, in place of DRIVE")
    ap.add_argument("--hills", action="store_true", help="the map with the hills (PL4NAR_HILLS)")
    a = ap.parse_args()
    if a.hills and a.street:
        raise SystemExit("--hills is the map's: not with --street")
    if a.dry_run:
        return dry_run(a)
    if not a.street and not host_cc():               # the map's host step: refused first, before anything else is looked at
        raise SystemExit("no host C compiler: the map_ao step builds tools/map_ao.c for this machine. Put gcc, cc or "
                         "clang on the PATH, or set HOST_CC (kos-cc builds for the Dreamcast and will not do)")
    if not a.sh4zam or not os.path.isdir(os.path.join(a.sh4zam, "include", "sh4zam")):
        raise SystemExit("--sh4zam: a SH4ZAM checkout (it has include/sh4zam)")
    if not kos_cc():
        raise SystemExit("kos-cc is not on the PATH: source the KallistiOS environment first")
    py = sys.executable
    street = a.street
    gen = GEN if street else GEN_MAP
    if not street:
        if not shz_sw(a.sh4zam):
            raise SystemExit("--sh4zam has no source/sw/shz_xmtrx_sw.c: the map_ao step needs SH4ZAM's software XMTRX on the host")
    os.makedirs(GEN, exist_ok=True)
    os.makedirs(gen, exist_ok=True)
    os.makedirs(LOD, exist_ok=True)
    run("credits", [py, os.path.join("tools", "check_credits.py")])
    run("controls", [py, os.path.join("tools", "check_controls.py")])
    run("fetch", [py, os.path.join("tools", "fetch.py")] + (["--pin"] if a.pin else []))
    coupe = coupe_obj(gen)
    if fresh(coupe):
        run("coupe", [py, os.path.join("tools", "coupe.py"), coupe])
    if fresh(os.path.join(GEN, "props.obj")):
        run("props", [py, os.path.join("tools", "props.py"), os.path.join(GEN, "props.obj")])
    if not street and fresh(os.path.join(GEN_MAP, "structures.obj")):
        run("structures", [py, os.path.join("tools", "structures.py"), os.path.join(GEN_MAP, "structures.obj")])
    todo = [(s, b, w) for s, b, w in LODS if fresh(os.path.join(LOD, os.path.basename(s)))]
    if todo and not shutil.which(a.blender):
        raise SystemExit("Blender not found (--blender <exe> or BLENDER): the lod step cuts %d models with it" % len(todo))
    for src, body, wheel in todo:
        run("lod %s" % os.path.basename(src),
            [a.blender, "-b", "--factory-startup", "--python", os.path.join("tools", "car_lod.py"), "--",
             os.path.join("assets", *src.split("/")), os.path.join(LOD, os.path.basename(src)), "--body", str(body), "--wheel", str(wheel)])
    if street:
        run("bake", [py, os.path.join("tools", "bake.py"), "--coupe", coupe])
    else:
        run("bake", [py, os.path.join("tools", "bake.py"), "--out", GEN_MAP, "--coupe", coupe])
        run("map", [py, os.path.join("tools", "map.py"), "--out", GEN_MAP, "--structures", os.path.join(GEN_MAP, "structures.obj")]
            + (["--blender", a.blender] if shutil.which(a.blender) else []))
        exe = os.path.join(BUILD_MAP, "host", "map_ao" + (".exe" if os.name == "nt" else ""))
        os.makedirs(os.path.dirname(exe), exist_ok=True)
        run("map_ao cc", map_ao_cmd(host_cc(), a.sh4zam, exe))
        run("map_ao", [exe, GEN_MAP])
    os.makedirs(a.out, exist_ok=True)
    folder, stem, vol, args = program(a)
    n, image = next_image(a.out, stem)
    make = ["make", "-j%d" % a.jobs, "SH4ZAM=%s" % a.sh4zam.replace(os.sep, "/"), "DEMO_BUILD=%d" % n] + (["RTT=1"] if a.rtt else []) + args
    if a.tech:
        make += ["AUTORUN=1", "TAG=%s" % os.path.splitext(os.path.basename(image))[0]]
    run("make", make)
    if os.path.exists(image):
        raise SystemExit("refuse existing image: %s" % image)
    elf = os.path.join(ROOT, *folder.split("/"), "pl4nar-demo.elf")
    run("image", [a.mkdcdisc, "-e", elf, "-o", image, "-N", "-n", vol])
    print("image: %s" % image)
    return 0


if __name__ == "__main__":
    sys.exit(main())
