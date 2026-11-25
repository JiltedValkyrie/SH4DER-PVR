#!/usr/bin/env python3
"""SH4DER-PVR: the examples disc, SH4DER-PVR-Examples.cdi: every approved demo on ONE disc, with the suite's menu.
Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

A new demo or test is first a small disc of its own (one ELF, its own name in demo/psm/out/cdi). When the owner
approves it, it is added to the list and to this disc. The developers' test discs (psm-suite-<tag>) are another thing
and are not made here.

    python demo/demos_disc.py [--list demo/demos_approved.json] [--tag <tag>] [--cdi-dir <dir>] [--mkdcdisc <exe>]
                              [--check] [--no-build] [--no-image] [--self-test]

The list (JSON, {"demos": [...]}) names the demos the owner has approved, and nothing else goes on the disc:
  name     up to 10 characters, shown in the menu       file   the name on the disc, 8.3, upper case, .BIN
  elf      the demo's ELF, a path from the repository's root or absolute
  order    its place in RUN ALL, 1 .. n with no gap; 0 leaves it out of RUN ALL
  what     one line, up to 49 characters                read   one line, up to 59 characters
  build    optional: the command that makes the ELF, as a list of words, run from the repository's root
           ("python" as the first word is this Python). The same command a demo's own make target runs.
  files    optional: files the demo reads from /cd, paths from the root; each goes on the disc under its own name
  credits  optional: a text file with the demo's credits; all of them are gathered into CREDITS.TXT on the disc
  note     optional: a remark for people; not used

What it does, each step refusing to go on when it fails:
  1 the list     read and checked (lengths, 8.3 names, the order, no name used twice on the disc, no private file)
  2 the tools    KOS_BASE is set, kos-cc and sh-elf-objcopy are on the PATH, mkdcdisc is found: each missing one is
                 named in a line of its own. (SH4ZAM is the one KallistiOS carries: nothing is cloned.)
  3 the builds   each entry's "build" command
  4 the disc     demo/psm/suite/build.py does the rest with this list: the menu, each ELF's checks (an SH-4 ELF at
                 0x8c010000, its end below the stack's room, one mail mark: it comes back to the menu), the manifest
                 with every file's sha256, and the image in demo/psm/out/cdi, never overwritten.

The image is SH4DER-PVR-Examples.cdi when that name is free. When it exists it is NOT overwritten: the image is
SH4DER-PVR-Examples-b<N>.cdi with the first N from 2 that is free, and the script says so. The tag (the menu's title
line and the build folder) is b<N> to match, b1 for the first; --tag gives another.
--check stops after step 1 (no tool is needed). --no-build skips step 3. --no-image stops before the image.
--self-test checks the list's rules on made-up lists and prints what it refused.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
SUITE_BUILD = os.path.join(ROOT, "demo", "psm", "suite", "build.py")
KEYS = {"name", "file", "elf", "order", "what", "read", "build", "files", "credits", "screen_credits", "note"}
NEEDED = ("name", "file", "elf", "order", "what", "read")
PRIVATE = {"TWTEX.BIN", "HERO.BIN"}         # files that are never on this disc
RESERVED = {"MENU.BIN", "CREDITS.TXT", "1ST_READ.BIN", "IP.BIN"}
NAME_83 = re.compile(r"^[A-Z0-9_]{1,8}\.[A-Z0-9]{1,3}$")


class ListError(Exception):
    pass


def check_list(doc, root=ROOT, need_files=True):
    """The list's entries, checked. Raises ListError with one line saying what is wrong."""
    if not isinstance(doc, dict) or not isinstance(doc.get("demos"), list) or not doc["demos"]:
        raise ListError('the list must be {"demos": [...]} with at least one demo')
    on_disc, names, orders, out = set(RESERVED), set(), [], []
    for k, e in enumerate(doc["demos"]):
        if not isinstance(e, dict):
            raise ListError("entry %d is not an object" % (k + 1))
        who = str(e.get("name", "entry %d" % (k + 1)))
        for key in e:
            if key not in KEYS:
                raise ListError("%s: unknown key %r" % (who, key))
        for key in NEEDED:
            if key not in e:
                raise ListError("%s: no %r" % (who, key))
        for key in ("name", "file", "elf", "what", "read"):
            if not isinstance(e[key], str) or not e[key]:
                raise ListError("%s: %r must be text" % (who, key))
        if len(e["name"]) > 10:
            raise ListError("%s: name over 10 characters" % who)
        if len(e["what"]) > 49:
            raise ListError("%s: what over 49 characters (%d)" % (who, len(e["what"])))
        if len(e["read"]) > 59:
            raise ListError("%s: read over 59 characters (%d)" % (who, len(e["read"])))
        if e["name"] in names:
            raise ListError("%s: the name is used twice" % who)
        names.add(e["name"])
        if not NAME_83.match(e["file"]) or not e["file"].endswith(".BIN"):
            raise ListError("%s: the disc name %r must be 8.3 upper case .BIN" % (who, e["file"]))
        if not isinstance(e["order"], int) or isinstance(e["order"], bool) or e["order"] < 0:
            raise ListError("%s: order must be a whole number, 0 or more" % who)
        if e["order"]:
            if e["order"] in orders:
                raise ListError("%s: order %d is used twice" % (who, e["order"]))
            orders.append(e["order"])
        if "build" in e and (not isinstance(e["build"], list) or not e["build"] or not all(isinstance(w, str) and w for w in e["build"])):
            raise ListError("%s: build must be a list of words" % who)
        files = e.get("files", [])
        if not isinstance(files, list) or not all(isinstance(f, str) and f for f in files):
            raise ListError("%s: files must be a list of paths" % who)
        for f in [e["file"]] + [os.path.basename(f).upper() for f in files]:
            if f in PRIVATE:
                raise ListError("%s: %s is a private file and is never put on this disc" % (who, f))
            if f in on_disc:
                raise ListError("%s: the disc name %s is used twice (or is the disc's own)" % (who, f))
            on_disc.add(f)
        for f in files:
            if not NAME_83.match(os.path.basename(f).upper()):
                raise ListError("%s: %s is not an 8.3 name" % (who, os.path.basename(f)))
        if "credits" in e and (not isinstance(e["credits"], str) or not e["credits"]):
            raise ListError("%s: credits must be a path" % who)
        if need_files:
            for f in files + ([e["credits"]] if "credits" in e else []):
                if not os.path.isfile(f if os.path.isabs(f) else os.path.join(root, f)):
                    raise ListError("%s: no file at %s" % (who, f))
        out.append(e)
    if sorted(orders) != list(range(1, len(orders) + 1)):
        raise ListError("RUN ALL's order must be 1 .. n with no gap: %r" % sorted(orders))
    return out


