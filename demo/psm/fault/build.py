#!/usr/bin/env python3
"""SH4DER-PVR: the fault catcher's test image, build script.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/fault/build.py [--tag f1] [--out <dir>] [--cdi-dir <dir>] [--mkdcdisc <exe>] [--host-cc <cc>] [--host-only] [--elf-only]

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH, KOS_BASE set). It fetches nothing
and touches no file outside --out (default build/psm_fault) and --cdi-dir (default demo/psm/out/cdi, the one place
the images are kept; git ignores it). Steps, each refusing to go on when it fails:

  1 host test  test/fault_test.c with the host compiler: the report's text, the watchdog's logic, the stack scan
  2 elf        kos-cc psm_fault_f1.c -> <out>/psm-fault-<tag>.elf
  3 image      mkdcdisc -> <cdi-dir>/psm-fault-<tag>.cdi. An image that is there is never overwritten: pick a new --tag

--host-only stops after step 1 (no KallistiOS needed). --elf-only stops after step 2 (no mkdcdisc needed).
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


def main():
    ap = argparse.ArgumentParser(description="Build the fault catcher's test image.")
    ap.add_argument("--tag", default="f1")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "psm_fault"))
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "demo", "psm", "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--host-cc", default=os.environ.get("HOST_CC", "gcc"))
    ap.add_argument("--host-only", action="store_true")
    ap.add_argument("--elf-only", action="store_true")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    image = os.path.join(os.path.abspath(a.cdi_dir), "psm-fault-%s.cdi" % a.tag)
    elf = os.path.join(out, "psm-fault-%s.elf" % a.tag)
    if not a.host_only and not a.elf_only and os.path.exists(image):
        raise SystemExit("refuse existing image: %s (pick a new --tag)" % image)

    test = os.path.join(out, "fault_test.exe" if os.name == "nt" else "fault_test")
    run("1 host test (compile)", [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + os.path.join(ROOT, "include"),
                                  os.path.join(ROOT, "test", "fault_test.c"), "-o", test])
    run("1 host test (run)", [test])
    if a.host_only:
        return 0

    kos_cc = find_kos_cc()
    if not os.environ.get("KOS_BASE") or not kos_cc:
        raise SystemExit("kos-cc is not on the PATH or KOS_BASE is not set: source the KallistiOS environment first")
    run("2 elf", kos_cc + ["-O2", "-Wall", "-Wextra", "-I" + os.path.join(ROOT, "include"), os.path.join(HERE, "psm_fault_f1.c"), "-o", elf])
    if a.elf_only:
        print("ELF: %s" % elf)
        return 0

    if os.path.exists(image):
        raise SystemExit("refuse existing image: %s" % image)
    os.makedirs(os.path.dirname(image), exist_ok=True)
    run("3 image", [a.mkdcdisc, "-e", elf, "-o", image, "-N", "-n", "PSM-FAULT"])
    print("IMAGE: %s" % image)
    return 0


if __name__ == "__main__":
    sys.exit(main())
