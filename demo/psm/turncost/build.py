#!/usr/bin/env python3
"""SH4DER-PVR: the camera-turn cost test (the turn plan's step 0), build script.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/turncost/build.py --tag <new tag> [--pad 0|8192] [--pad-mn 0|8192] [--pad-lut 0|8192] [--ship] [--keep] [--sink-inline] [--out <dir>] [--cdi-dir <dir>] [--mkdcdisc <exe>]
                                      [--sh4zam <dir>] [--sh4zam-dce <dir>] [--host-only] [--elf-only]

The program is the walled square (demo/psm/psm_dc.c, SQUARE=square) built with PSM_VIEWCOST and VC_PART 8: it boots into
the vc-g script of demo/psm/psm_viewcost.h (36 stages: routes B and A; the sky, the boot view, down; still, still with the
placement forced every frame, turning; the plain placement f0 and all three options f7), times p0 in its three steps,
and ends on two text pages and the graphs (Start on the last page runs it again). --pad puts that many bytes between
mclip and mscr in psm_frame_t (PSM_PAD_MCLIP, the plan's 2a); build one image with 0 and one with 8192 for the A/B.
8192 is psm_scene.h's default since the t2 A/B (2026-11-28); --pad always passes the value, so an image says what it has.

Run it in an MSYS2 shell where the KallistiOS environment is sourced (kos-cc on the PATH, KOS_BASE set). It fetches
nothing and touches no file outside --out (default build/psm_turncost) and --cdi-dir (default out/cdi). Steps, each
refusing to go on when it fails:

  1 host tests  demo/psm/test/psm_viewcost_test.c with VC_PART 8, and test/viewcull_test.c, with the host compiler
  2 inputs      pvr_scene_begin_rtt declared under KOS_BASE; the Fantasy Town Kit zip for the square (no stand-in boxes)
  3 elf         make in a build folder of its own, <out>/<tag>, then a copy to <out>/psm-turncost-<tag>.elf. A tag whose
                folder or ELF is there is refused: pick a new --tag
  4 image       mkdcdisc -> <cdi-dir>/psm-turncost-<tag>.cdi and the build note .txt beside it; never overwritten

--host-only stops after step 1, --elf-only after step 3 (no mkdcdisc needed).
"""
import argparse
import hashlib
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
ZIP = os.path.join(ROOT, "demo", "psm", "assets", "_dl", "kenney_fantasy-town-kit_2.0.zip")


def run(step, argv):
    print("== %s: %s" % (step, " ".join(argv)), flush=True)
    r = subprocess.run(argv, cwd=ROOT)
    if r.returncode:
        raise SystemExit("%s failed (rc %d)" % (step, r.returncode))


def git(*args):
    return subprocess.run(["git"] + list(args), cwd=ROOT, stdout=subprocess.PIPE, check=True).stdout


def tree_state():
    """(HEAD, branch, sha256 of the working tree against HEAD, [changed files]) for the library."""
    head = git("rev-parse", "HEAD").decode().strip()
    branch = git("rev-parse", "--abbrev-ref", "HEAD").decode().strip()
    h = hashlib.sha256()
    h.update(git("diff", "HEAD"))
    files = [l[3:].strip().strip('"') for l in git("status", "--porcelain").decode().splitlines()]
    for f in sorted(l for l in git("ls-files", "--others", "--exclude-standard").decode().splitlines()):
        h.update(b"\0untracked\0" + f.encode())
        try:
            with open(os.path.join(ROOT, f), "rb") as fh:
                h.update(fh.read())
        except OSError:
            pass
    return head, branch, h.hexdigest(), files


def has_rtt(kos_base):
    try:
        with open(os.path.join(kos_base, "kernel", "arch", "dreamcast", "include", "dc", "pvr.h"), encoding="utf-8", errors="replace") as f:
            return "pvr_scene_begin_rtt" in f.read()
    except OSError:
        return False