def check_elfs(entries, root=ROOT):
    for e in entries:
        p = e["elf"] if os.path.isabs(e["elf"]) else os.path.join(root, e["elf"])
        if not os.path.isfile(p):
            raise ListError("%s: no ELF at %s%s" % (e["name"], e["elf"], "" if "build" in e else " (and the entry has no build command)"))


def find_tool(name):
    for d in os.environ.get("PATH", "").split(os.pathsep):
        for n in (name, name + ".exe"):
            if d and os.path.isfile(os.path.join(d, n)):
                return os.path.join(d, n)
    return None


def missing_tools(mkdcdisc):
    """One line for each thing a disc build needs and does not find."""
    miss = []
    if not os.environ.get("KOS_BASE"):
        miss.append("KOS_BASE is not set: source KallistiOS's environ.sh in this shell first")
    if not find_tool("kos-cc") and not os.path.isfile(os.path.join(os.environ.get("KOS_BASE", ""), "utils", "build_wrappers", "kos-cc")):
        miss.append("kos-cc is not on the PATH: it comes with the KallistiOS environment")
    if not find_tool("sh-elf-objcopy"):
        miss.append("sh-elf-objcopy is not on the PATH: the SH-4 toolchain's bin folder is missing from it")
    if not (os.path.isfile(mkdcdisc) or find_tool(mkdcdisc)):
        miss.append("mkdcdisc is not found (%s): give --mkdcdisc <exe> or set MKDCDISC" % mkdcdisc)
    return miss


