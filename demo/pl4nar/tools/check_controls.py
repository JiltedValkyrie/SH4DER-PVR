#!/usr/bin/env python3
"""The README's Controls table is the controls table in src/controls.h, row for row.

    python tools/check_controls.py           exit 0 and CONTROLS_OK, or each row that differs
    python tools/check_controls.py --table   print the table as the README should have it
    python tools/check_controls.py --self-test

src/controls.h is what the pad read (read_pad through ctl_map) and the HUD's key lines use, so a README that matches it
says what the code does. A row of controls.h is a CTL_ROW( line; the README's table is the first table under
"## Controls" (Keys | Action | Builds). Standard library only.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROW = re.compile(r'^CTL_ROW\(([^,]+),\s*([^,]+),\s*([^,]+),\s*"([^"]*)",\s*"([^"]*)",\s*(NULL|"[^"]*")\)\s*$')
BUILDS = {"CTL_B_STREET": "street", "CTL_B_MAP": "map", "CTL_B_HILLS": "hills"}


def builds_text(expr):
    if expr.strip() == "CTL_B_ALL":
        return "all"
    names = [BUILDS[p.strip()] for p in expr.split("|")]
    return ", ".join(n for n in ("street", "map", "hills") if n in names)


def code_rows(text):
    rows = []
    for line in text.splitlines():
        if line.startswith("CTL_ROW("):
            m = ROW.match(line.strip())
            if not m:
                raise ValueError("a CTL_ROW line this check cannot read: " + line)
            rows.append((m.group(4), m.group(5), builds_text(m.group(3))))
    return rows


def readme_rows(text):
    rows, inside, seen = [], False, False
    for line in text.splitlines():
        if line.startswith("## "):
            if inside:
                break
            inside = line.strip() == "## Controls"
            continue
        if not inside:
            continue
        if line.startswith("|"):
            seen = True
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if set(line) <= set("|- :") or cells[0] == "Keys":
                continue
            rows.append(tuple(c.replace("**", "") for c in cells))
        elif seen:
            break
    return rows


def table(rows):
    out = ["| Keys | Action | Builds |", "|---|---|---|"]
    out += ["| **%s** | %s | %s |" % r for r in rows]
    return "\n".join(out)


def compare(code, readme):
    bad = []
    for i in range(max(len(code), len(readme))):
        c = code[i] if i < len(code) else None
        r = readme[i] if i < len(readme) else None
        if c != r:
            bad.append("row %d: controls.h %s, README %s" % (i + 1, c, r))
    return bad


def self_test():
    h = 'CTL_ROW(CTL_X, DEMO_BTN_REFLECT, CTL_B_ALL, "X", "reflection on / off", "X reflect")\n' \
        'CTL_ROW(CTL_B | CTL_Y, DEMO_BTN_TILES, CTL_B_MAP | CTL_B_HILLS, "B + Y", "tiles", NULL)\n'
    c = code_rows(h)
    assert c == [("X", "reflection on / off", "all"), ("B + Y", "tiles", "map, hills")], c
    md = "## Controls\n\n" + table(c) + "\n\nmore\n\n## Cost\n| a | b | c |\n"
    assert readme_rows(md) == c
    assert not compare(c, readme_rows(md))
    assert compare(c, readme_rows(md.replace("tiles |", "tile |")))
    print("SELF_TEST_OK")


def main():
    if "--self-test" in sys.argv:
        self_test()
        return 0
    code = code_rows(open(os.path.join(ROOT, "src", "controls.h"), encoding="utf-8").read())
    if "--table" in sys.argv:
        print(table(code))
        return 0
    readme = readme_rows(open(os.path.join(ROOT, "README.md"), encoding="utf-8").read())
    bad = compare(code, readme)
    for b in bad:
        print(b)
    if bad:
        return 1
    print("CONTROLS_OK (%d rows)" % len(code))
    return 0


if __name__ == "__main__":
    sys.exit(main())
