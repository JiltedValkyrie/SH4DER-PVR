#!/usr/bin/env python3
"""SH4DER-PVR: the ao0 image (AO method 2's hardware questions), build script.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/ao/build.py --tag <tag> [--out <dir>] [--cdi-dir <dir>] [--mkdcdisc <exe>] [--host-cc <cc>]
                                [--sh4zam <checkout>] [--town-inc <dir> ...] [--host-only] [--elf-only]

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH, KOS_BASE set). It fetches nothing
and touches no file outside --out (default build/psm_ao) and --cdi-dir (default demo/psm/out/cdi, the one place the
images are kept; git ignores it). Steps, each refusing to go on when it fails:

  1 host tests  test/ao_layer_test.c (the method's maths) and test/ao0_model_test.c (the image's model), plain C;
                with --town-inc (the folders of the town's generated headers) ao_layer_test.c again with the town's sink
  2 elf         kos-cc ao0_spike.c -> <out>/psm-ao0-<tag>.elf, the library's Dreamcast flags, SH4ZAM's headers from
                --sh4zam (default third_party/sh4zam; the checkout is only read)
  3 image       mkdcdisc -> <cdi-dir>/psm-ao0-<tag>.cdi. An image that is there is never overwritten: pick a new --tag

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
    ap = argparse.ArgumentParser(description="Build the ao0 image.")
    ap.add_argument("--tag", required=True)
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "psm_ao"))
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "demo", "psm", "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--host-cc", default=os.environ.get("HOST_CC", "gcc"))
    ap.add_argument("--sh4zam", default=os.path.join(ROOT, "third_party", "sh4zam"))
    ap.add_argument("--town-inc", action="append", default=[])
    ap.add_argument("--host-only", action="store_true")
    ap.add_argument("--elf-only", action="store_true")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    image = os.path.join(os.path.abspath(a.cdi_dir), "psm-ao0-%s.cdi" % a.tag)
    elf = os.path.join(out, "psm-ao0-%s.elf" % a.tag)
    if not a.host_only and not a.elf_only and os.path.exists(image):
        raise SystemExit("refuse existing image: %s (pick a new --tag)" % image)

    exe = ".exe" if os.name == "nt" else ""
    lib_inc = ["-I" + os.path.join(ROOT, "include"), "-I" + os.path.join(ROOT, "sh4dow-dc", "include")]
    shz_inc = ["-I" + os.path.join(a.sh4zam, "include"), "-I" + os.path.join(ROOT, "third_party", "sh4zam-dce", "include")]
    host = [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DSH4DER_CORE_NO_SH4ZAM"] + lib_inc
    for name in ("ao_layer_test", "ao0_model_test"):
        test = os.path.join(out, name + exe)
        run("1 host test %s (compile)" % name, host + [os.path.join(HERE, "test", name + ".c"), "-o", test, "-lm"])
        run("1 host test %s (run)" % name, [test])
    if a.town_inc:
        test = os.path.join(out, "ao_layer_test_town" + exe)
        run("1 host test with the town's sink (compile)", host + ["-DPSM_HAVE_TOWN", "-DAO_TEST_TOWN"] + ["-I" + d for d in a.town_inc] +
            [os.path.join(HERE, "test", "ao_layer_test.c"), "-o", test, "-lm"])
        run("1 host test with the town's sink (run)", [test])
    if a.host_only:
        return 0

    kos_cc = find_kos_cc()
    if not os.environ.get("KOS_BASE") or not kos_cc:
        raise SystemExit("kos-cc is not on the PATH or KOS_BASE is not set: source the KallistiOS environment first")
    if not os.path.isfile(os.path.join(a.sh4zam, "include", "sh4zam", "shz_sh4zam.h")):
        raise SystemExit("no SH4ZAM at %s (see AGENTS.md, or give --sh4zam)" % a.sh4zam)
    lib = os.path.join(a.sh4zam, "build", "libsh4zam.a")
    run("2 elf", kos_cc + ["-O3", "-Wall", "-Wextra", "-ffast-math", "-ffp-contract=fast", "-mfsrra", "-mfsca", "-fno-math-errno"] + lib_inc + shz_inc +
        ["-DAO0_TAG=%s" % a.tag, os.path.join(HERE, "ao0_spike.c")] + ([lib] if os.path.isfile(lib) else []) + ["-o", elf])
    if a.elf_only:
        print("ELF: %s" % elf)
        return 0

    if os.path.exists(image):
        raise SystemExit("refuse existing image: %s" % image)
    os.makedirs(os.path.dirname(image), exist_ok=True)
    run("3 image", [a.mkdcdisc, "-e", elf, "-o", image, "-N", "-n", "PSM-AO0"])
    print("IMAGE: %s" % image)
    return 0


if __name__ == "__main__":
    sys.exit(main())
