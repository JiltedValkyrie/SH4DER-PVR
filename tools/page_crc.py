#!/usr/bin/env python3
"""SH4DER-PVR: check a result page typed from a photograph against the CRC in its header.

tools/page_crc.py
Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

The console test programs' pages (include/sh4der_page.h) end their header line with "CRC xxxxxxxx": the CRC-32 (zip's)
of the page's content. A text page's content is each of its lines as shown, in order, each followed by one line feed,
up to its last line that is not empty. Type the page's lines (not the header) into a file, exactly as shown, and:

    python tools/page_crc.py page.txt [expected]

It prints the CRC; with the header's value as a second argument it says whether they agree, and exits 1 when they do
not. A line's leading spaces count (a continued line starts with three). Trailing empty lines are dropped.

TRAILING SPACES COUNT, and a photograph does not show them. The console is the reference: when it cuts a long line at
a space, that space goes with the cut, but where the text had two spaces there the first one stays at the end of the
line (found on the first suite run: "... SHED<=0 " before "   text 92v"). So this script keeps every trailing space
that is typed. When the typed page does not agree, it also tries one more space at the end of each line that is
followed by a continued line, in every combination, and says which lines needed it.

--self-test checks the routine against the CRC-32 check value and against the console's line cut.
"""
import itertools
import sys
import zlib

COLS = 64                                   # sh4der_page.h, PG_COLS at the small size


def page_lines(text):
    lines = [l.rstrip("\r") for l in text.split("\n")]
    while lines and not lines[-1].strip(" "):
        lines.pop()
    return lines


def lines_crc(lines):
    return zlib.crc32("".join(l + "\n" for l in lines).encode("latin-1")) & 0xFFFFFFFF


def page_crc(text):
    return lines_crc(page_lines(text))


def with_cut_spaces(lines, want):
    """The 1-based numbers of the lines that need one more trailing space for the page to give `want`, or None."""
    where = [i for i in range(len(lines) - 1) if lines[i + 1].startswith("   ") and not lines[i].endswith(" ")]
    if len(where) > 18:                     # 2^18 pages is the most this tries
        where = where[:18]
    for n in range(1, len(where) + 1):
        for pick in itertools.combinations(where, n):
            trial = [l + " " if i in pick else l for i, l in enumerate(lines)]
            if lines_crc(trial) == want:
                return [i + 1 for i in pick]
    return None


def console_wrap(line, cols=COLS):
    """pg_say's cut of one line into page lines (sh4der_page.h), for the self-test."""
    out, s, first = [], line, True
    while True:
        room = cols if first else cols - 3
        n, skip = min(len(s), room), 0
        if len(s) > room:
            k = room
            while k > room // 2 and s[k] != " ":
                k -= 1
            if s[k] == " ":
                n, skip = k, 1
        out.append(("" if first else "   ") + s[:n])
        s, first = s[n + skip:], False
        if not s:
            return out


def self_test():
    ok = zlib.crc32(b"123456789") == 0xCBF43926
    ok = ok and page_crc("8 FRAME 60 frames, bands 2 wide; us\n\n") == zlib.crc32(b"8 FRAME 60 frames, bands 2 wide; us\n")
    # a line with two spaces where the console cuts it: the first space stays on the page's line
    town = "  used KB 65/768/1024  est KB 754/754  KOS max 1024K  SHED<=0  text 92v"
    cut = console_wrap(town)
    ok = ok and cut == ["  used KB 65/768/1024  est KB 754/754  KOS max 1024K  SHED<=0 ", "   text 92v"]
    console = lines_crc(cut)
    ok = ok and console == 0x10BADF18
    ok = ok and page_crc("\n".join(cut) + "\n") == console                               # typed with the space: agrees
    typed = [l.rstrip(" ") for l in cut]
    ok = ok and lines_crc(typed) != console and with_cut_spaces(typed, console) == [1]   # typed without: found
    ok = ok and console_wrap("a" * 40 + " " + "b" * 40) == ["a" * 40, "   " + "b" * 40]   # one space: none stays
    print("page_crc self-test: %s" % ("ok" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv):
    if len(argv) >= 2 and argv[1] == "--self-test":
        return self_test()
    if len(argv) < 2:
        print(__doc__)
        return 2
    with open(argv[1], encoding="latin-1") as f:
        lines = page_lines(f.read())
    crc = lines_crc(lines)
    print("%08x" % crc)
    if len(argv) >= 3:
        want = int(argv[2], 16)
        if crc == want:
            print("agrees with the header")
            return 0
        need = with_cut_spaces(lines, want)
        if need:
            print("agrees with the header once a space is added at the end of line%s %s (the console keeps a space there: the text"
                  " has two where it cuts the line)" % ("s" if len(need) > 1 else "", ", ".join(str(n) for n in need)))
            return 0
        print("DOES NOT AGREE with the header's %08x: a character differs" % want)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
