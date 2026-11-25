#!/usr/bin/env python3
"""SH4DER-PVR: the disc's credits text (demo/psm/psm_credits.h), written from the licences themselves.
Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

    python demo/psm/tools/make_credits.py --kos <KallistiOS tree> [--sh4zam third_party/sh4zam] [--check]

Every third-party notice on the disc's CREDITS screen is taken from its source's own file, word for word, and only
re-wrapped to the screen's 64 columns:
  SH4ZAM       <sh4zam>/LICENSE, the whole file (MIT)
  SH4ZAM-DCE   third_party/sh4zam-dce/LICENSE must begin with the same text (checked), so it is named with the same notice
  KallistiOS   <kos>/doc/license/LICENSE.KOS from "All of the documentation" to the end (the KOS License), and the
               contributors' names of <kos>/AUTHORS (their years are left out and the text says so)
What this script writes itself (our own notice, the list of what is linked in, the art of the street demo) is in
OWN below; change it there. The street demo's art list is a copy of demo/pl4nar/CREDITS.md's table: when that table
changes, change it here.

Without --check it writes demo/psm/psm_credits.h. With --check it writes nothing and fails when the file on disk is
not what it would write (the suite's build runs this when it can find the trees), or when a line is over 64 columns.
No game or engine name goes into the text: SH4DER-PVR's own LICENSE file is named, not copied.
"""
import argparse
import os
import re
import sys
import textwrap

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
OUT = os.path.join(ROOT, "demo", "psm", "psm_credits.h")
COLS = 64
PAGE = "\f"                                  # a new page starts here

FOOT = ["SH4DER-PVR Copyright (c) 2026 Jilted Valkyrie",
        "Uses SH4ZAM (MIT) and KallistiOS (KOS License). Y: CREDITS"]


def wrap(text):
    """Paragraphs (blank-line separated) re-wrapped to COLS; the words and their order untouched."""
    out = []
    for para in re.split(r"\n\s*\n", text.strip("\n")):
        out += textwrap.wrap(" ".join(para.split()), COLS, break_long_words=False, break_on_hyphens=False) + [""]
    return out[:-1]


def numbered(text):
    """The KOS License's numbered conditions keep their numbers at the line's start."""
    out = []
    for para in re.split(r"\n\s*\n", text.strip("\n")):
        items = re.split(r"\n(?=\d\. )", para)
        for it in items:
            out += textwrap.wrap(" ".join(it.split()), COLS, subsequent_indent="   " if re.match(r"\d\. ", it) else "", break_long_words=False, break_on_hyphens=False)
        out.append("")
    return out[:-1]


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read().replace("\r\n", "\n")


def build(kos, sh4zam):
    mit = read(os.path.join(sh4zam, "LICENSE"))
    dce = os.path.join(ROOT, "third_party", "sh4zam-dce", "LICENSE")
    if os.path.isfile(dce) and not " ".join(read(dce).split()).startswith(" ".join(mit.split())):   # it goes on with a note of its own
        raise SystemExit("third_party/sh4zam-dce/LICENSE does not begin with SH4ZAM's text any more: give it its own section")
    m = re.match(r"MIT License\s+(Copyright \(c\) [^\n]+)\n(.*)", mit, re.S)
    if not m:
        raise SystemExit("SH4ZAM's LICENSE does not start as expected")
    mit_notice, mit_terms = m.group(1).strip(), m.group(2)
    koslic = read(os.path.join(kos, "doc", "license", "LICENSE.KOS"))
    at = koslic.find("All of the documentation and software")
    if at < 0:
        raise SystemExit("LICENSE.KOS: its terms were not found")
    kos_terms = koslic[at:]
    authors = read(os.path.join(kos, "AUTHORS"))
    at = authors.find("Contributors list")
    names = [ln.split(":")[0].strip() for ln in authors[at:].split("\n")[2:] if ":" in ln]
    if len(names) < 20:
        raise SystemExit("AUTHORS: the contributors were not found")

    t = []
    t += ["SH4DER-PVR", "Copyright (c) 2026 Jilted Valkyrie", ""]
    t += wrap("Every program on this disc is part of SH4DER-PVR and is under the licence in the LICENSE file of its source. "
              "Wherever SH4DER-PVR is used, attribution to its author is required in a credits or notices section such as this one.")
    t += ["", "THIRD-PARTY SOFTWARE IN THE PROGRAMS", ""]
    t += ["SH4ZAM (the SH-4 maths), MIT License", mit_notice, ""]
    t += wrap("SH4ZAM-DCE (a layer over SH4ZAM, used by some programs) carries the same notice and the same license.")
    t += [PAGE, "SH4ZAM and SH4ZAM-DCE: MIT License", mit_notice]              # no blank lines to spare: the terms fill the page
    t += wrap(mit_terms)
    cut = kos_terms.find("THIS SOFTWARE IS PROVIDED")
    if cut < 0:
        raise SystemExit("LICENSE.KOS: its disclaimer was not found")
    t += [PAGE, "KallistiOS (the Dreamcast system library): KOS License"]
    t += numbered(kos_terms[:cut])
    t += [PAGE, "KallistiOS: KOS License, the rest", ""]
    t += wrap(kos_terms[cut:])
    t += [PAGE, "KallistiOS's contributors, from its AUTHORS file (the years of", "each are in that file):", ""]
    t += textwrap.wrap(", ".join(names) + ".", COLS, break_long_words=False, break_on_hyphens=False)
    t += ["", "ALSO"]
    t += wrap("The C library (Newlib) and the compiler's runtime library (libgcc) are linked into the programs by the toolchain. "
              "Their own licences apply; KallistiOS's AUTHORS file names them as outside its own.")
    t += wrap("The text on screen is drawn from the console's own BIOS font, read when a program runs. No font is on this disc.")
    t += wrap("The disc image was written by mkdcdisc, Copyright (C) 2022-2026 Simulant and contributors. It is a tool: none of it is in the programs.")
    art = [PAGE, "ART IN THE STREET DEMO"]
    art += wrap("All of it is CC0 1.0, a public domain dedication. Credit is not required; it is given gladly.")
    art += [""] + wrap("Kenney (kenney.nl): Car Kit, Modular Buildings, City Kit Commercial, City Kit Suburban.")
    art += wrap("ambientCG (ambientcg.com): Asphalt025A, Bricks075A, Concrete034, PaintedPlaster017.")
    art += wrap("The numbers on that demo's own screen are Pillow's built-in bitmap font, drawn into a texture when the demo is built (Pillow: MIT-CMU licence).")
    art += [""] + wrap("The car, the street's props and structures, the map, the sky and the code of that demo are SH4DER-PVR's own work.")
    return t, art


