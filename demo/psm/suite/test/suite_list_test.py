#!/usr/bin/env python3
"""SH4DER-PVR: host test of the suite tables' menu fields (menu look B v7), build.py's check_menu and write_cards.
Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

    python -I demo/psm/suite/test/suite_list_test.py [table.json ...]   (default: every suite_*.json beside build.py)

Checks:
  1 every table: check_menu finds nothing wrong (a table from before the menu fields passes as it is); suite_14.json
    is there and carries the fields
  2 suite_14.json: 14 tests, every title 25 characters at most, every effect heading 24 at most, every demo's effects
    in the By Effect rows and every row's demos carry it, the 34 names of Not in the menu, the heat haze entry
  3 broken copies of suite_14.json are refused, one fault each: a long title, a long heading, an effect with no row, a
    row with a demo that lacks it, 33 names, a ledger ID on a card, a menu group that is not listed, a demo with no
    effect, a character outside ASCII 32 .. 126, the headings out of order, the groups out of order
  4 suite_cards.h (write_cards) from suite_14.json: its counts, and it compiles with the menu's two types, a copy
    held here (TYPES; suite_view.h is not read, so the test does not wait on the menu program), when a host C
    compiler is there and can build an empty program (NOT RUN is printed when not)
Standard library only. Prints "suite_list_test: ok" or each failure and exits 1.
"""
import copy
import glob
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SUITE = os.path.dirname(HERE)
spec = importlib.util.spec_from_file_location("suite_build_for_test", os.path.join(SUITE, "build.py"))
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)

TYPES = ("typedef struct suite_card { unsigned char menu_group, n_effects; const char *const *effects; "
         "const char *what, *controls, *cost; } suite_card_t;\n"
         "typedef struct suite_fx { const char *heading; unsigned char n; const unsigned char *demos; } suite_fx_t;\n")
fails = []


def check(cond, what):
    if not cond:
        fails.append(what)


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def main(argv):
    tables = argv or sorted(glob.glob(os.path.join(SUITE, "suite_*.json")))
    for p in tables:
        bad = build.check_menu(load(p))
        check(not bad, "%s: %s" % (os.path.basename(p), "; ".join(bad)))

    p14 = os.path.join(SUITE, "suite_14.json")
    check(os.path.isfile(p14), "suite_14.json is not there")
    if not os.path.isfile(p14):
        return report()
    d = load(p14)
    t = d["tests"]
    check(len(t) == 14, "suite_14: %d tests, not 14" % len(t))
    check(all(len(x["title"]) <= 25 for x in t), "suite_14: a title over 25 characters")
    check(all(len(r["heading"]) <= 24 for r in d["effect_headings"]), "suite_14: a heading over 24 characters")
    check(len(d["not_in_menu"]["items"]) == 34, "suite_14: Not in the menu has %d names" % len(d["not_in_menu"]["items"]))
    check(any(x["name"] == "heat-haze" and x["menu_group"] == "Driving" and x["title"] == "Heat haze on a hot road" for x in t),
          "suite_14: no heat haze entry in Driving")
    check(not build.check_menu(d), "suite_14: check_menu refuses it")

    def broken(change, what):
        b = copy.deepcopy(d)
        change(b)
        check(build.check_menu(b), "a broken table is not refused: %s" % what)

    broken(lambda b: b["tests"][0].__setitem__("title", "x" * 26), "a title of 26")
    broken(lambda b: b["effect_headings"][0].__setitem__("heading", "X" * 25), "a heading of 25")
    broken(lambda b: b["tests"][0]["effects"].append({"heading": "NO SUCH ROW", "text": "New"}), "an effect with no row")
    broken(lambda b: b["effect_headings"][0]["demos"].append(b["tests"][-1]["name"]), "a row with a demo that lacks it")
    broken(lambda b: b["not_in_menu"]["items"].pop(), "33 names")
    broken(lambda b: b["tests"][0]["card"].__setitem__("cost", "PVR-160: 18 ms"), "a ledger ID on a card")
    broken(lambda b: b["tests"][0].__setitem__("menu_group", "Nowhere"), "a menu group not listed")
    broken(lambda b: b["tests"][0].__setitem__("effects", []), "a demo with no effect")
    broken(lambda b: b["tests"][0]["card"].__setitem__("what", "a curly " + chr(0x2019) + "quote"), "a character outside ASCII")
    broken(lambda b: b["effect_headings"].reverse(), "headings out of alphabetical order")
    broken(lambda b: b.__setitem__("menu_groups", list(reversed(b["menu_groups"]))), "the groups in another order")

    with tempfile.TemporaryDirectory() as tmp:
        h = os.path.join(tmp, "suite_cards.h")
        build.write_cards(d, h, "suite_14.json")
        with open(h, encoding="ascii") as f:
            text = f.read()
        check("#define SUITE_FX_N 14u" in text and "#define SUITE_NIM_N 34u" in text and "#define SUITE_GROUPS_N 5u" in text,
              "suite_cards.h: the counts are not 14 rows, 34 names and 5 groups")
        cc = shutil.which(os.environ.get("HOST_CC", "gcc")) or shutil.which("cc")
        if cc:   # a compiler that cannot build an empty program here (a gcc found on a PATH that lacks its own folders) is no compiler
            probe = os.path.join(tmp, "probe.c")
            with open(probe, "w", encoding="ascii") as f:
                f.write("int main(void) { return 0; }\n")
            if subprocess.run([cc, probe, "-o", os.path.join(tmp, "probe.exe")], capture_output=True).returncode:
                print("suite_list_test: the suite_cards.h compile NOT RUN (%s cannot build an empty program in this shell)" % cc)
                cc = None
        if cc:
            c = os.path.join(tmp, "t.c")
            with open(c, "w", encoding="ascii") as f:
                f.write("#define SUITE_N %uu\n" % len(t))
                f.write(TYPES)
                f.write('#include "suite_cards.h"\n')
                f.write("int main(void) { return suite_card[0].what && suite_fx[0].n && suite_nim_item[0] && suite_group_name[0]"
                        " && suite_nim_subtitle[0] && suite_nim_intro[0] && suite_card_fx_0[0] && suite_fx_d_0[0] < SUITE_N ? 0 : 1; }\n")
            exe = os.path.join(tmp, "t.exe")
            r = subprocess.run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable", c, "-o", exe],
                               capture_output=True, text=True)
            check(r.returncode == 0, "suite_cards.h does not compile (rc %d): %s" % (r.returncode, (r.stderr + r.stdout).strip()[:400]))
            if r.returncode == 0:
                check(subprocess.run([exe]).returncode == 0, "suite_cards.h: the sample program fails")
        elif not (shutil.which(os.environ.get("HOST_CC", "gcc")) or shutil.which("cc")):
            print("suite_list_test: the suite_cards.h compile NOT RUN (no host C compiler)")
    return report()


def report():
    for f in fails:
        print("FAIL", f)
    print("suite_list_test: ok" if not fails else "suite_list_test: %d failed" % len(fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
