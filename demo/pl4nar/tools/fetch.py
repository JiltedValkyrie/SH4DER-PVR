#!/usr/bin/env python3
"""Fetch the demo's third-party art (fetch_table.json) into assets/: download, check sha256, unpack the listed files.

    python tools/fetch.py            every pack; a pack that is already unpacked is left alone
    python tools/fetch.py --pin      also accept packs with no sha256 yet and print the hash to put in the table
    python tools/fetch.py --check    no network: say which packs are present, pinned, missing

Nothing fetched is committed (assets/ is ignored). A file in the pack is found by its base name, whatever folder the
pack keeps it in. A pack whose url is null is downloaded by hand from its page into assets/_dl/<file>.
Standard library only. Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.
"""
import argparse
import hashlib
import json
import os
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")
DL = os.path.join(ASSETS, "_dl")


def table():
    with open(os.path.join(ROOT, "fetch_table.json"), encoding="utf-8") as fh:
        return json.load(fh)["packs"]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for block in iter(lambda: fh.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def wanted(pack):
    return [os.path.join(ASSETS, pack["into"], os.path.basename(t)) for t in pack["take"]]


def fetch(pack, pin):
    name = pack["name"]
    if all(os.path.exists(p) for p in wanted(pack)):
        print("have     %s" % name)
        return True
    os.makedirs(DL, exist_ok=True)
    zpath = os.path.join(DL, pack["file"])
    if not os.path.exists(zpath):
        if not pack["url"]:
            print("MISSING  %s: no direct link. Download it from %s and save it as assets/_dl/%s" % (name, pack["page"], pack["file"]))
            return False
        print("download %s" % name)
        req = urllib.request.Request(pack["url"], headers={"User-Agent": "sh4der-pvr-demo-fetch"})
        with urllib.request.urlopen(req, timeout=120) as r, open(zpath + ".part", "wb") as out:
            for block in iter(lambda: r.read(1 << 16), b""):
                out.write(block)
        os.replace(zpath + ".part", zpath)
    got = sha256(zpath)
    if pack["sha256"] is None:
        if not pin:
            print("UNPINNED %s: sha256 %s is not in fetch_table.json yet (run with --pin to accept it once)" % (name, got))
            return False
        print("pin      %s: \"sha256\": \"%s\"" % (name, got))
    elif got != pack["sha256"]:
        print("REFUSED  %s: sha256 %s, the table says %s" % (name, got, pack["sha256"]))
        return False
    os.makedirs(os.path.join(ASSETS, pack["into"]), exist_ok=True)
    with zipfile.ZipFile(zpath) as z:
        members = {os.path.basename(m): m for m in z.namelist() if not m.endswith("/")}
        for dst in wanted(pack):
            base = os.path.basename(dst)
            if base not in members:
                print("REFUSED  %s: the pack has no file named %s" % (name, base))
                return False
            if not os.path.exists(dst):                      # never over a file that is there
                with z.open(members[base]) as src, open(dst, "wb") as out:
                    out.write(src.read())
    print("unpacked %s" % name)
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--pin", action="store_true")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    ok = True
    for pack in table():
        if a.check:
            have = all(os.path.exists(p) for p in wanted(pack))
            print("%-8s %-20s %s" % ("have" if have else "missing", pack["name"], "pinned" if pack["sha256"] else "NOT pinned"))
            ok = ok and have
        else:
            ok = fetch(pack, a.pin) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
