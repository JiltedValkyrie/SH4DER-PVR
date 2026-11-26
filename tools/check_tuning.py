#!/usr/bin/env python3
"""Check README.md's tuning tables against the SH4DER_* macros of include/sh4der_*.h.

A macro counts when its #define has a numeric default: a number, or an expression of numbers
and other SH4DER_* macros (it is worked out). Include guards, macros with arguments and
macros that are not a number (a string, a type, a KallistiOS name) are left out.

A table row counts when its first cell is one SH4DER_* name (backticks allowed) and it has a
second cell: that cell is the value.

Reported, each with file:line:
  DIFFERS     the README's value is not the header's (floats: equal to 1 part in a million)
  STALE       a table row whose name is no macro and no enumerator of a header. A row that names
              an enumerator (a table of kinds), or a macro that is not a number, is not checked;
              the report counts those rows
  UNREADABLE  a table row whose value cell is not a number
  EXEMPT      a name in EXEMPT below that no header defines as a number (the list is out of date)
  MISSING     a numeric macro that no table row names and EXEMPT does not list: a warning,
              a failure with --strict

usage: check_tuning.py [--strict] [--verbose] [repo]   (default: the repo this script is in)
       check_tuning.py --self-test                     the checks of this script, against a made-up repo

--verbose also lists the rows not checked and the macros left out as not numeric.
Exit 0 when no row differs, is stale or unreadable and EXEMPT is current (and, with --strict,
nothing is missing), 1 otherwise. Standard library only.
"""
import ast
import glob
import os
import re
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REL_TOL = 1.0e-6

# Numeric macros that are not tuning values and need no table row: (why, names) a group.
EXEMPT = [
    ("flags and indices: a name for a choice, the number carries no tuning", """
        SH4DER_CA_RED SH4DER_CA_GREEN SH4DER_CA_BLUE
        SH4DER_PVRX_AREA_64 SH4DER_PVRX_AREA_32 SH4DER_PVRX_END_POLL SH4DER_PVRX_END_IRQ
        SH4DER_RISE_SALT_LIFT SH4DER_RISE_SALT_DX SH4DER_RISE_SALT_DZ SH4DER_RISE_SALT_SKY SH4DER_RISE_SALT_SWAY
        SH4DER_RISE_SALT_TWK
        SH4DER_SPARK_FREE SH4DER_SPARK_BILLBOARD SH4DER_SPARK_STREAK
        SH4DER_SPARK_KEEP SH4DER_SPARK_RETREAT SH4DER_SPARK_TAIL0 SH4DER_SPARK_GRAVITY
     """),
    ("derived counts: worked out from macros that have a row, or fixed by the shape", """
        SH4DER_BLOOM_RINGS_VERTS SH4DER_BLOOM_RINGS_TRIS
        SH4DER_FOCUS_TEX_BYTES SH4DER_FOCUS_ROW_VERTS SH4DER_FOCUS_VERTS SH4DER_FOCUS_TRIS
        SH4DER_TORCH_VERTS
        SH4DER_VISOR_LUT_BINS SH4DER_VISOR_GRAIN_BYTES SH4DER_VISOR_SCAN_BYTES SH4DER_VISOR_LUT_BYTES
        SH4DER_CLOUDVOLUME_GROWTH_LN SH4DER_MIST_SLICE_VERTS
     """),
    ("constants: mathematics, hash numbers and hardware words, not a choice", """
        SH4DER_FILM_TAU SH4DER_MIST_TAU SH4DER_RISE_TAU SH4DER_WATER_TAU SH4DER_RISE_INV255
        SH4DER_FILM_HASH_MASK SH4DER_FILM_HASH_SCALE SH4DER_RISE_HASH_MASK SH4DER_RISE_HASH_SCALE
        SH4DER_MOTES_GEN_WRAP SH4DER_MOTES_GEN_PRIME SH4DER_LENS_GRAIN_SEED
        SH4DER_PVRX_DMA_UNIT SH4DER_PVRX_CMD_USERCLIP SH4DER_FAULT_VSYNC
     """),
    ("input limits: the bounds a call refuses or clamps at, there to keep the maths finite", """
        SH4DER_DEFORM_MAX_DIM SH4DER_DEFORM_MAX_CELLS SH4DER_DEFORM_MAX_HEIGHT SH4DER_DEFORM_MIN_RADIUS
        SH4DER_DEFORM_MAX_RADIUS SH4DER_DEFORM_MAX_STEPS SH4DER_DEFORM_MIN_STEP
        SH4DER_MIST_MAX_COORD SH4DER_MIST_MAX_EXTENT SH4DER_MIST_MIN_TILE SH4DER_MIST_MAX_RATE
        SH4DER_MIST_DIR_MIN2 SH4DER_MIST_DIR_MAX2
        SH4DER_FILM_FLICKER_MAX SH4DER_FILM_RATE_MAX_HZ SH4DER_FILM_WEAVE_MAX_PX SH4DER_FILM_TIME_MAX_S
        SH4DER_UVWARP_SIDE_MAX SH4DER_UVWARP_UV_MAX
     """),
]

