#!/usr/bin/env python3
"""SH4DER-PVR: the test suite disc (image psm-suite-<tag>), build script.  Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/suite/build.py --tag s1 --list <table.json> [--out <dir>] [--cdi-dir <dir>] [--mkdcdisc <exe>]
                                   [--host-cc <cc>] [--host-only] [--no-image] [--title <text>] [--prefix <name>]

One disc: a menu the console boots, and every test as a file the menu starts (../psm_suite.h). This script builds no
test: it takes each test's ELF as its own build made it, named in the table, and checks what can be checked on a host.
Run it in a shell where the KallistiOS environment is sourced. It fetches nothing and touches no file outside --out
(default build/psm_suite/<tag>) and --cdi-dir (default out/cdi at the top of the repository; git ignores it).

The table is JSON: {"tests": [{"name", "file", "elf", "order", "what", "read", "group", "credits"}, ...]}
  name   up to 10 characters, shown in the menu     file   the name on the disc, 8.3, upper case, .BIN
  elf    the test's ELF, a path from the repository's root or absolute
  order  the test's place in its group's RUN ALL, 1 ..; 0 leaves it out
  group  optional: "demos" (the menu's TECH DEMOS) or "tools" (DEVELOPER TOOLS); "tools" when left out, so a table
         from before the groups builds as it did (a disc of one group has no top screen: suite_nav.h)
  credits  optional: a key of CREDITS below when the program carries third-party work that the menu's CREDITS screen
         must name ("pl4nar": the street demo's CC0 art)
--title is the menu's title line (default "PSM TEST SUITE"); --prefix the image's name before the tag (default
psm-suite). --image is the image's whole file name in --cdi-dir, in place of <prefix>-<tag>.cdi (still never
overwritten). demo/demos_disc.py gives all three: the examples disc is SH4DER-PVR-Examples.cdi.
  what   one line, up to 49 characters: what the test does and about how long
  read   one line, up to 59 characters: what to photograph
The menu look B v7 (suite_14.json on) adds, checked by check_menu (demo/psm/suite/test/suite_list_test.py):
  menu_group  one of the table's "menu_groups" (By Demo); the order in a group is the table's order
  effects     [{"name": the card's plain name, "heading": its By Effect row}], the main effect first
  card        {"what", "controls", "cost"}: plain sentences; cost only where a number was measured; no ledger IDs
  top level   "effect_headings" [{"heading" (24 characters at most), "demos" (names, best first)}] and
              "not_in_menu" {"title", "subtitle", "intro", "items" [{"name", "also_shown_in": [...]}]} (34 names)

Steps, each refusing to go on when it fails:
  1 host tests  test/suite_test.c (finding the mail in an image), test/suite_nav_test.c (the menu's screens and
                buttons, the credits text's form)
  2 tests       each ELF: it is an SH-4 ELF that loads and starts at 0x8c010000 (the address arch_exec copies to);
                objcopy to <out>/disc/<file>; the mail's mark counted in the binary: exactly one means the test was
                built with psm_suite.h and comes back to the menu, none means NO RETURN, more is refused; its image
                and .bss end below the stack's room
  3 menu        tools/make_credits.py --check (the repository's tools/) when an SH4ZAM checkout is there (the CREDITS text is still the
                licences' own words); suite_list.h written from the table and the checks; kos-cc suite_menu.c;
                objcopy to disc/MENU.BIN; the same checks, and its mail must be there
  4 image       mkdcdisc: the menu as the boot program, disc/* as files -> <cdi-dir>/psm-suite-<tag>.cdi, never
                overwritten. A manifest (every file's bytes and sha256, the image's) is written beside the build.

--host-only stops after step 1. --no-image stops after step 3.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
MARK = b"PSM-SUITE-MAIL-1"
LOAD = 0x8C010000
RAM_END = 0x8D000000
STACK_ROOM = 1 << 20                        # kept free under the top of RAM for the stack and the exec's copy loop
GROUPS = {"tools": 0, "demos": 1}           # suite_nav.h: NAV_TOOLS, NAV_DEMOS
CREDITS = {"pl4nar": "PSM_CREDITS_PL4NAR", "square": "PSM_CREDITS_SQUARE", "space": "PSM_CREDITS_SPACE"}  # a table's "credits" key -> the switch of ../psm_credits.h


def run(step, argv):
    print("== %s: %s" % (step, " ".join(argv)), flush=True)
    r = subprocess.run(argv, cwd=ROOT)
    if r.returncode:
        raise SystemExit("%s failed (rc %d)" % (step, r.returncode))


def find_tool(name):
    for d in os.environ.get("PATH", "").split(os.pathsep):
        for n in (name, name + ".exe"):
            p = os.path.join(d, n)
            if os.path.isfile(p):
                return p
    return None


def find_kos_cc():
    p = find_tool("kos-cc")
    if not p:
        return None
    return [shutil.which("sh") or "sh", p] if os.name == "nt" else [p]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def elf_facts(path):
    """(entry, lowest load address, end of the loaded image with .bss) of a 32-bit little-endian SH ELF."""
    with open(path, "rb") as f:
        d = f.read()
    if d[:4] != b"\x7fELF" or d[4] != 1 or d[5] != 1:
        raise SystemExit("%s: not a 32-bit little-endian ELF" % path)
    machine, = struct.unpack_from("<H", d, 18)
    if machine != 42:
        raise SystemExit("%s: not an SH ELF (machine %u)" % (path, machine))
    entry, phoff = struct.unpack_from("<II", d, 24)
    phentsize, phnum = struct.unpack_from("<HH", d, 42)
    lo, hi = None, 0
    for i in range(phnum):
        ptype, _, vaddr, _, _, memsz = struct.unpack_from("<IIIIII", d, phoff + i * phentsize)
        if ptype == 1 and memsz:
            lo = vaddr if lo is None else min(lo, vaddr)
            hi = max(hi, vaddr + memsz)
    return entry, lo, hi


def marks(path):
    """Offsets of the mail's mark in a binary, at multiples of 4 (where psm_suite.h looks)."""
    with open(path, "rb") as f:
        d = f.read()
    return [m.start() for m in re.finditer(re.escape(MARK), d) if m.start() % 4 == 0 and m.start() + 32 <= ((len(d) + 3) & ~3)], d.count(MARK)


