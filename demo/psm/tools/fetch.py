#!/usr/bin/env python3
"""Fetch the third-party art listed in demo/psm/fetch_table.json into demo/psm/assets/_dl.

Each file is checked against its sha256. A file with sha256 null is refused unless --pin is given,
which downloads it and prints the hash to put in the table. Nothing is overwritten that already matches.
"""
import hashlib
import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TABLE = os.path.join(ROOT, "fetch_table.json")
DEST = os.path.join(ROOT, "assets", "_dl")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def main(argv):
    pin = "--pin" in argv
    os.makedirs(DEST, exist_ok=True)
    with open(TABLE, encoding="utf-8") as f:
        table = json.load(f)
    bad = 0
    for item in table["files"]:
        path = os.path.join(DEST, item["file"])
        want = item["sha256"]
        if want is None and not pin:
            print("%s: not pinned (sha256 null); run with --pin once and put the hash in the table" % item["file"])
            bad += 1
            continue
        if not (os.path.exists(path) and (want is None or sha256(path) == want)):
            if item["url"] is None:                      # a download by hand: the page, then assets/_dl
                print("%s: missing; download it by hand from %s into %s" % (item["file"], item["page"], DEST))
                bad += 1
                continue
            with urllib.request.urlopen(item["url"], timeout=60) as r, open(path, "wb") as out:
                out.write(r.read())
        got = sha256(path)
        if want is None:
            print("%s: sha256 %s (put this in the table)" % (item["file"], got))
        elif got != want:
            print("%s: sha256 MISMATCH (want %s, got %s)" % (item["file"], want, got))
            bad += 1
        else:
            print("%s: ok" % item["file"])
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
