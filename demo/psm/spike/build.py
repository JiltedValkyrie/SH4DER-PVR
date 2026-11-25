#!/usr/bin/env python3
"""SH4DER-PVR: route AB spike, build script.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/spike/build.py [--tag s4] [--out <dir>] [--cdi-dir <dir>] [--mkdcdisc <exe>] [--host-cc <cc>] [--host-only] [--elf-only]

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH, KOS_BASE set). It fetches nothing
and touches no file outside --out (default build/psm_ab_spike) and --cdi-dir (default demo/psm/out/cdi, the one
place the images are kept; git ignores it). Steps, each refusing to go on when it fails:

  1 host test  test/ab_model_test.c with the host compiler: the model the console's check uses
  2 rtt        the KallistiOS under KOS_BASE must declare pvr_scene_begin_rtt; if not, it says so and stops
  3 elf        kos-cc ab_spike.c -> <out>/psm-ab-spike-<tag>.elf
  4 image      mkdcdisc -> <cdi-dir>/psm-ab-spike-<tag>.cdi. An image that is there is never overwritten: pick a new --tag

--host-only stops after step 1 (no KallistiOS needed). --elf-only stops after step 3 (no mkdcdisc needed).
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))


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


def has_rtt(kos_base):
    hdr = os.path.join(kos_base, "kernel", "arch", "dreamcast", "include", "dc", "pvr.h")
    try:
        with open(hdr, encoding="utf-8", errors="replace") as f:
            return "pvr_scene_begin_rtt" in f.read()
    except OSError:
        return False


def main():
    ap = argparse.ArgumentParser(description="Build the route AB spike image.")
    ap.add_argument("--tag", default="s4")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "psm_ab_spike"))
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "demo", "psm", "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--host-cc", default=os.environ.get("HOST_CC", "gcc"))
    ap.add_argument("--host-only", action="store_true")
    ap.add_argument("--elf-only", action="store_true")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    image = os.path.join(os.path.abspath(a.cdi_dir), "psm-ab-spike-%s.cdi" % a.tag)
    elf = os.path.join(out, "psm-ab-spike-%s.elf" % a.tag)
    if not a.host_only and not a.elf_only and os.path.exists(image):
        raise SystemExit("refuse existing image: %s (pick a new --tag)" % image)

    test = os.path.join(out, "ab_model_test.exe" if os.name == "nt" else "ab_model_test")
    run("1 host test (compile)", [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror",
                                  os.path.join(HERE, "test", "ab_model_test.c"), "-lm", "-o", test])
    run("1 host test (run)", [test])
    if a.host_only:
        return 0

    kos_base = os.environ.get("KOS_BASE", "")
    kos_cc = find_kos_cc()
    if not kos_base or not kos_cc:
        raise SystemExit("kos-cc is not on the PATH or KOS_BASE is not set: source the KallistiOS environment first")
    if not has_rtt(kos_base):
        raise SystemExit("this KallistiOS (%s) does not declare pvr_scene_begin_rtt: the spike needs it, stopping" % kos_base)
    print("== 2 rtt: pvr_scene_begin_rtt is declared in %s" % kos_base)

    run("3 elf", kos_cc + ["-O2", "-Wall", "-Wextra", "-I" + HERE, os.path.join(HERE, "ab_spike.c"), "-o", elf])
    if a.elf_only:
        print("ELF: %s" % elf)
        return 0

    if os.path.exists(image):
        raise SystemExit("refuse existing image: %s" % image)
    os.makedirs(os.path.dirname(image), exist_ok=True)
    run("4 image", [a.mkdcdisc, "-e", elf, "-o", image, "-N", "-n", "PSM-AB-SPIKE"])
    print("IMAGE: %s" % image)
    return 0


if __name__ == "__main__":
    sys.exit(main())
