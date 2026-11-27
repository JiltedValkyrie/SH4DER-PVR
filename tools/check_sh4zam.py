#!/usr/bin/env python3
"""Fail, with how to get it, when there is no usable SH4ZAM install or checkout.

SH4ZAM (the maths: https://github.com/gyrovorbis/sh4zam, MIT, Falco Girgis) is not shipped
with this repository; third_party/sh4zam is ignored by git. It is a git clone of SH4ZAM's
master, kept current with git pull: this library follows its latest. The Dreamcast targets and
the host test of the SH4ZAM path (make test-shz) need 0.10 or later, and master at or after the
commits in NEEDED (checked by name: master still says 0.10.0). The plain C path
(make test) does not need it.

usage: check_sh4zam.py [prefix] [--sw <file>]
                                        prefix: an install prefix (include/sh4zam, lib/libsh4zam.a) or a source checkout;
                                        default: $SH4ZAM, else $KOS_PORTS when SH4ZAM is installed there, else
                                        third_party/sh4zam next to this repo.
                                        --sw: SH4ZAM's software XMTRX source, required (the host tests compile it)
       check_sh4zam.py --self-test      the checks of this script, against made-up checkouts

Exit 0 when the checkout is usable, 1 otherwise (the reason and the fix on stderr).
"""
import os
import re
import subprocess
import sys
import tempfile

MIN_VERSION = (0, 10)
REPO = "https://github.com/gyrovorbis/sh4zam"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# what every build includes from an install prefix or a checkout
REQUIRED = [
    "include/sh4zam/shz_sh4zam.h",
    "include/sh4zam/shz_version.h",
]
SW = "source/sw/shz_xmtrx_sw.c"
# names the library or sh4zam-dce calls that came after the 0.10.0 tag (the version number did not move):
# (header, name, the first master commit that has it)
NEEDED = [
    ("include/sh4zam/shz_scalar.h", "shz_opaquef", "3cdff11"),
]


def fix_text(path):
    rel = path
    return (
        "SH4ZAM is not shipped with this repository. The default is SH4ZAM installed into KallistiOS's ports\n"
        "($KOS_PORTS/include/sh4zam, $KOS_PORTS/lib/libsh4zam.a; update it if it is older than %d.%d). The host tests\n"
        "also need its source/sw/shz_xmtrx_sw.c: a symlinked install finds it in its checkout, or get a checkout\n"
        "(master) into third_party:\n"
        "    git clone %s third_party/sh4zam\n"
        "and keep it current with:  git -C third_party/sh4zam pull\n"
        "(git ignores that folder; never commit it, and do not copy its files into this library).\n"
        "Or point at an install prefix or a checkout:  make ... SH4ZAM=<prefix or checkout>   (looked at: %s)"
        % (MIN_VERSION[0], MIN_VERSION[1], REPO, rel)
    )


def source_root(path):
    """The source checkout behind path: path itself, or the checkout its include/sh4zam links to (a symlinked install)."""
    real = os.path.realpath(os.path.join(path, "include", "sh4zam"))
    return os.path.dirname(os.path.dirname(real))


def default_path():
    """$SH4ZAM, else $KOS_PORTS when SH4ZAM is installed there, else third_party/sh4zam."""
    if os.environ.get("SH4ZAM"):
        return os.environ["SH4ZAM"]
    ports = os.environ.get("KOS_PORTS")
    if ports and os.path.isfile(os.path.join(ports, REQUIRED[0])):
        return ports
    return os.path.join(ROOT, "third_party", "sh4zam")


def git_commit(path):
    """The checkout's short commit, or None when it is not a git clone (or git is missing)."""
    if not os.path.exists(os.path.join(path, ".git")):
        return None
    try:
        out = subprocess.run(["git", "-C", path, "log", "-1", "--format=%h %cs"],
                             capture_output=True, text=True, timeout=20)
    except (OSError, subprocess.SubprocessError):
        return None
    if out.returncode != 0:
        return None
    return out.stdout.strip() or None


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