ENUM = re.compile(r"\benum\b[^{;]*\{([^{}]*)\}")
DEFINE = re.compile(r"^\s*#\s*define\s+(SH4DER_\w+)(\(?)(.*)$")
ROW = re.compile(r"^\|\s*`?(SH4DER_\w+)`?\s*\|([^|]*)\|")
NUMBER = re.compile(
    r"0[xX][0-9a-fA-F]+[uUlL]*"
    r"|(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fFuUlL]*"
)
TOKEN = re.compile(r"\s*(%s|[A-Za-z_]\w*|<<|>>|[-+*/%%()|&^~])" % NUMBER.pattern)


def c_number(tok):
    """A C number token as an int or a float."""
    if tok[:2] in ("0x", "0X"):
        return int(tok.rstrip("uUlL"), 16)
    body = tok.rstrip("fFuUlL")
    if re.fullmatch(r"\d+", body) and not re.search(r"[fF]$", tok):
        return int(body)
    return float(body)


def evaluate(text, lookup):
    """The value of a C constant expression, or None. lookup(name) gives a macro's value or None."""
    out = []
    pos = 0
    text = text.strip()
    if not text:
        return None
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if not m:
            return None
        tok = m.group(1)
        pos = m.end()
        if NUMBER.fullmatch(tok):
            out.append(repr(c_number(tok)))
        elif re.match(r"[A-Za-z_]", tok):
            val = lookup(tok)
            if val is None:
                return None
            out.append("(%r)" % (val,))
        else:
            out.append(tok)
    try:
        tree = ast.parse(" ".join(out), mode="eval")
    except SyntaxError:
        return None

    def walk(node):
        if isinstance(node, ast.Expression):
            return walk(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)) and not isinstance(node.value, bool):
            return node.value
        if isinstance(node, ast.UnaryOp):
            v = walk(node.operand)
            if isinstance(node.op, ast.USub):
                return -v
            if isinstance(node.op, ast.UAdd):
                return v
            if isinstance(node.op, ast.Invert) and isinstance(v, int):
                return ~v & 0xFFFFFFFF
            raise ValueError
        if isinstance(node, ast.BinOp):
            a, b = walk(node.left), walk(node.right)
            both_int = isinstance(a, int) and isinstance(b, int)
            if isinstance(node.op, ast.Add):
                return a + b
            if isinstance(node.op, ast.Sub):
                return a - b
            if isinstance(node.op, ast.Mult):
                return a * b
            if isinstance(node.op, ast.Div):
                if both_int:                       # C: toward zero
                    q = abs(a) // abs(b)
                    return q if (a < 0) == (b < 0) else -q
                return a / b
            if both_int:
                if isinstance(node.op, ast.Mod):
                    return a % b
                if isinstance(node.op, ast.LShift):
                    return a << b
                if isinstance(node.op, ast.RShift):
                    return a >> b
                if isinstance(node.op, ast.BitOr):
                    return a | b
                if isinstance(node.op, ast.BitAnd):
                    return a & b
                if isinstance(node.op, ast.BitXor):
                    return a ^ b
        raise ValueError

    try:
        return walk(tree)
    except (ValueError, ZeroDivisionError, OverflowError):
        return None


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n") or " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def read_headers(root):
    """({name: [(text, rel, line)]} for every SH4DER_* define, the SH4DER_* enumerators of the headers)."""
    raw = {}
    enums = set()
    for path in sorted(glob.glob(os.path.join(root, "include", "sh4der_*.h"))):
        rel = "include/" + os.path.basename(path)
        with open(path, encoding="utf-8", errors="replace") as f:
            text = strip_comments(f.read())
        for body in ENUM.findall(text):
            enums.update(re.findall(r"(?:^|,)\s*(SH4DER_\w+)", body))
        lines = text.split("\n")
        i = 0
        while i < len(lines):
            line, at = lines[i], i + 1
            while line.endswith("\\") and i + 1 < len(lines):
                i += 1
                line = line[:-1] + " " + lines[i]
            i += 1
            m = DEFINE.match(line)
            if not m:
                continue
            name, paren, body = m.groups()
            if paren:                              # "NAME(" with no space before the bracket: a macro with arguments
                raw.setdefault(name, []).append((None, rel, at))
                continue
            raw.setdefault(name, []).append((body.strip(), rel, at))
    return raw, enums