PREFIX = "SH4DER-PVR-Examples"
TITLE = "SH4DER-PVR EXAMPLES"


def free_image(cdi_dir, prefix=PREFIX):
    """(the image's file name, its build number): <prefix>.cdi when free, else <prefix>-b<N>.cdi, the first free N from 2."""
    if not os.path.exists(os.path.join(cdi_dir, prefix + ".cdi")):
        return prefix + ".cdi", 1
    n = 2
    while os.path.exists(os.path.join(cdi_dir, "%s-b%d.cdi" % (prefix, n))):
        n += 1
    return "%s-b%d.cdi" % (prefix, n), n


def self_test():
    good = {"name": "ao4", "file": "AO4.BIN", "elf": "a.elf", "order": 1, "what": "w", "read": "r"}

    def refused(doc, root=ROOT, need_files=True, elfs=False):
        try:
            got = check_list(doc, root, need_files)
            if elfs:
                check_elfs(got, root)
        except ListError as err:
            return str(err)
        return None

    def with_(**kw):
        e = dict(good); e.update(kw)
        return {"demos": [e]}

    cases = [
        ("an empty list", {"demos": []}),
        ("not a list", {"tests": [good]}),
        ("a name over 10", with_(name="elevenchars")),
        ("what over 49", with_(what="x" * 50)),
        ("read over 59", with_(read="x" * 60)),
        ("a disc name not 8.3", with_(file="TOOLONGNAME.BIN")),
        ("a disc name in lower case", with_(file="ao4.bin")),
        ("a disc name that is not .BIN", with_(file="AO4.ELF")),
        ("a missing key", {"demos": [{k: v for k, v in good.items() if k != "read"}]}),
        ("an unknown key", with_(colour="red")),
        ("an order that is not a number", with_(order="1")),
        ("a gap in the order", with_(order=2)),
        ("an order used twice", {"demos": [good, dict(good, name="b", file="B.BIN")]}),
        ("a name used twice", {"demos": [good, dict(good, file="B.BIN", order=2)]}),
        ("a disc name used twice", {"demos": [good, dict(good, name="b", order=2)]}),
        ("the menu's own name", with_(file="MENU.BIN")),
        ("a private file", with_(files=["stage/TWTEX.BIN"])),
        ("a file that clashes with a demo", {"demos": [good, dict(good, name="b", file="B.BIN", order=2, files=["x/ao4.bin"])]}),
        ("a build that is not a list", with_(build="make ao-demo")),
    ]
    fails = 0
    for what, doc in cases:
        why = refused(doc, need_files=False)
        print("  %-34s %s" % (what + ":", why if why else "TAKEN, and it must not be"))
        fails += why is None
    with tempfile.TemporaryDirectory() as d:
        why = refused(with_(), d, True, True)
        print("  %-34s %s" % ("a missing ELF:", why if why else "TAKEN, and it must not be")); fails += why is None
        why = refused(with_(credits="CREDITS.md"), d)
        print("  %-34s %s" % ("a missing credits file:", why if why else "TAKEN, and it must not be")); fails += why is None
        open(os.path.join(d, "a.elf"), "wb").close(); open(os.path.join(d, "FONT.BIN"), "wb").close(); open(os.path.join(d, "c.md"), "w").close()
        ok = {"demos": [dict(good, files=["FONT.BIN"], credits="c.md", build=["python", "x.py"], note="n"), dict(good, name="b", file="B.BIN", order=0)]}
        why = refused(ok, d, True, True)
        print("  %-34s %s" % ("a good list of two:", "taken" if why is None else "REFUSED: " + why)); fails += why is not None
        got = free_image(d)
        print("  %-34s %s" % ("the image, none there yet:", got[0])); fails += got != ("SH4DER-PVR-Examples.cdi", 1)
        open(os.path.join(d, "SH4DER-PVR-Examples.cdi"), "wb").close()
        got = free_image(d)
        print("  %-34s %s" % ("the image, the name taken:", got[0])); fails += got != ("SH4DER-PVR-Examples-b2.cdi", 2)
        open(os.path.join(d, "SH4DER-PVR-Examples-b2.cdi"), "wb").close()
        got = free_image(d)
        print("  %-34s %s" % ("the image, b2 taken too:", got[0])); fails += got != ("SH4DER-PVR-Examples-b3.cdi", 3)
        with open(os.path.join(d, "SH4DER-PVR-Examples.cdi"), "rb") as f:
            fails += f.read() != b""            # the taken names were not written to
    with open(os.path.join(HERE, "demos_approved.json"), encoding="utf-8") as f:
        why = refused(json.load(f), need_files=True)
    print("  %-34s %s" % ("demo/demos_approved.json:", "taken" if why is None else "REFUSED: " + why)); fails += why is not None
    print("demos_disc self-test: %s" % ("%d FAILED" % fails if fails else "all passed"))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description="Every approved demo on one disc, with the suite's menu.")
    ap.add_argument("--list", default=os.path.join(HERE, "demos_approved.json"))
    ap.add_argument("--tag")
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "demo", "psm", "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-image", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()

    try:
        with open(a.list, encoding="utf-8") as f:
            entries = check_list(json.load(f))
    except (OSError, ValueError) as err:
        raise SystemExit("demos disc: cannot read the list %s: %s" % (a.list, err))
    except ListError as err:
        raise SystemExit("demos disc: %s" % err)
    print("the list: %d demo(s): %s" % (len(entries), ", ".join(e["name"] for e in entries)))
    if a.check:
        return 0

    miss = missing_tools(a.mkdcdisc) if not a.no_image else [m for m in missing_tools(a.mkdcdisc) if not m.startswith("mkdcdisc")]
    if miss:
        raise SystemExit("demos disc: cannot build here:\n  " + "\n  ".join(miss))
    with open(SUITE_BUILD, encoding="utf-8") as f:
        if "--prefix" not in f.read():
            raise SystemExit("demos disc: demo/psm/suite/build.py has no --prefix and --title: nothing built")
        f.seek(0)
        if "--image" not in f.read():
            raise SystemExit("demos disc: demo/psm/suite/build.py has no --image: nothing built")

    if not a.no_build:
        for e in entries:
            if "build" in e:
                argv = [sys.executable if e["build"][0] == "python" else e["build"][0]] + e["build"][1:]
                print("== build %s: %s" % (e["name"], " ".join(argv)), flush=True)
                if subprocess.run(argv, cwd=ROOT).returncode:
                    raise SystemExit("demos disc: the build of %s failed" % e["name"])
    try:
        check_elfs(entries)
    except ListError as err:
        raise SystemExit("demos disc: %s" % err)

    cdi_dir = os.path.abspath(a.cdi_dir)
    image, number = free_image(cdi_dir)
    tag = a.tag or "b%d" % number
    if number > 1:
        print("%s.cdi exists and is not overwritten: this image is %s" % (PREFIX, image))
    out = os.path.join(ROOT, "build", "demos_disc", tag)
    os.makedirs(out, exist_ok=True)
    table = os.path.join(out, "demos_list.json")
    with open(table, "w", encoding="utf-8", newline="\n") as f:
        # the suite's build reads "tests"; every entry here is a TECH DEMO of its menu. This list's "credits" is a
        # file's path (checked above); the suite's own "credits" key (what its CREDITS screen must name) is "screen_credits" here.
        tests = [dict({k: e[k] for k in ("name", "file", "elf", "order", "what", "read")}, group="demos", **({"credits": e["screen_credits"]} if "screen_credits" in e else {}))
                 for e in entries]
        json.dump({"note": "written by demo/demos_disc.py from %s" % os.path.relpath(a.list, ROOT), "tests": tests}, f, indent=1)
    argv = [sys.executable, SUITE_BUILD, "--tag", tag, "--list", table, "--prefix", PREFIX, "--title", TITLE, "--image", image, "--out", out,
            "--cdi-dir", cdi_dir, "--mkdcdisc", a.mkdcdisc] + (["--no-image"] if a.no_image else [])
    print("== disc: %s" % " ".join(argv), flush=True)
    return subprocess.run(argv, cwd=ROOT).returncode


if __name__ == "__main__":
    sys.exit(main())