def to_bin(objcopy, elf, out):
    run("objcopy", [objcopy, "-R", ".stack", "-O", "binary", elf, out])


def check_binary(name, elf, binp):
    entry, lo, hi = elf_facts(elf)
    if entry != LOAD or lo != LOAD:
        raise SystemExit("%s: starts at %08x and loads at %08x, not %08x: arch_exec cannot start it" % (name, entry, lo or 0, LOAD))
    if hi > RAM_END - STACK_ROOM:
        raise SystemExit("%s: image and .bss end at %08x, inside the stack's room" % (name, hi))
    at, total = marks(binp)
    if total > 1 or len(at) != total:
        raise SystemExit("%s: %u mail marks in the binary (%u where the loader looks): refused" % (name, total, len(at)))
    return {"bytes": os.path.getsize(binp), "returns": total == 1, "end": hi, "sha256": sha256(binp)}


LEDGER_ID = re.compile(r"\b[A-Z]{2,6}-\d{3}\b")   # a ledger ID (PVR-160, SH4-076): wiki only, never on a card
NOT_IN_MENU_N = 34                                 # the names in the Not in the menu row (menu look B v7)
MENU_GROUPS = ["Driving", "Rooms and light", "Characters", "Town", "Tools"]   # By Demo, in this order (suite_view.h)
TAB_ROWS_MAX = 250                                 # rows a tab the menu holds (headings and rows)