def header_values(raw):
    """({name: [(value, rel, line)]}, [(name, rel, line)] left out as not numeric)."""
    cache = {}

    def lookup(name, seen=()):
        if name in cache:
            return cache[name]
        if name in seen or name not in raw:
            return None
        text = raw[name][0][0]
        val = evaluate(text, lambda n: lookup(n, seen + (name,))) if text else None
        cache[name] = val
        return val

    values, left_out = {}, []
    for name, defs in raw.items():
        for text, rel, line in defs:
            if not text:
                if text is None:
                    left_out.append((name, rel, line))
                continue                           # an include guard or a switch
            val = evaluate(text, lookup)
            if val is None:
                left_out.append((name, rel, line))
            else:
                values.setdefault(name, []).append((val, rel, line))
    return values, left_out


def read_tables(readme):
    """[(name, cell, line)] for every table row that starts with one SH4DER_* name."""
    rows = []
    with open(readme, encoding="utf-8", errors="replace") as f:
        for n, line in enumerate(f, 1):
            m = ROW.match(line)
            if m:
                rows.append((m.group(1), m.group(2).strip(), n))
    return rows


def same(a, b):
    if a == b:
        return True
    return abs(a - b) <= REL_TOL * max(abs(a), abs(b))


def show(v):
    if isinstance(v, int):
        return "%d (0x%X)" % (v, v) if v > 0xFFFF else "%d" % v
    return "%.9g" % v


def check(root, exempt=None, strict=False):
    """(lines of the report, number of failures, counts by kind, macros left out, rows not checked)."""
    if exempt is None:
        exempt = [n for _, names in EXEMPT for n in names.split()]
    raw, enums = read_headers(root)
    values, left_out = header_values(raw)
    rows = read_tables(os.path.join(root, "README.md"))
    differs, stale, unreadable, unchecked = [], [], [], []
    named = set()
    for name, cell, line in rows:
        named.add(name)
        if name not in values:
            if name in enums or name in raw:        # a table of kinds, or a macro that is not a number
                unchecked.append("README.md:%d  %s  (%s)" % (line, name, "an enumerator" if name in enums else "not a number"))
                continue
            stale.append("README.md:%d  %s  (value %s)" % (line, name, cell))
            continue
        got = evaluate(cell.replace("`", ""), lambda n: values[n][0][0] if n in values else None)
        if got is None:
            unreadable.append("README.md:%d  %s  value cell '%s'  (header: %s)"
                              % (line, name, cell, show(values[name][0][0])))
            continue
        if not any(same(got, v) for v, _, _ in values[name]):
            v, rel, at = values[name][0]
            differs.append("README.md:%d  %s  README %s, header %s  (%s:%d)" % (line, name, cell, show(v), rel, at))
    gone = ["%s  (no header defines it as a number: take it off EXEMPT)" % n for n in exempt if n not in values]
    missing = {}
    for name in values:
        if name not in named and name not in exempt:
            v, rel, at = values[name][0]
            missing.setdefault(rel, []).append("%s:%d  %s  %s" % (rel, at, name, show(v)))
    n_missing = sum(len(m) for m in missing.values())
    out = []
    for title, items in (("DIFFERS", differs), ("STALE", stale), ("UNREADABLE", unreadable), ("EXEMPT", gone)):
        out.append("%s: %d" % (title, len(items)))
        out += ["  " + s for s in items]
    out.append("MISSING (%s): %d" % ("fails: --strict" if strict else "warning", n_missing))
    for rel in sorted(missing):
        out.append("  %s (%d)" % (rel, len(missing[rel])))
        out += ["    " + s for s in missing[rel]]
    counts = {"differs": len(differs), "stale": len(stale), "unreadable": len(unreadable), "missing": n_missing,
              "gone": len(gone), "exempt": len(exempt) - len(gone), "macros": len(values), "rows": len(rows),
              "unchecked": len(unchecked)}
    bad = len(differs) + len(stale) + len(unreadable) + len(gone) + (n_missing if strict else 0)
    return out, bad, counts, left_out, unchecked