def c_str(s):
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"').replace("\f", "\\f")


def render(t, art):
    for ln in FOOT + t + art:
        if len(ln) > COLS:
            raise SystemExit("over %u columns: %r" % (COLS, ln))
        if any(ord(c) < 32 and c != PAGE or ord(c) > 126 for c in ln):
            raise SystemExit("not plain ASCII: %r" % ln)
    o = ["/* SH4DER-PVR: the disc's copyright footer and CREDITS text. WRITTEN BY demo/psm/tools/make_credits.py from the",
         "   licences' own files: do not edit; change the script and run it.",
         "",
         "   demo/psm/psm_credits.h",
         "   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE. The third-party notices below are their owners'.",
         "",
         "   psm_credits_foot   two lines for the foot of every menu screen",
         "   psm_credits_text   the CREDITS screen's lines, 64 columns; \"\\f\" alone starts a new page",
         "   PSM_CREDITS_PL4NAR defined: the disc carries the street demo, and its art's page is in. */",
         "",
         "#ifndef PSM_CREDITS_H",
         "#define PSM_CREDITS_H",
         "",
         "static const char *const psm_credits_foot[2] = {"]
    o += ["    %s," % c_str(ln) for ln in FOOT]
    o += ["};", "", "static const char *const psm_credits_text[] = {"]
    o += ["    %s," % c_str(ln) for ln in t]
    o += ["#ifdef PSM_CREDITS_PL4NAR"]
    o += ["    %s," % c_str(ln) for ln in art]
    o += ["#endif", "};", "#define PSM_CREDITS_N ((unsigned)(sizeof psm_credits_text / sizeof psm_credits_text[0]))", "", "#endif /* PSM_CREDITS_H */", ""]
    return "\n".join(o)


def main():
    ap = argparse.ArgumentParser(description="Write or check demo/psm/psm_credits.h.")
    ap.add_argument("--kos", default=os.environ.get("KOS_BASE"))
    ap.add_argument("--sh4zam", default=os.path.join(ROOT, "third_party", "sh4zam"))
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    if not a.kos or not os.path.isfile(os.path.join(a.kos, "doc", "license", "LICENSE.KOS")):
        raise SystemExit("no KallistiOS tree (--kos or KOS_BASE) with doc/license/LICENSE.KOS")
    if not os.path.isfile(os.path.join(a.sh4zam, "LICENSE")):
        raise SystemExit("no SH4ZAM LICENSE at %s (--sh4zam)" % a.sh4zam)
    text = render(*build(a.kos, a.sh4zam))
    if a.check:
        have = read(OUT) if os.path.isfile(OUT) else ""
        if have != text:
            raise SystemExit("demo/psm/psm_credits.h is not what the licences give now: run demo/psm/tools/make_credits.py and look at the difference")
        print("psm_credits.h: matches the licences' files (%u lines of text)" % text.count("\n"))
        return 0
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote %s" % OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