def check_menu(doc):
    """The menu fields of a table (look B v7: suite_14.json on). A table without them (suite_13 and before) passes as
    it is. Returns a list of lines saying what is wrong; empty when the table is right."""
    tests, bad = doc["tests"], []
    if "effect_headings" not in doc and not any("menu_group" in t for t in tests):
        return bad
    groups = doc.get("menu_groups", [])
    rows = {r["heading"]: r["demos"] for r in doc.get("effect_headings", [])}
    names = [t["name"] for t in tests]
    if len(rows) != len(doc.get("effect_headings", [])):
        bad.append("effect_headings: a heading is there twice")
    if groups != MENU_GROUPS:
        bad.append("menu_groups must be %r, in that order" % (MENU_GROUPS,))
    heads = [r["heading"] for r in doc.get("effect_headings", [])]
    if heads != sorted(heads):
        bad.append("effect_headings: not in alphabetical order")
    for h, demos in rows.items():
        if len(h) > 24 or h != h.upper():
            bad.append("effect heading %r: over 24 characters or not upper case" % h)
        if not demos:
            bad.append("effect heading %r: no demo (only effects with a demo are rows)" % h)
        for n in demos:
            if n not in names:
                bad.append("effect heading %r: no test named %r" % (h, n))
    for t in tests:
        who = t["name"]
        for key in ("menu_group", "effects", "card"):
            if key not in t:
                bad.append("%s: no %r" % (who, key))
        if "menu_group" in t and t["menu_group"] not in groups:
            bad.append("%s: menu_group %r is not one of menu_groups" % (who, t["menu_group"]))
        if not t.get("title") or len(t["title"]) > 25:
            bad.append("%s: title missing or over 25 characters" % who)
        effs = t.get("effects", [])
        if not effs:
            bad.append("%s: no effect" % who)
        for e in effs:
            if "text" in e and not e["text"]:
                bad.append("%s: an effect with an empty text" % who)
            if e.get("heading") not in rows:
                bad.append("%s: effect %r has no row in effect_headings" % (who, e.get("heading")))
            elif who not in rows[e["heading"]]:
                bad.append("%s: the row %r does not list it" % (who, e["heading"]))
        for h, demos in rows.items():
            if who in demos and h not in [e.get("heading") for e in effs]:
                bad.append("%s: listed under %r but has no such effect" % (who, h))
        card = t.get("card", {})
        for key in ("what", "controls"):
            if not card.get(key):
                bad.append("%s: card has no %r" % (who, key))
        for key, text in card.items():
            if LEDGER_ID.search(text) or "NOT RUN" in text:
                bad.append("%s: card %s carries a ledger ID or NOT RUN (status and IDs are not card data)" % (who, key))
    ni = doc.get("not_in_menu", {})
    items = ni.get("items", [])
    if len(items) != NOT_IN_MENU_N:
        bad.append("not_in_menu: %d names, not %d" % (len(items), NOT_IN_MENU_N))
    if len(set(i.get("name") for i in items)) != len(items):
        bad.append("not_in_menu: a name is there twice")
    for i in items:
        if not i.get("name") or not isinstance(i.get("also_shown_in", []), list):
            bad.append("not_in_menu: an entry with no name, or also_shown_in not a list")
    if not ni.get("subtitle") or not ni.get("intro"):
        bad.append("not_in_menu: no subtitle or intro")
    by_demo = len(set(t.get("menu_group") for t in tests)) + len(tests)
    by_effect = len(rows) + sum(len(d) for d in rows.values()) + 1 + len(items)
    if by_demo > TAB_ROWS_MAX or by_effect > TAB_ROWS_MAX:
        bad.append("a tab over %d rows (By Demo %d, By Effect %d)" % (TAB_ROWS_MAX, by_demo, by_effect))
    for path, text in menu_texts(doc):
        if any(not 32 <= ord(ch) <= 126 for ch in text):
            bad.append("%s: a character outside ASCII 32 .. 126 (the font has nothing else)" % path)
    return bad


def menu_texts(doc):
    """(where, text) for every string the menu shows."""
    for t in doc["tests"]:
        yield "%s title" % t["name"], t.get("title", "")
        for e in t.get("effects", []):
            yield "%s effect" % t["name"], fx_text(t, e)
        for k, v in t.get("card", {}).items():
            yield "%s card %s" % (t["name"], k), v
    for r in doc.get("effect_headings", []):
        yield "heading", r["heading"]
    ni = doc.get("not_in_menu", {})
    for k in ("subtitle", "intro"):
        yield "not_in_menu %s" % k, ni.get(k, "")
    for i in ni.get("items", []):
        yield "not_in_menu item", nim_text(i)