def self_test():
    ok = True

    def expect(name, got, want):
        nonlocal ok
        if got != want:
            ok = False
            print("FAIL %s: got %r, want %r" % (name, got, want), file=sys.stderr)

    none = lambda n: None
    expect("float suffix", evaluate("0.55f", none), 0.55)
    expect("hex", evaluate("0xFFFFEBF0u", none), 0xFFFFEBF0)
    expect("unsigned", evaluate("128u", none), 128)
    expect("exponent", same(evaluate("1.0e-18f", none), 1e-18), True)
    expect("negative", evaluate("-0.065f", none), -0.065)
    expect("brackets", evaluate("(1 + 3 * 8)", none), 25)
    expect("int divide", evaluate("7 / 2", none), 3)
    expect("float divide", evaluate("1.0f / 4", none), 0.25)
    expect("shift", evaluate("1u << 5", none), 32)
    expect("a name", evaluate("SH4DER_A * 2", {"SH4DER_A": 6}.get), 12)
    expect("an unknown name", evaluate("PVR_LIST_TR_POLY", none), None)
    expect("a string", evaluate('"0.05"', none), None)
    expect("a cast", evaluate("(uint32_t)4", none), None)
    expect("empty", evaluate("", none), None)
    expect("0.40 is 0.4", same(0.40, evaluate("0.4f", none)), True)
    expect("0.41 is not 0.4", same(0.41, 0.4), False)
    expect("1 is 1.0", same(1, 1.0), True)

    header = (
        "#ifndef SH4DER_T_H\n#define SH4DER_T_H\n"
        "#define SH4DER_T_GOOD 0.40f /**< \\brief Agrees. */\n"
        "#define SH4DER_T_INT 192 /* agrees */\n"
        "#define SH4DER_T_BAD 0.25f // differs\n"
        "#define SH4DER_T_GONE 3u\n"
        "#define SH4DER_T_SUM (SH4DER_T_INT * 2 + \\\n    1)\n"
        "#define SH4DER_T_FN(x) ((x) * 2)\n"
        "#define SH4DER_T_TEXT \"v\"\n"
        "#define SH4DER_T_WORDS 4\n"
        "enum { SH4DER_T_KIND = 0, SH4DER_T_KIND_B };\n"
        "/* #define SH4DER_T_COMMENT 9 */\n"
        "#if 1\n#define SH4DER_T_TWO 1\n#else\n#define SH4DER_T_TWO 2\n#endif\n"
        "#endif\n"
    )
    readme = (
        "# T\n\n| Macro | Default | Meaning |\n|---|---|---|\n"
        "| SH4DER_T_GOOD | 0.4 | same number, other spelling |\n"
        "| `SH4DER_T_INT` | `192` | backticks |\n"
        "| SH4DER_T_BAD | 0.30 | not the header's |\n"
        "| SH4DER_T_SUM | 385 | worked out |\n"
        "| SH4DER_T_OLD | 7 | no such macro |\n"
        "| SH4DER_T_WORDS | four | not a number |\n"
        "| SH4DER_T_TWO | 2 | the #else value |\n"
        "| SH4DER_T_TEXT | v | not numeric in the header: not checked |\n"
        "| SH4DER_T_KIND | 0.06 | an enumerator in another table: not checked |\n"
        "| SH4DER_T_FN | 2 | a macro with arguments: not checked |\n"
        "A line with SH4DER_T_GONE in prose is not a row.\n"
    )
    with tempfile.TemporaryDirectory() as tmp:
        os.makedirs(os.path.join(tmp, "include"))
        with open(os.path.join(tmp, "include", "sh4der_t.h"), "w", encoding="utf-8") as f:
            f.write(header)
        with open(os.path.join(tmp, "include", "other.h"), "w", encoding="utf-8") as f:
            f.write("#define SH4DER_T_ELSEWHERE 5\n")
        with open(os.path.join(tmp, "README.md"), "w", encoding="utf-8") as f:
            f.write(readme)
        out, bad, counts, left_out, unchecked = check(tmp, exempt=[])
        text = "\n".join(out)
        expect("counts", (counts["differs"], counts["stale"], counts["unreadable"], counts["missing"]), (1, 1, 1, 1))
        expect("failures: missing is a warning", bad, 3)
        expect("missing is still listed", "MISSING (warning): 1" in text, True)
        expect("failures: --strict", check(tmp, exempt=[], strict=True)[1], 4)
        expect("rows not checked", sorted(u.split()[1] for u in unchecked), ["SH4DER_T_FN", "SH4DER_T_KIND", "SH4DER_T_TEXT"])
        expect("macros", counts["macros"], 7)
        expect("differs names BAD", "SH4DER_T_BAD  README 0.30, header 0.25  (include/sh4der_t.h:5)" in text, True)
        expect("missing names GONE", "include/sh4der_t.h:6  SH4DER_T_GONE  3" in text, True)
        expect("stale names OLD", "SH4DER_T_OLD" in text, True)
        expect("unreadable names WORDS", "SH4DER_T_WORDS  value cell 'four'" in text, True)
        expect("left out", sorted(n for n, _, _ in left_out), ["SH4DER_T_FN", "SH4DER_T_TEXT"])
        expect("a comment is not a define", "SH4DER_T_COMMENT" in text, False)
        expect("only sh4der_*.h is read", "SH4DER_T_ELSEWHERE" in text, False)
        # the three failing rows mended: only the missing macro is left, a warning
        mended = readme.replace("0.30", "0.25").replace("| SH4DER_T_OLD | 7 | no such macro |\n", "").replace("four", "4")
        with open(os.path.join(tmp, "README.md"), "w", encoding="utf-8") as f:
            f.write(mended)
        expect("warning alone passes", check(tmp, exempt=[])[1], 0)
        expect("warning alone, counted", check(tmp, exempt=[])[2]["missing"], 1)
        expect("warning alone fails --strict", check(tmp, exempt=[], strict=True)[1], 1)
        # exempt: the missing macro is neither a warning nor a --strict failure
        res = check(tmp, exempt=["SH4DER_T_GONE"], strict=True)
        expect("exempt passes --strict", (res[1], res[2]["missing"], res[2]["exempt"]), (0, 0, 1))
        # an exempt name no header defines (or defines as text) fails, strict or not
        res = check(tmp, exempt=["SH4DER_T_GONE", "SH4DER_T_NEVER", "SH4DER_T_TEXT"])
        expect("exempt out of date", (res[1], res[2]["gone"]), (2, 2))
        expect("exempt out of date is named", "SH4DER_T_NEVER" in "\n".join(res[0]), True)
        # a name the header only uses (a word in code, no macro, no enumerator) is stale, never passed over
        with open(os.path.join(tmp, "README.md"), "w", encoding="utf-8") as f:
            f.write(mended + "| SH4DER_T_USED | 1 | a word in the header's code only |\n")
        with open(os.path.join(tmp, "include", "sh4der_u.h"), "w", encoding="utf-8") as f:
            f.write("static int x = SH4DER_T_USED;\nstruct s { int SH4DER_T_FIELD; };\n")
        res = check(tmp, exempt=[])
        expect("a used word is stale", (res[1], res[2]["stale"]), (1, 1))
        os.remove(os.path.join(tmp, "include", "sh4der_u.h"))
        # a row for the missing macro: nothing left, strict or not
        with open(os.path.join(tmp, "README.md"), "w", encoding="utf-8") as f:
            f.write(mended.replace("A line with", "| SH4DER_T_GONE | 3 | now a row |\nA line with"))
        expect("mended", check(tmp, exempt=[], strict=True)[1], 0)
    names = [n for _, group in EXEMPT for n in group.split()]
    expect("EXEMPT has no name twice", len(names), len(set(names)))
    expect("EXEMPT names are SH4DER_ macros", all(re.fullmatch(r"SH4DER_\w+", n) for n in names), True)
    print("check_tuning self-test: %s" % ("ok" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv):
    if argv and argv[0] == "--self-test":
        return self_test()
    verbose = "--verbose" in argv
    strict = "--strict" in argv
    argv = [a for a in argv if a not in ("--verbose", "--strict")]
    root = argv[0] if argv else ROOT
    if not os.path.isfile(os.path.join(root, "README.md")):
        print("error: no README.md in %s" % root, file=sys.stderr)
        return 1
    out, bad, counts, left_out, unchecked = check(root, strict=strict)
    print("\n".join(out))
    if verbose:
        print("ROWS NOT CHECKED: %d" % len(unchecked))
        for u in unchecked:
            print("  " + u)
        print("NOT NUMERIC (left out): %d" % len(left_out))
        for name, rel, line in left_out:
            print("  %s:%d  %s" % (rel, line, name))
    print("check_tuning: %d numeric macros (%d exempt, %d more left out as not numeric), %d table rows"
          " (%d not checked: enumerators or not numbers); %d differ, %d stale, %d unreadable, %d exempt out of date;"
          " %d missing a table row (%s): %s"
          % (counts["macros"], counts["exempt"], len(left_out), counts["rows"], counts["unchecked"],
             counts["differs"], counts["stale"], counts["unreadable"], counts["gone"], counts["missing"],
             "fails" if strict else "warning", "FAILED" if bad else "OK"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
