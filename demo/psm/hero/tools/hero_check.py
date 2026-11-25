#!/usr/bin/env python3
"""SH4DER-PVR: hero1, check a HERO.BIN by itself (format: demo/psm/hero/README.md).  Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

    python demo/psm/hero/tools/hero_check.py <file>      prints the counts and HERO_OK, or each problem and exits 1
    python demo/psm/hero/tools/hero_check.py --self-test  checks the checker on a small file it makes and spoils
"""
import math
import struct
import sys

BONES, V_MAX, I_MAX, S_MAX, F_MAX, TEX_MAX = 33, 2048, 8192, 1024, 64, 128 * 128


def pad(n):
    return (n + 3) & ~3


def check(data):
    """The problems found, [] if none, and the counts."""
    bad = []
    if len(data) < 128:
        return ["shorter than the header"], {}
    h = struct.unpack_from("<32I", data, 0)
    if h[0] != 0x4F524548 or h[1] != 1:
        return ["not a HERO version 1 file"], {}
    bones, vn, ni, ns, vs0, clips, fps, f0, f1 = h[2], h[3], h[4], h[5], h[6], h[7], h[8], h[9], h[10]
    if bones != BONES: bad.append("bones %d, want %d" % (bones, BONES))
    if not 3 <= vn < V_MAX: bad.append("vertices %d out of 3 .. %d" % (vn, V_MAX - 1))
    if not 3 <= ni < I_MAX: bad.append("indices %d out of 3 .. %d" % (ni, I_MAX - 1))
    if not 1 <= ns < S_MAX: bad.append("strips %d out of 1 .. %d" % (ns, S_MAX - 1))
    if vs0 > ns: bad.append("first visor strip %d past the strips %d" % (vs0, ns))
    if clips != 2 or fps != 30: bad.append("clips %d fps %d, want 2 and 30" % (clips, fps))
    for c, f in enumerate((f0, f1)):
        if not 2 <= f <= F_MAX: bad.append("clip %d frames %d out of 2 .. %d" % (c, f, F_MAX))
    tex = []
    for t in range(3):
        w, hh, fmt = h[11 + 3 * t], h[12 + 3 * t], h[13 + 3 * t]
        ok = all(8 <= x <= 1024 and x & (x - 1) == 0 for x in (w, hh)) and w * hh <= TEX_MAX and fmt in (0, 1)
        if not ok: bad.append("texture %d: %d x %d format %d" % (t, w, hh, fmt))
        tex.append((w, hh))
    if any(x not in (0,) for x in h[20:32]): bad.append("header words 20 .. 31 not 0")
    if bad:
        return bad, {}
    sizes = [("parent", pad(BONES)), ("head", 12 * BONES), ("position", 12 * vn), ("normal", 12 * vn), ("uv", 8 * vn),
             ("bone0", pad(vn)), ("bone1", pad(vn)), ("weight", pad(vn)), ("index", pad(2 * ni)), ("length", pad(2 * ns)),
             ("clip0", 48 * BONES * f0), ("clip1", 48 * BONES * f1)] + [("tex%d" % t, pad(2 * w * hh)) for t, (w, hh) in enumerate(tex)]
    want = 128 + sum(s for _, s in sizes)
    if len(data) != want:
        return ["size %d bytes, the header says %d" % (len(data), want)], {}
    at, blk = 128, {}
    for name, s in sizes:
        blk[name] = at
        at += s
    parent = data[blk["parent"]:blk["parent"] + BONES]
    for i, p in enumerate(parent):
        if p != 255 and p >= i: bad.append("bone %d's parent %d does not come first" % (i, p))
    fl = lambda name, n: struct.unpack_from("<%df" % n, data, blk[name])
    pos, nrm, uv = fl("position", 3 * vn), fl("normal", 3 * vn), fl("uv", 2 * vn)
    if not all(math.isfinite(x) and abs(x) < 100.0 for x in pos): bad.append("a position not finite or past 100 m")
    for i in range(vn):
        l2 = sum(x * x for x in nrm[3 * i:3 * i + 3])
        if not 0.98 < l2 < 1.02:
            bad.append("normal %d length^2 %.3f" % (i, l2))
            break
    if not all(math.isfinite(x) and -0.01 <= x <= 1.01 for x in uv): bad.append("a uv outside 0 .. 1")
    b0, b1 = data[blk["bone0"]:blk["bone0"] + vn], data[blk["bone1"]:blk["bone1"] + vn]
    if max(b0) >= BONES or max(b1) >= BONES: bad.append("a vertex bone past 32")
    if any((b0[i], b1[i]) < (b0[i - 1], b1[i - 1]) for i in range(1, vn)): bad.append("vertices not sorted by (bone 0, bone 1)")
    idx = struct.unpack_from("<%dH" % ni, data, blk["index"])
    if max(idx) >= vn: bad.append("an index past the vertices")
    sl = struct.unpack_from("<%dH" % ns, data, blk["length"])
    if min(sl) < 3: bad.append("a strip shorter than 3")
    if sum(sl) != ni: bad.append("strip lengths sum %d, indices %d" % (sum(sl), ni))
    for c, f in enumerate((f0, f1)):
        m = fl("clip%d" % c, 12 * BONES * f)
        if not all(math.isfinite(x) and abs(x) < 100.0 for x in m): bad.append("clip %d: a matrix value not finite or past 100" % c); continue
        for k in range(0, len(m), 12):
            rows = [m[k + 4 * r:k + 4 * r + 3] for r in range(3)]
            det = (rows[0][0] * (rows[1][1] * rows[2][2] - rows[1][2] * rows[2][1]) - rows[0][1] * (rows[1][0] * rows[2][2] - rows[1][2] * rows[2][0])
                   + rows[0][2] * (rows[1][0] * rows[2][1] - rows[1][1] * rows[2][0]))
            if not 0.5 < det < 2.0:
                bad.append("clip %d frame %d bone %d: turn's determinant %.3f (not a turn)" % (c, k // 12 // BONES, k // 12 % BONES, det))
                break
    tris = sum(n - 2 for n in sl)
    return bad, {"vertices": vn, "triangles": tris, "strips": ns, "visor strips": ns - vs0, "TA vertices a pass": ni,
                 "frames": (f0, f1), "textures": tex, "bytes": len(data)}


def self_test():
    """A small valid file: OK; then each spoilt copy: refused."""
    vn, ni, ns = 4, 4, 1
    hd = [0x4F524548, 1, 33, vn, ni, ns, 0, 2, 30, 2, 2, 8, 8, 0, 8, 8, 0, 8, 8, 0] + [0] * 12
    ident = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0]
    body = struct.pack("<32I", *hd) + bytes([255] + list(range(32))) + b"\0" * 3 + struct.pack("<99f", *([0.0] * 99))
    body += struct.pack("<12f", *([0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0])) + struct.pack("<12f", *([0, 0, 1] * 4)) + struct.pack("<8f", *([0.5] * 8))
    body += bytes([0, 0, 1, 1]) + bytes([0, 0, 1, 1]) + bytes([255] * 4) + struct.pack("<4H", 0, 1, 2, 3) + struct.pack("<H", 4) + b"\0\0"
    body += struct.pack("<%df" % (12 * 33 * 4), *(ident * 33 * 4)) + b"\0" * (3 * 128)
    good = bytearray(body)
    assert check(bytes(good))[0] == [], check(bytes(good))[0]
    at_len = 128 + 36 + 396 + 12 * vn + 12 * vn + 8 * vn + 3 * 4 + 2 * ni   # the strip length block
    spoil = [(0, b"XERO"), (12, struct.pack("<I", 5000)), (at_len, struct.pack("<H", 9))]
    for at, b in spoil:
        x = bytearray(good)
        x[at:at + len(b)] = b
        assert check(bytes(x))[0], "spoilt at %d passed" % at
    assert check(bytes(good[:-2]))[0], "a short file passed"
    print("hero_check self-test: OK")


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    if sys.argv[1] == "--self-test":
        self_test()
        return 0
    with open(sys.argv[1], "rb") as f:
        bad, counts = check(f.read())
    for k, v in counts.items():
        print("%s: %s" % (k, v))
    for b in bad:
        print("PROBLEM: %s" % b)
    print("HERO_OK" if not bad else "HERO_BAD")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