def fx_text(t, e):
    """An effect's bullet: its text, or its heading in sentence case with " (main)" on the first."""
    if e.get("text"):
        return e["text"]
    h = e.get("heading", "")
    return h[:1] + h[1:].lower() + (" (main)" if t["effects"] and t["effects"][0] is e else "")


def nim_text(i):
    return i["name"] + (" (also shown in: %s)" % "; ".join(i["also_shown_in"]) if i.get("also_shown_in") else "")


def write_cards(doc, path, list_name):
    """<out>/suite_cards.h: the menu's cards, its By Effect rows and the Not in the menu row, data only; the types are
    the menu's (suite_view.h). The same index as suite_list."""
    tests = doc["tests"]
    index = {t["name"]: k for k, t in enumerate(tests)}
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write("/* Written by demo/psm/suite/build.py from %s. Do not edit. */\n" % list_name)
        f.write("#define SUITE_GROUPS_N %uu\n" % len(MENU_GROUPS))
        f.write("static const char *const suite_group_name[SUITE_GROUPS_N] = { %s };\n" % ", ".join(c_str(g.upper()) for g in MENU_GROUPS))
        for k, t in enumerate(tests):
            f.write("static const char *const suite_card_fx_%u[] = { %s };\n" % (k, ", ".join(c_str(fx_text(t, e)) for e in t["effects"])))
        f.write("static const suite_card_t suite_card[SUITE_N] = {\n")
        for k, t in enumerate(tests):
            c = t["card"]
            f.write("    { %uu, %uu, suite_card_fx_%u, %s, %s, %s },\n" % (MENU_GROUPS.index(t["menu_group"]), len(t["effects"]), k,
                                                                    c_str(c["what"]), c_str(c["controls"]), c_str(c["cost"]) if c.get("cost") else "0"))
        f.write("};\n")
        rows = doc["effect_headings"]
        f.write("#define SUITE_FX_N %uu\n" % len(rows))
        for k, r in enumerate(rows):
            f.write("static const unsigned char suite_fx_d_%u[] = { %s };\n" % (k, ", ".join("%uu" % index[n] for n in r["demos"])))
        f.write("static const suite_fx_t suite_fx[SUITE_FX_N] = {\n")
        for k, r in enumerate(rows):
            f.write("    { %s, %uu, suite_fx_d_%u },\n" % (c_str(r["heading"]), len(r["demos"]), k))
        f.write("};\n")
        ni = doc["not_in_menu"]
        f.write("#define SUITE_NIM_N %uu\n" % len(ni["items"]))
        f.write("static const char suite_nim_subtitle[] = %s;\n" % c_str(ni["subtitle"]))
        f.write("static const char suite_nim_intro[] = %s;\n" % c_str(ni["intro"]))
        f.write("static const char *const suite_nim_item[SUITE_NIM_N] = {\n")
        for i in ni["items"]:
            f.write("    %s,\n" % c_str(nim_text(i)))
        f.write("};\n")


def c_str(s):
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')


