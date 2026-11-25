#!/usr/bin/env python3
"""Fail, with how to get it, when there is no usable SH4ZAM checkout.

SH4ZAM (the maths: https://github.com/gyrovorbis/sh4zam, MIT, Falco Girgis) is not shipped
with this repository; third_party/sh4zam is ignored by git. The Dreamcast targets and the
host test of the SH4ZAM path (make test-shz) need 0.10 or later. The plain C path (make test)
does not need it.

usage: check_sh4zam.py [checkout]       (default: $SH4ZAM, else third_party/sh4zam next to this repo)
       check_sh4zam.py --self-test      the checks of this script, against made-up checkouts

Exit 0 when the checkout is usable, 1 otherwise (the reason and the fix on stderr).
"""
import os
import re
import sys
import tempfile

MIN_VERSION = (0, 10)
REPO = "https://github.com/gyrovorbis/sh4zam"
TAG = "v0.10.0"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# what the Makefile compiles or includes from a checkout
REQUIRED = [
    "include/sh4zam/shz_sh4zam.h",
    "include/sh4zam/shz_version.h",
    "source/sw/shz_xmtrx_sw.c",
]


def fix_text(path):
    rel = path
    return (
        "SH4ZAM is not shipped with this repository. Get it (version %d.%d or later) into third_party:\n"
        "    git clone --branch %s %s third_party/sh4zam\n"
        "(git ignores that folder; never commit it, and do not copy its files into this library).\n"
        "Or point at a checkout you already have:  make ... SH4ZAM=<checkout>   (looked at: %s)"
        % (MIN_VERSION[0], MIN_VERSION[1], TAG, REPO, rel)
    )


def version_of(path):
    """(major, minor) from shz_version.h, or None."""
    try:
        with open(os.path.join(path, "include/sh4zam/shz_version.h"), encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError:
        return None
    major = re.search(r"#define\s+SHZ_VERSION_MAJOR\s+(\d+)", text)
    minor = re.search(r"#define\s+SHZ_VERSION_MINOR\s+(\d+)", text)
    if not major or not minor:
        return None
    return int(major.group(1)), int(minor.group(1))


def check(path):
    """None when usable, else the reason."""
    if not os.path.isdir(path):
        return "no SH4ZAM checkout at %s" % path
    missing = [r for r in REQUIRED if not os.path.isfile(os.path.join(path, r))]
    if missing:
        return "%s is not a SH4ZAM checkout, missing: %s" % (path, ", ".join(missing))
    ver = version_of(path)
    if ver is None:
        return "%s: no version in include/sh4zam/shz_version.h" % path
    if ver < MIN_VERSION:
        return "%s is SH4ZAM %d.%d, this library needs %d.%d or later" % (path, ver[0], ver[1], MIN_VERSION[0], MIN_VERSION[1])
    return None


def self_test():
    ok = True

    def expect(name, got_bad, want_bad):
        nonlocal ok
        if bool(got_bad) != want_bad:
            ok = False
            print("FAIL %s: %r" % (name, got_bad), file=sys.stderr)

    def make(root, minor, files=REQUIRED):
        for rel in files:
            p = os.path.join(root, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "w", encoding="utf-8") as f:
                if rel.endswith("shz_version.h"):
                    f.write("#define SHZ_VERSION_MAJOR   0\n#define SHZ_VERSION_MINOR   %d\n" % minor)
                else:
                    f.write("\n")

    with tempfile.TemporaryDirectory() as tmp:
        expect("no folder", check(os.path.join(tmp, "nothing")), True)
        good = os.path.join(tmp, "good")
        make(good, 10)
        expect("0.10 present", check(good), False)
        newer = os.path.join(tmp, "newer")
        make(newer, 11)
        expect("0.11 present", check(newer), False)
        old = os.path.join(tmp, "old")
        make(old, 9)
        expect("0.9 too old", check(old), True)
        partial = os.path.join(tmp, "partial")
        make(partial, 10, REQUIRED[:2])
        expect("source file missing", check(partial), True)
        empty = os.path.join(tmp, "empty")
        os.makedirs(empty)
        expect("empty folder", check(empty), True)
    print("check_sh4zam self-test: %s" % ("ok" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv):
    if argv and argv[0] == "--self-test":
        return self_test()
    path = argv[0] if argv else os.environ.get("SH4ZAM") or os.path.join(ROOT, "third_party", "sh4zam")
    reason = check(path)
    if reason:
        print("error: " + reason, file=sys.stderr)
        print(fix_text(path), file=sys.stderr)
        return 1
    ver = version_of(path)
    print("SH4ZAM %d.%d at %s" % (ver[0], ver[1], path))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
