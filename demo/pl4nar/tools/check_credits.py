#!/usr/bin/env python3
"""Every pack in fetch_table.json has a row in CREDITS.md, and every art row in CREDITS.md is a pack in the table.

    python tools/check_credits.py      exit 0 and CREDITS_OK, or the names that are on one side only
    python tools/check_credits.py --root DIR   another demo's fetch_table.json, CREDITS.md and licenses/

An art row is a table row under "## Art" whose first cell is the pack's name. Also checks that each row carries the
pack's page link and its licence, and that licenses/CC0-1.0.txt is there. Standard library only.
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def art_rows(text):
    rows, inside = {}, False
    for line in text.splitlines():
        if line.startswith("## "):
            inside = line.strip() == "## Art"
            continue
        if inside and line.startswith("|") and not set(line) <= set("|- :"):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if cells and cells[0] != "Pack":
                rows[cells[0]] = line
    return rows


def main():
    global ROOT
    if len(sys.argv) == 3 and sys.argv[1] == "--root":
        ROOT = os.path.abspath(sys.argv[2])
    elif len(sys.argv) != 1:
        print("usage: check_credits.py [--root DIR]")
        return 2
    with open(os.path.join(ROOT, "fetch_table.json"), encoding="utf-8") as fh:
        packs = {p["name"]: p for p in json.load(fh)["packs"]}
    with open(os.path.join(ROOT, "CREDITS.md"), encoding="utf-8") as fh:
        rows = art_rows(fh.read())
    bad = []
    for name in sorted(set(packs) - set(rows)):
        bad.append("no CREDITS row: %s" % name)
    for name in sorted(set(rows) - set(packs)):
        bad.append("not in the fetch table: %s" % name)
    for name in sorted(set(packs) & set(rows)):
        if packs[name]["page"] not in rows[name]:
            bad.append("row without the pack's page link: %s" % name)
        if packs[name]["licence"] not in rows[name]:
            bad.append("row without the pack's licence: %s" % name)
    if not os.path.exists(os.path.join(ROOT, "licenses", "CC0-1.0.txt")):
        bad.append("licenses/CC0-1.0.txt is missing")
    for b in bad:
        print(b)
    if bad:
        return 1
    print("CREDITS_OK %d packs" % len(packs))
    return 0


if __name__ == "__main__":
    sys.exit(main())
