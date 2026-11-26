#!/usr/bin/env python3
"""SH4DER-PVR: the PVR-side compare spikes (cmp, vol, sub, c4, r3), build script.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/compare/build.py [--spike cmp|vol|sub|c4|r3] [--tag <tag>] [--out <dir>] [--cdi-dir <dir>] [--mkdcdisc <exe>] [--host-cc <cc>]
                                     [--sh4zam <checkout>] [--host-only] [--elf-only]

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH, KOS_BASE set). It fetches nothing
and touches no file outside --out (default build/psm_cmp) and --cdi-dir (default demo/psm/out/cdi, the one place
the images are kept; git ignores it). Steps, each refusing to go on when it fails:

  1 host test  test/<spike>_model_test.c with the host compiler: the model (a model, not a measurement)
  2 elf        kos-cc <spike>_spike.c -> <out>/psm-<spike>-<tag>.elf
  3 image      mkdcdisc -> <cdi-dir>/psm-<spike>-<tag>.cdi. An image that is there is never overwritten: pick a new --tag

--spike cmp (default tag c1) is the compare by blending; --spike vol (default tag c2) is modifier volumes at scale;
--spike sub (default tag c3) is the bump unit as a subtract, the frame buffer write threshold, punch-through and fog.
--spike c4 (default tag c4) is the whole chain: a map the PVR draws, the bump unit as the compare, the mask, the shadow
layer. It is written on the library's maths switch (include/sh4der_core.h) and on SH4ZAM-DCE: its host test is built
twice (plain C, and on SH4ZAM's software back end), and its program with SH4ZAM (--sh4zam, default third_party/sh4zam;
the checkout is only read) and the flags of the library's Dreamcast build.
--spike r3 (default tag a) is the R3 page, the punch-through product (../recipes/R3_PAGE_SPEC.md): its host test is
built as plain C with the library's include paths, its program as c4's; the tag is printed on its first page.

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
    ap = argparse.ArgumentParser(description="Build the PVR-side compare spike.")
    ap.add_argument("--spike", choices=("cmp", "vol", "sub", "c4", "r3"), default="cmp")
    ap.add_argument("--tag", default=None)
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "psm_cmp"))
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "demo", "psm", "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--host-cc", default=os.environ.get("HOST_CC", "gcc"))
    ap.add_argument("--sh4zam", default=os.path.join(ROOT, "third_party", "sh4zam"))
    ap.add_argument("--host-only", action="store_true")
    ap.add_argument("--elf-only", action="store_true")
    a = ap.parse_args()
    tag = a.tag or {"cmp": "c1", "vol": "c2", "sub": "c3", "c4": "c4", "r3": "a"}[a.spike]

    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    image = os.path.join(os.path.abspath(a.cdi_dir), "psm-%s-%s.cdi" % (a.spike, tag))
    elf = os.path.join(out, "psm-%s-%s.elf" % (a.spike, tag))
    if not a.host_only and not a.elf_only and os.path.exists(image):
        raise SystemExit("refuse existing image: %s (pick a new --tag)" % image)

    exe = ".exe" if os.name == "nt" else ""
    src = os.path.join(HERE, "test", "%s_model_test.c" % a.spike)
    test = os.path.join(out, "%s_model_test%s" % (a.spike, exe))
    lib_inc = ["-I" + os.path.join(ROOT, "include")]
    shz_inc = ["-I" + os.path.join(a.sh4zam, "include"), "-I" + os.path.join(ROOT, "third_party", "sh4zam-dce", "include")]
    if a.spike == "c4":
        if not os.path.isfile(os.path.join(a.sh4zam, "include", "sh4zam", "shz_sh4zam.h")):
            raise SystemExit("no SH4ZAM at %s (see AGENTS.md for the clone line, or give --sh4zam)" % a.sh4zam)
        run("1 host test, plain C (compile)", [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DSH4DER_CORE_NO_SH4ZAM"] + lib_inc + [src, "-o", test, "-lm"])
        run("1 host test, plain C (run)", [test])
        test = os.path.join(out, "%s_model_test_shz%s" % (a.spike, exe))
        run("1 host test, SH4ZAM software back end (compile)", [a.host_cc, "-O2", "-std=gnu11", "-Wall", "-Wextra", "-Werror"] + lib_inc + shz_inc +
            [src, os.path.join(a.sh4zam, "source", "sw", "shz_xmtrx_sw.c"), "-o", test, "-lm"])
        run("1 host test, SH4ZAM software back end (run)", [test])
    elif a.spike == "r3":
        run("1 host test, plain C (compile)", [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DSH4DER_CORE_NO_SH4ZAM"] + lib_inc + [src, "-o", test, "-lm"])
        run("1 host test, plain C (run)", [test])
    else:
        run("1 host test (compile)", [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", src, "-o", test, "-lm"])
        run("1 host test (run)", [test])
    if a.host_only:
        return 0

    kos_cc = find_kos_cc()
    if not os.environ.get("KOS_BASE") or not kos_cc:
        raise SystemExit("kos-cc is not on the PATH or KOS_BASE is not set: source the KallistiOS environment first")
    if a.spike in ("c4", "r3"):            # the library's Dreamcast flags; SH4ZAM's compiled half only if the checkout has built it
        lib = os.path.join(a.sh4zam, "build", "libsh4zam.a")
        run("2 elf", kos_cc + ["-O3", "-Wall", "-Wextra", "-ffast-math", "-ffp-contract=fast", "-mfsrra", "-mfsca", "-fno-math-errno"] + lib_inc + shz_inc +
            (["-DR3_TAG=%s" % tag] if a.spike == "r3" else []) + [os.path.join(HERE, "%s_spike.c" % a.spike)] + ([lib] if os.path.isfile(lib) else []) + ["-o", elf])
    else:
        run("2 elf", kos_cc + ["-O2", "-Wall", "-Wextra", os.path.join(HERE, "%s_spike.c" % a.spike), "-o", elf])
    if a.elf_only:
        print("ELF: %s" % elf)
        return 0

    if os.path.exists(image):
        raise SystemExit("refuse existing image: %s" % image)
    os.makedirs(os.path.dirname(image), exist_ok=True)
    run("3 image", [a.mkdcdisc, "-e", elf, "-o", image, "-N", "-n", "PSM-%s" % a.spike.upper()])
    print("IMAGE: %s" % image)
    return 0


if __name__ == "__main__":
    sys.exit(main())
