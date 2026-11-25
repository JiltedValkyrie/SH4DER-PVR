#!/usr/bin/env python3
"""SH4DER-PVR: the QR reading test (image psm-qr-<tag>), build script.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/qr/build.py [--tag <tag>] [--out <dir>] [--cdi-dir <dir>] [--mkdcdisc <exe>] [--host-cc <cc>]
                                [--host-only] [--elf-only]

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH, KOS_BASE set). The QR encoder is
Project Nayuki's QR Code generator library (MIT License); it is not in this repository. Step 0 runs ../tools/fetch.py,
which downloads the files of ../fetch_table.json that are not there yet into ../assets/_dl and checks every sha256.
Nothing else is fetched, and no file is touched outside --out (default build/psm_qr), ../assets/_dl and --cdi-dir
(default demo/psm/out/cdi, the one place the images are kept; git ignores it). Steps, each refusing to go on when it
fails:

  0 fetch      ../tools/fetch.py (sha256 checked)
  1 host test  test/qr_test.c with the host compiler, then ../tools/qr_check.py --self-test and --matrices on its dump:
               every code decoded again by a decoder that shares no code with the encoder
  2 elf        kos-cc qr_spike.c and the encoder -> <out>/psm-qr-<tag>.elf
  3 image      mkdcdisc -> <cdi-dir>/psm-qr-<tag>.cdi. An image that is there is never overwritten: pick a new --tag

--host-only stops after step 1 (no KallistiOS needed). --elf-only stops after step 2 (no mkdcdisc needed).
"""
import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
DL = os.path.join(HERE, "..", "assets", "_dl")
TOOLS = os.path.join(HERE, "..", "tools")


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
    ap = argparse.ArgumentParser(description="Build the QR reading test.")
    ap.add_argument("--tag", default="q1")
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "psm_qr"))
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "demo", "psm", "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--host-cc", default=os.environ.get("HOST_CC", "gcc"))
    ap.add_argument("--host-only", action="store_true")
    ap.add_argument("--elf-only", action="store_true")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    image = os.path.join(os.path.abspath(a.cdi_dir), "psm-qr-%s.cdi" % a.tag)
    elf = os.path.join(out, "psm-qr-%s.elf" % a.tag)
    if not a.host_only and not a.elf_only and os.path.exists(image):
        raise SystemExit("refuse existing image: %s (pick a new --tag)" % image)

    run("0 fetch", [sys.executable, os.path.join(TOOLS, "fetch.py")])
    enc = os.path.join(DL, "qrcodegen.c")
    exe = ".exe" if os.name == "nt" else ""
    test = os.path.join(out, "qr_test" + exe)
    run("1 host test (compile)", [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + DL, os.path.join(HERE, "test", "qr_test.c"), enc, "-o", test])
    run("1 host test (run)", [test, out])
    run("1 decoder self-test", [sys.executable, os.path.join(TOOLS, "qr_check.py"), "--self-test"])
    run("1 decode every code", [sys.executable, os.path.join(TOOLS, "qr_check.py"), "--matrices", os.path.join(out, "qr_matrices.txt")])
    if a.host_only:
        return 0

    kos_cc = find_kos_cc()
    if not os.environ.get("KOS_BASE") or not kos_cc:
        raise SystemExit("kos-cc is not on the PATH or KOS_BASE is not set: source the KallistiOS environment first")
    run("2 elf", kos_cc + ["-O2", "-Wall", "-Wextra", "-I" + DL, "-DQR_TAG=%s" % a.tag, os.path.join(HERE, "qr_spike.c"), enc, "-o", elf])
    if a.elf_only:
        print("ELF: %s" % elf)
        return 0

    if os.path.exists(image):
        raise SystemExit("refuse existing image: %s" % image)
    os.makedirs(os.path.dirname(image), exist_ok=True)
    run("3 image", [a.mkdcdisc, "-e", elf, "-o", image, "-N", "-n", "PSM-QR"])
    print("IMAGE: %s" % image)
    return 0


if __name__ == "__main__":
    sys.exit(main())
