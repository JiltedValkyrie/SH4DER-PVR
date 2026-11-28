#!/usr/bin/env python3
"""SH4DER-PVR: render-done wait test (sleeping against polled), build script.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/waitpoll/build.py --tag <new tag> [--out <dir>] [--cdi-dir <dir>] [--mkdcdisc <exe>] [--sh4zam <dir>] [--host-only] [--elf-only]

Run it in a shell where the KallistiOS environment is sourced (kos-cc on the PATH, KOS_BASE set). It fetches nothing and
touches no file outside --out (default build/psm_waitpoll) and --cdi-dir (default out/cdi at the top of the repository,
the one place the images are kept). Steps, each refusing to go on when it fails:

  1 host tests  test/pvrx_test.c and test/page_test.c with the host compiler
  2 rtt         the KallistiOS under KOS_BASE must declare pvr_scene_begin_rtt
  3 elf         kos-cc waitpoll_spike.c + src/sh4der_pl4nar.c + src/sh4der_pvrx.c -> <out>/psm-waitpoll-<tag>.elf.
                An ELF that is already there is never overwritten: pick a new --tag
  4 image       mkdcdisc -> <cdi-dir>/psm-waitpoll-<tag>.cdi, and the build note <cdi-dir>/psm-waitpoll-<tag>.txt beside it.
                An image or a note that is there is never overwritten: pick a new --tag

The build note records the library's HEAD, a sha256 over `git diff HEAD` and every untracked file (the working tree the
image was built from: the library is not committed while a test is judged), the files that differ, and the sizes.
--host-only stops after step 1. --elf-only stops after step 3 (no mkdcdisc needed).
"""
import argparse
import hashlib
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


def git(*args):
    r = subprocess.run(["git"] + list(args), cwd=ROOT, stdout=subprocess.PIPE, check=True)
    return r.stdout


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


def main():
    ap = argparse.ArgumentParser(description="Build the wait poll test image.")
    ap.add_argument("--tag", required=True)
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "psm_waitpoll"))
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--host-cc", default=os.environ.get("HOST_CC", "gcc"))
    ap.add_argument("--sh4zam", default=os.path.join(ROOT, "third_party", "sh4zam"))
    ap.add_argument("--host-only", action="store_true")
    ap.add_argument("--elf-only", action="store_true")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    image = os.path.join(os.path.abspath(a.cdi_dir), "psm-waitpoll-%s.cdi" % a.tag)
    note = os.path.join(os.path.abspath(a.cdi_dir), "psm-waitpoll-%s.txt" % a.tag)
    elf = os.path.join(out, "psm-waitpoll-%s.elf" % a.tag)
    if not a.host_only and os.path.exists(elf):
        raise SystemExit("refuse existing ELF: %s (pick a new --tag)" % elf)
    if not a.host_only and not a.elf_only and (os.path.exists(image) or os.path.exists(note)):
        raise SystemExit("refuse existing image or note: %s (pick a new --tag)" % image)

    exe = ".exe" if os.name == "nt" else ""
    host = [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DSH4DER_CORE_NO_SH4ZAM", "-I" + os.path.join(ROOT, "include")]
    for name in ("pvrx_test", "page_test"):
        test = os.path.join(out, name + exe)
        run("1 host test %s (compile)" % name, host + [os.path.join(ROOT, "test", name + ".c"), "-o", test, "-lm"])
        run("1 host test %s (run)" % name, [test])
    if a.host_only:
        return 0

    kos_base = os.environ.get("KOS_BASE", "")
    kos_cc = find_kos_cc()
    if not kos_base or not kos_cc:
        raise SystemExit("kos-cc is not on the PATH or KOS_BASE is not set: source the KallistiOS environment first")
    if not has_rtt(kos_base):
        raise SystemExit("this KallistiOS (%s) does not declare pvr_scene_begin_rtt: the test needs it, stopping" % kos_base)
    print("== 2 rtt: pvr_scene_begin_rtt is declared in %s" % kos_base)
    if not os.path.isfile(os.path.join(a.sh4zam, "include", "sh4zam", "shz_sh4zam.h")):
        raise SystemExit("no SH4ZAM at %s (see AGENTS.md, or give --sh4zam)" % a.sh4zam)

    if os.path.exists(elf):
        raise SystemExit("refuse existing ELF: %s (pick a new --tag)" % elf)
    head, branch, tree, files = tree_state()
    lib = os.path.join(a.sh4zam, "build", "libsh4zam.a")
    inc = ["-I" + os.path.join(ROOT, "include"), "-I" + os.path.join(a.sh4zam, "include"), "-I" + os.path.join(ROOT, "third_party", "sh4zam-dce", "include")]
    run("3 elf", kos_cc + ["-O3", "-Wall", "-Wextra", "-ffast-math", "-ffp-contract=fast", "-mfsrra", "-mfsca", "-fno-math-errno"] + inc +
        ["-DWP_TAG=" + a.tag, "-DWP_LIB=" + head[:12], "-DWP_DIFF=" + tree[:12], "-DSH4DER_PL4NAR_HAVE_RTT"] +
        [os.path.join(HERE, "waitpoll_spike.c"), os.path.join(ROOT, "src", "sh4der_pl4nar.c"), os.path.join(ROOT, "src", "sh4der_pvrx.c")] +
        ([lib] if os.path.isfile(lib) else []) + ["-o", elf])
    if a.elf_only:
        print("ELF: %s" % elf)
        return 0

    if os.path.exists(image) or os.path.exists(note):
        raise SystemExit("refuse existing image or note: %s" % image)
    os.makedirs(os.path.dirname(image), exist_ok=True)
    run("4 image", [a.mkdcdisc, "-e", elf, "-o", image, "-N", "-n", "PSM-WAITPOLL"])
    with open(elf, "rb") as f:
        elf_sha = hashlib.sha256(f.read()).hexdigest()
    with open(note, "x", encoding="utf-8", newline="\n") as f:
        f.write("wait poll test, build %s\nimage: %s\nelf sha256: %s\n" % (a.tag, os.path.basename(image), elf_sha))
        f.write("library: branch %s, HEAD %s\nworking tree sha256 (git diff HEAD + untracked files): %s\n" % (branch, head, tree))
        f.write("files that differ from HEAD (%d):\n" % len(files))
        for p in files:
            f.write("  %s\n" % p)
        f.write("KOS_BASE: %s\nSH4ZAM: %s\n" % (kos_base, a.sh4zam))
    print("IMAGE: %s" % image)
    print("NOTE:  %s" % note)
    return 0


if __name__ == "__main__":
    sys.exit(main())