def main():
    ap = argparse.ArgumentParser(description="Build the test suite disc.")
    ap.add_argument("--tag", required=True)
    ap.add_argument("--list", required=True)
    ap.add_argument("--out", default=None)
    ap.add_argument("--cdi-dir", default=os.path.join(ROOT, "out", "cdi"))
    ap.add_argument("--mkdcdisc", default=os.environ.get("MKDCDISC", "mkdcdisc"))
    ap.add_argument("--host-cc", default=os.environ.get("HOST_CC", "gcc"))
    ap.add_argument("--host-only", action="store_true")
    ap.add_argument("--no-image", action="store_true")
    ap.add_argument("--title", default="SH4DER-PVR SUITE")
    ap.add_argument("--prefix", default="psm-suite")
    ap.add_argument("--image", help="the image's file name in --cdi-dir, in place of <prefix>-<tag>.cdi")
    a = ap.parse_args()
    if not re.match(r"^[A-Za-z0-9 .+-]{1,24}$", a.title) or len(a.title) + 1 + len(a.tag) > 40:
        raise SystemExit("--title: up to 24 letters, digits, spaces, . + -, and 40 characters with the tag (the console's revision is beside it)")
    if not re.match(r"^[A-Za-z0-9-]{1,24}$", a.prefix):
        raise SystemExit("--prefix: up to 24 letters, digits, -")
    if a.image and not re.match(r"^[A-Za-z0-9-]{1,40}\.cdi$", a.image):
        raise SystemExit("--image: a file name of letters, digits and -, ending .cdi (no folder: it goes in --cdi-dir)")

    out = os.path.abspath(a.out or os.path.join(ROOT, "build", "psm_suite", a.tag))
    disc = os.path.join(out, "disc")
    os.makedirs(disc, exist_ok=True)
    image = os.path.join(os.path.abspath(a.cdi_dir), a.image or "%s-%s.cdi" % (a.prefix, a.tag))
    if not a.host_only and not a.no_image and os.path.exists(image):
        raise SystemExit("refuse existing image: %s (pick a new %s)" % (image, "--image" if a.image else "--tag"))

    exe = ".exe" if os.name == "nt" else ""
    test = os.path.join(out, "suite_test" + exe)
    run("1 host test (compile)", [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", os.path.join(ROOT, "test", "suite_test.c"), "-o", test])
    run("1 host test (run)", [test])
    test = os.path.join(out, "suite_nav_test" + exe)
    run("1 host test suite_nav_test (compile)", [a.host_cc, "-O2", "-std=c99", "-Wall", "-Wextra", "-Werror", os.path.join(HERE, "test", "suite_nav_test.c"), "-o", test])
    run("1 host test suite_nav_test (run)", [test])
    if a.host_only:
        return 0

    with open(a.list, encoding="utf-8") as f:
        doc = json.load(f)
    tests = doc["tests"]
    bad = check_menu(doc)
    if bad:
        raise SystemExit("the menu fields of %s:\n  %s" % (a.list, "\n  ".join(bad)))
    kos_cc, objcopy = find_kos_cc(), find_tool("sh-elf-objcopy")
    if not os.environ.get("KOS_BASE") or not kos_cc or not objcopy:
        raise SystemExit("kos-cc or sh-elf-objcopy is not on the PATH, or KOS_BASE is not set: source the KallistiOS environment first")

    files = set(["MENU.BIN", "1ST_READ.BIN"])
    for t in tests:
        if not re.match(r"^[A-Z0-9_]{1,8}\.BIN$", t["file"]) or t["file"] in files:
            raise SystemExit("%s: the disc name must be 8.3 upper case .BIN and not used twice" % t["file"])
        files.add(t["file"])
        t.setdefault("group", "tools")
        if t["group"] not in GROUPS or t.get("credits", "pl4nar") not in CREDITS:
            raise SystemExit("%s: group must be one of %s, credits (when given) one of %s" % (t["name"], sorted(GROUPS), sorted(CREDITS)))
        if len(t.get("title", "")) > 25 or len(t.get("shows", "")) > 49:
            raise SystemExit("%s: title over 25 or shows over 49 characters" % t["name"])
        if len(t["name"]) > 10 or len(t["what"]) > 49 or len(t["read"]) > 59:
            raise SystemExit("%s: name over 10, what over 49 or read over 59 characters" % t["name"])
        elf = t["elf"] if os.path.isabs(t["elf"]) else os.path.join(ROOT, t["elf"])
        if not os.path.isfile(elf):
            raise SystemExit("%s: no ELF at %s" % (t["name"], elf))
        t["elf_abs"], t["elf_sha256"] = elf, sha256(elf)
        binp = os.path.join(disc, t["file"])
        to_bin(objcopy, elf, binp)
        t.update(check_binary(t["name"], elf, binp))
        print("   %-10s %-12s %8u bytes, ends %08x, %s" % (t["name"], t["file"], t["bytes"], t["end"], "returns to the menu" if t["returns"] else "NO RETURN"))
    for group in sorted(GROUPS):
        orders = sorted(t["order"] for t in tests if t["order"] and t["group"] == group)
        if orders != list(range(1, len(orders) + 1)):
            raise SystemExit("RUN ALL's order must be 1 .. n with no gap in each group: %s has %r" % (group, orders))
    for t in tests:
        if t["order"] and not t["returns"]:
            raise SystemExit("%s is in RUN ALL but cannot come back to the menu" % t["name"])

    with open(os.path.join(out, "suite_list.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("/* Written by demo/psm/suite/build.py from %s. Do not edit. */\n" % os.path.basename(a.list))
        f.write("#define SUITE_TAG %s\n#define SUITE_TITLE %s\n#define SUITE_N %uu\n" % (c_str(a.tag), c_str(a.title), len(tests)))
        f.write("static const suite_entry_t suite_list[SUITE_N] = {\n")
        for t in tests:
            f.write("    { %s, %s, %s, %s, %uu, %uu, %uu, %uu, %s, %s },\n" % (c_str(t["file"]), c_str(t["name"]), c_str(t["what"]), c_str(t["read"]), t["order"], int(t["returns"]), t["bytes"],
                                                                 GROUPS[t["group"]], c_str(t["title"]) if t.get("title") else "0", c_str(t["shows"]) if t.get("shows") else "0"))
        f.write("};\n")
    if "effect_headings" in doc:
        write_cards(doc, os.path.join(out, "suite_cards.h"), os.path.basename(a.list))
    menu_elf, menu_bin =os.path.join(out, "%s-%s.elf" % (a.prefix, a.tag)), os.path.join(disc, "MENU.BIN")
    if os.path.isfile(os.path.join(ROOT, "third_party", "sh4zam", "LICENSE")):
        run("3 credits", [sys.executable, os.path.join(ROOT, "tools", "make_credits.py"), "--check", "--kos", os.environ["KOS_BASE"]])
    else:
        print("== 3 credits: NOT CHECKED against the licences (no third_party/sh4zam checkout); psm_credits.h is used as it is", flush=True)
    switches = sorted(set("-D" + CREDITS[t["credits"]] for t in tests if t.get("credits")))
    run("3 menu", kos_cc + ["-O2", "-Wall", "-Wextra", "-I" + out, "-I" + os.path.join(ROOT, "include")] + switches + [os.path.join(HERE, "suite_menu.c"), "-o", menu_elf])
    to_bin(objcopy, menu_elf, menu_bin)
    menu = check_binary("menu", menu_elf, menu_bin)
    if not menu["returns"]:
        raise SystemExit("the menu's binary has no mail: no test could hand anything back")

    lines = ["%s-%s" % (a.prefix, a.tag), "", "file          bytes     sha256 (binary on the disc)                                       returns  ELF sha256"]
    lines.append("%-12s %8u  %s  -        %s" % ("MENU.BIN", menu["bytes"], menu["sha256"], sha256(menu_elf)))
    for t in tests:
        lines.append("%-12s %8u  %s  %-7s  %s" % (t["file"], t["bytes"], t["sha256"], "yes" if t["returns"] else "NO", t["elf_sha256"]))
    if not a.no_image:
        os.makedirs(os.path.dirname(image), exist_ok=True)
        argv = [a.mkdcdisc, "-e", menu_elf, "-o", image, "-N", "-n", a.prefix.upper(), "-f", menu_bin]
        for t in tests:
            argv += ["-f", os.path.join(disc, t["file"])]
        run("4 image", argv)
        lines += ["", "image %s" % os.path.basename(image), "bytes %u" % os.path.getsize(image), "sha256 %s" % sha256(image)]
        print("IMAGE: %s" % image)
    with open(os.path.join(out, "manifest.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
