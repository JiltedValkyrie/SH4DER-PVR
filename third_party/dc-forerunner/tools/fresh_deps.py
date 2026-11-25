#!/usr/bin/env python3
"""Fetch what a fresh clone lacks, pinned. Model: the Halo port's tools/fresh_deps.py.

  python tools/fresh_deps.py --ensure     clone each missing dependency at its pin into third_party/, then check
  python tools/fresh_deps.py --check      check only, no network

An existing folder is never touched: a stale one is named, not replaced. KallistiOS and mkdcdisc stay external (config KOS_BASE, MKDCDISC;
their pins are in README.md). The folders are git-ignored.
"""
import pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
# name: (folder, url, pin, file that proves the clone). A pin starting with "v" is a tag, else a commit.
REPOS = {
    "SH4ZAM": ("third_party/sh4zam", "https://github.com/gyrovorbis/sh4zam.git", "v0.10.0", "include/sh4zam/shz_sh4zam.h"),
    "SH4DER-PVR": ("third_party/sh4der-pvr", "https://github.com/JiltedValkyrie/SH4DER-PVR", "76d12c9c5d082c042157f3dca1d8ef621f2464bc", "include/sh4der_core.h"),
    "SH4DOW-DC": ("third_party/sh4dow-dc", "https://github.com/JiltedValkyrie/SH4DOW-DC", "bed4aa36e66cb4ec83d267fd0ccb0b24ec245e60", "include/sh4dow_dc.h"),
}

def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True)

def sh4zam_problem(root):
    """include/sh4zam/shz_version.h says the pin and every source tools/sh4zam_lib.mk compiles is there."""
    try:
        text = (root / "include" / "sh4zam" / "shz_version.h").read_text(encoding="utf-8", errors="replace")
        have = ".".join(re.search(r"#define SHZ_VERSION_%s\s+(\d+)" % k, text).group(1) for k in ("MAJOR", "MINOR", "PATCH"))
    except (OSError, AttributeError):
        return "has no readable include/sh4zam/shz_version.h"
    if "v" + have != REPOS["SH4ZAM"][2]:
        return "is SH4ZAM v%s, the build compiles %s" % (have, REPOS["SH4ZAM"][2])
    mk = (ROOT / "tools" / "sh4zam_lib.mk").read_text(encoding="utf-8")
    missing = [m for m in re.findall(r"\$\(SHZ_DIR\)/(source/[^\s%]+\.[cs])", mk) if not (root / m).is_file()]
    return "lacks %s (tools/sh4zam_lib.mk compiles it)" % missing[0] if missing else ""

def problem(name):
    folder, _, pin, proof = REPOS[name]
    root = ROOT / folder
    if not (root / proof).exists():
        return "is missing"
    if name == "SH4ZAM":
        return sh4zam_problem(root)
    head = git("-c", "safe.directory=*", "-C", str(root), "rev-parse", "HEAD").stdout.strip()
    return "" if head == pin else "is at %s, the pin is %s" % (head[:8] or "no git HEAD", pin[:8])

def clone(name):
    folder, url, pin, _ = REPOS[name]
    dest = str(ROOT / folder)
    if pin.startswith("v"):
        r = git("clone", "--branch", pin, "--depth", "1", url, dest)
    else:
        r = git("clone", url, dest)
        if not r.returncode:
            r = git("-c", "advice.detachedHead=false", "-C", dest, "checkout", pin)
    return "" if not r.returncode else "clone failed: " + (r.stderr.strip().splitlines() or ["exit %d" % r.returncode])[-1]

if __name__ == "__main__":
    if not ({"--ensure", "--check"} & set(sys.argv)):
        sys.exit(__doc__)
    bad = []
    for name, (folder, _, pin, _) in REPOS.items():
        if problem(name) == "is missing" and "--ensure" in sys.argv:
            print("cloning %s at %s" % (name, pin[:8]))
            why = clone(name) or problem(name)
        else:
            why = problem(name)
        print("%s %s" % (name, why and "%s %s" % (folder, why) or "ok"))
        if why:
            bad.append(name)
    sys.exit(1 if bad else 0)