def main():
    ap = argparse.ArgumentParser(description="Build the camera-turn cost test image (vc-g).")
    ap.add_argument("--tag", required=True)
    ap.add_argument("--pad", type=int, default=8192, choices=(0, 8192))
    ap.add_argument("--pad-mn", type=int, default=0, choices=(0, 8192), help="PSM_PAD_MN, the turn plan's 2b")
    ap.add_argument("--ship", action="store_true", help="every stage with the lazy plan and the colour table + seen only (VC_SHIP_OPT 56)")
    ap.add_argument("--keep", action="store_true", help="every stage +256: PSM_KEEP_SUN and the sun stepped a clock minute (the turn plan's step 2)")
    ap.add_argument("--sink-inline", action="store_true", help="PSM_SINK_INLINE=1: the sink's hot path inline (step 2 b)")
    ap.add_argument("--bound", action="store_true", help="the closed square (SQUARE=closed) and every stage +512: PSM_WORLD_BOUND and its fog (the world step)")
    ap.add_argument("--ground", action="store_true", help="every stage +128: PSM_PLACE_GROUND, the ground window culled a block at a time (step 4)")
    ap.add_argument("--vis", action="store_true", help="every stage +1024: PSM_PLACE_VIS, the models' seen marks kept on a still camera (step 4)")
    ap.add_argument("--cold", action="store_true", help="PSM_COLD=1: load, text and page code marked cold, out of the frame's code (step 4 c)")
    ap.add_argument("--pad-lut", type=int, default=0, choices=(0, 8192), help="PSM_PAD_LUT, 2d")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "psm_turncost"))
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--host-cc", default=os.environ.get("HOST_CC", "gcc"))
    ap.add_argument("--make", default=os.environ.get("MAKE", "make"))
    ap.add_argument("--sh4zam", default=os.path.join(ROOT, "third_party", "sh4zam"))
    ap.add_argument("--sh4zam-dce", default=os.path.join(ROOT, "third_party", "sh4zam-dce"))
    ap.add_argument("--host-only", action="store_true")
    ap.add_argument("--elf-only", action="store_true")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    bdir = os.path.join(out, a.tag)
    elf = os.path.join(out, "psm-turncost-%s.elf" % a.tag)
    image = os.path.join(os.path.abspath(a.cdi_dir), "psm-turncost-%s.cdi" % a.tag)
    note = os.path.join(os.path.abspath(a.cdi_dir), "psm-turncost-%s.txt" % a.tag)
    if not a.host_only and (os.path.exists(bdir) or os.path.exists(elf)):
        raise SystemExit("refuse existing build folder or ELF for tag %s: %s (pick a new --tag)" % (a.tag, bdir))
    if not a.host_only and not a.elf_only and (os.path.exists(image) or os.path.exists(note)):
        raise SystemExit("refuse existing image or note: %s (pick a new --tag)" % image)

    os.makedirs(out, exist_ok=True)
    exe = ".exe" if os.name == "nt" else ""
    host = [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DSH4DER_CORE_NO_SH4ZAM", "-I" + os.path.join(ROOT, "include")]
    t1 = os.path.join(out, "psm_viewcost8_test" + exe)
    add = (56 if a.ship else 0) | (256 if a.keep else 0) | (512 if a.bound else 0) | (128 if a.ground else 0) | (1024 if a.vis else 0)
    square = "closed" if a.bound else "square"
    ship = ["-DVC_SHIP_OPT=%du" % add] if add else []
    run("1 host test psm_viewcost_test VC_PART 8 (compile)", host + ["-DVC_PART=8"] + ship + [ "-I" + os.path.join(ROOT, "demo", "psm"),
                                                                   os.path.join(ROOT, "demo", "psm", "test", "psm_viewcost_test.c"), "-o", t1, "-lm"])
    run("1 host test psm_viewcost_test VC_PART 8 (run)", [t1])
    t2 = os.path.join(out, "viewcull_test" + exe)
    run("1 host test viewcull_test (compile)", host + [os.path.join(ROOT, "test", "viewcull_test.c"), "-o", t2, "-lm"])
    run("1 host test viewcull_test (run)", [t2])
    if a.host_only:
        return 0

    kos_base = os.environ.get("KOS_BASE", "")
    if not kos_base or not any(os.path.isfile(os.path.join(d, "kos-cc")) for d in os.environ.get("PATH", "").split(os.pathsep)):
        raise SystemExit("kos-cc is not on the PATH or KOS_BASE is not set: source the KallistiOS environment first")
    if not has_rtt(kos_base):
        raise SystemExit("this KallistiOS (%s) does not declare pvr_scene_begin_rtt: route A needs it, stopping" % kos_base)
    if not os.path.isfile(ZIP):
        raise SystemExit("no %s: the square would bake as stand-in boxes, stopping (demo/psm/fetch_table.json)" % ZIP)
    print("== 2 inputs: rtt declared in %s; the square's zip is there" % kos_base)

    if os.path.exists(bdir) or os.path.exists(elf):
        raise SystemExit("refuse existing build folder or ELF: %s" % bdir)
    head, branch, tree, files = tree_state()
    extra = "-DPSM_VIEWCOST -DVC_PART=8 -DPSM_PAD_MCLIP=%du -DPSM_PAD_MN=%du -DPSM_PAD_LUT=%du" % (a.pad, a.pad_mn, a.pad_lut) + (" -DVC_SHIP_OPT=%du" % add if add else "") + (" -DPSM_SINK_INLINE=1" if a.sink_inline else "") + (" -DPSM_COLD=1" if a.cold else "")
    made = os.path.join(bdir, "psm_dc", "sh4der-psm-ab.elf")
    run("3 elf", [a.make, "BUILD=" + bdir, "SQUARE=" + square, "SH4ZAM=" + a.sh4zam, "SH4ZAM_DCE=" + a.sh4zam_dce, "PSM_DC_EXTRA=" + extra, made])
    with open(made, "rb") as f:
        data = f.read()
    with open(elf, "xb") as f:                 # never over an ELF that is there
        f.write(data)
    print("ELF: %s (pads clip %d mn %d lut %d B, SQUARE=%s)" % (elf, a.pad, a.pad_mn, a.pad_lut, square))
    if a.elf_only:
        return 0

    if os.path.exists(image) or os.path.exists(note):
        raise SystemExit("refuse existing image or note: %s" % image)
    os.makedirs(os.path.dirname(image), exist_ok=True)
    run("4 image", [a.mkdcdisc, "-e", elf, "-o", image, "-N", "-n", "PSM-TURNCOST"])
    with open(note, "x", encoding="utf-8", newline="\n") as f:
        f.write("camera-turn cost test (vc-g), build %s, pads clip %d mn %d lut %d B\nimage: %s\nelf sha256: %s\n"
                % (a.tag, a.pad, a.pad_mn, a.pad_lut, os.path.basename(image), hashlib.sha256(data).hexdigest()))
        f.write("library: branch %s, HEAD %s\nworking tree sha256 (git diff HEAD + untracked files): %s\n" % (branch, head, tree))
        f.write("flags: SQUARE=%s PSM_DC_EXTRA=%s\nfiles that differ from HEAD (%d):\n" % (square, extra, len(files)))
        for p in files:
            f.write("  %s\n" % p)
        f.write("KOS_BASE: %s\nSH4ZAM: %s\n" % (kos_base, a.sh4zam))
    print("IMAGE: %s" % image)
    print("NOTE:  %s" % note)
    return 0


if __name__ == "__main__":
    sys.exit(main())