def check(path, sw=None):
    """None when usable, else the reason. sw: the software XMTRX source the caller compiles, required when given."""
    if not os.path.isdir(path):
        return "no SH4ZAM install or checkout at %s" % path
    missing = [r for r in REQUIRED if not os.path.isfile(os.path.join(path, r))]
    if missing:
        return "%s is not a SH4ZAM install or checkout, missing: %s" % (path, ", ".join(missing))
    if sw is not None and not os.path.isfile(sw):
        return "no SH4ZAM software XMTRX source (%s) for %s: the host tests need a source checkout" % (sw, path)
    ver = version_of(path)
    if ver is None:
        return "%s: no version in include/sh4zam/shz_version.h" % path
    if ver < MIN_VERSION:
        return "%s is SH4ZAM %d.%d, this library needs %d.%d or later" % (path, ver[0], ver[1], MIN_VERSION[0], MIN_VERSION[1])
    for rel, name, commit in NEEDED:
        try:
            with open(os.path.join(path, rel), encoding="utf-8", errors="replace") as f:
                found = re.search(r"\b%s\b" % re.escape(name), f.read())
        except OSError:
            found = None
        if not found:
            return "%s has no %s in %s: it is older than SH4ZAM master %s (pull it: git -C %s pull)" % (path, name, rel, commit, path)
    return None


def self_test():
    ok = True

    def expect(name, got_bad, want_bad):
        nonlocal ok
        if bool(got_bad) != want_bad:
            ok = False
            print("FAIL %s: %r" % (name, got_bad), file=sys.stderr)

    def make(root, minor, files=REQUIRED + [SW], names=True):
        for rel in files + [n[0] for n in NEEDED]:
            p = os.path.join(root, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "a", encoding="utf-8") as f:
                if rel.endswith("shz_version.h"):
                    f.write("#define SHZ_VERSION_MAJOR   0\n#define SHZ_VERSION_MINOR   %d\n" % minor)
                f.write("\n")
                if names:
                    f.write("".join("SHZ_INLINE float %s(float value);\n" % n[1] for n in NEEDED if n[0] == rel))

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
        make(partial, 10, REQUIRED[:1])
        expect("header missing", check(partial), True)
        ports = os.path.join(tmp, "ports")
        make(ports, 10, REQUIRED)
        expect("install prefix, headers only", check(ports), False)
        expect("install prefix, sw asked, none", check(ports, os.path.join(ports, SW)), True)
        expect("install prefix, sw from a checkout", check(ports, os.path.join(good, SW)), False)
        if hasattr(os, "symlink"):
            linked = os.path.join(tmp, "linked")
            os.makedirs(os.path.join(linked, "include"))
            os.symlink(os.path.join(good, "include", "sh4zam"), os.path.join(linked, "include", "sh4zam"))
            expect("symlinked install finds its checkout", os.path.realpath(source_root(linked)) != os.path.realpath(good), False)
        tag = os.path.join(tmp, "tag")
        make(tag, 10, names=False)
        expect("0.10.0 tag without shz_opaquef", check(tag), True)
        empty = os.path.join(tmp, "empty")
        os.makedirs(empty)
        expect("empty folder", check(empty), True)
    print("check_sh4zam self-test: %s" % ("ok" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv):
    if argv and argv[0] == "--self-test":
        return self_test()
    sw = None
    if "--sw" in argv:
        i = argv.index("--sw")
        if i + 1 >= len(argv):
            print("error: --sw needs a file", file=sys.stderr)
            return 1
        sw = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    path = argv[0] if argv else default_path()
    reason = check(path, sw)
    if reason:
        print("error: " + reason, file=sys.stderr)
        print(fix_text(path), file=sys.stderr)
        return 1
    ver = version_of(path)
    src = source_root(path)
    commit = git_commit(src)
    if commit:
        print("SH4ZAM %d.%d at %s, git %s (update: git -C %s pull)" % (ver[0], ver[1], path, commit, src))
    elif not os.path.isdir(os.path.join(path, "source")):
        print("SH4ZAM %d.%d installed at %s" % (ver[0], ver[1], path))
    else:
        print("SH4ZAM %d.%d at %s (not a git clone: it cannot follow SH4ZAM's latest; re-clone with git clone %s)"
              % (ver[0], ver[1], path, REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
