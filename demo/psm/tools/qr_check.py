#!/usr/bin/env python3
"""SH4DER-PVR: check what was read from the QR test image (psm-qr-q1), and decode its codes on the host.

demo/psm/tools/qr_check.py
Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

The test image (demo/psm/qr) shows 16 QR codes of known payloads at 10, 8, 6 and 4 pixels a module. A payload is

    q1 <id> <crc>|<data>      id: version-pixels-letter, crc: CRC-32 (zip's) of data, 8 hex digits

Three uses:

    python demo/psm/tools/qr_check.py read.txt
        read.txt holds what a phone read, one decoded string a line, in any order (other lines are ignored).
        Each string is checked against its own CRC and against the payload this script makes again from the id.
        The table at the end says which of the 16 codes were read, by version and module size.

    python demo/psm/tools/qr_check.py --photos <file or folder> ...
        The same, with the strings read from photographs by OpenCV's QR detector. Needs the cv2 module (it is not
        part of this repository and is not needed for the other uses). A code OpenCV does not read may still read
        on a phone, and the other way round: say which reader gave a result.

    python demo/psm/tools/qr_check.py --matrices qr_matrices.txt
        The host test's dump of every code's modules (demo/psm/qr/test/qr_test.c), decoded here: format bits, mask,
        the codewords in their zigzag, the blocks, a Reed-Solomon check of every block (all syndromes zero), the
        byte segment. This decoder is written from the standard's layout and shares no code with the encoder.

--self-test checks the CRC and the payload generator against values fixed here.
"""
import os
import re
import sys
import zlib

ALNUM = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
SETS = [(3, 10, 1), (3, 8, 1), (5, 8, 1), (3, 6, 2), (5, 6, 1), (3, 4, 6), (5, 4, 4)]      # version, pixels, codes
CAPACITY = {3: 42, 5: 84}                                                                # bytes, level M, byte mode
BLOCKS = {(3, "M"): (1, 26), (5, "M"): (2, 24)}                                          # blocks, check codewords a block
ALIGN = {3: 22, 5: 30}


def payload(ver, px, k):
    """The payload of code k of the set (ver, px): the generator of demo/psm/qr/qr_page.h."""
    ident = "%u-%02u-%c" % (ver, px, ord("a") + k)
    r = zlib.crc32(ident.encode()) & 0xFFFFFFFF
    data = ""
    for _ in range(CAPACITY[ver] - 19):
        r = (r * 1103515245 + 12345) & 0xFFFFFFFF
        data += ALNUM[(r >> 16) % 62]
    return "q1 %s %08x|%s" % (ident, zlib.crc32(data.encode()) & 0xFFFFFFFF, data)


def all_payloads():
    return {payload(v, p, k)[3:9]: payload(v, p, k) for v, p, n in SETS for k in range(n)}


def judge(text):
    """(id, verdict) for one decoded string; id is None when it is not one of ours."""
    m = re.match(r"^q1 (\d-\d\d-[a-z]) ([0-9a-f]{8})\|([0-9A-Za-z]+)$", text)
    if not m:
        return None, "not a q1 payload"
    ident, crc, data = m.groups()
    if zlib.crc32(data.encode()) & 0xFFFFFFFF != int(crc, 16):
        return ident, "CRC DOES NOT AGREE"
    want = all_payloads().get(ident)
    if want is None:
        return ident, "no such code in the set"
    return ident, "ok" if text == want else "CRC agrees but NOT THE SET'S PAYLOAD"


def report(found):
    """found: {id: verdict}. Prints the table; returns the number of codes read right."""
    good = 0
    print("version  px  codes read right")
    for v, p, n in SETS:
        ids = ["%u-%02u-%c" % (v, p, ord("a") + k) for k in range(n)]
        ok = [i for i in ids if found.get(i) == "ok"]
        bad = ["%s (%s)" % (i, found[i]) for i in ids if i in found and found[i] != "ok"]
        good += len(ok)
        print("   %u     %2u  %u of %u%s" % (v, p, len(ok), n, ("   " + "; ".join(bad)) if bad else ""))
    print("%u of 16 codes read right" % good)
    return good


# ---- a decoder for a module matrix (rows of 0 and 1, 1 is dark) ----

def gf_tables():
    exp, log, x = [0] * 512, [0] * 256, 1
    for i in range(255):
        exp[i], log[x] = x, i
        x <<= 1
        if x & 0x100:
            x ^= 0x11D
    for i in range(255, 512):
        exp[i] = exp[i - 255]
    return exp, log


EXP, LOG = gf_tables()


def syndromes_zero(block, ncheck):
    for i in range(ncheck):                 # the block as a polynomial, at alpha^i
        acc = 0
        for c in block:
            acc = (EXP[LOG[acc] + i] if acc else 0) ^ c
        if acc:
            return False
    return True


def decode_matrix(ver, rows):
    """The text of a code from its modules. Raises ValueError with the reason when a step fails."""
    n = 17 + 4 * ver
    if len(rows) != n or any(len(r) != n for r in rows):
        raise ValueError("not %u x %u modules" % (n, n))
    dark = lambda x, y: rows[y][x] == "1"
    # the format bits beside the top-left finder: 15 bits, masked with 101010000010010
    pos = [(8, i) for i in range(6)] + [(8, 7), (8, 8), (7, 8)] + [(14 - i, 8) for i in range(9, 15)]
    bits = 0
    for i, (x, y) in enumerate(pos):
        bits |= dark(x, y) << i
    bits ^= 0x5412
    data, rem = bits >> 10, bits >> 10
    for _ in range(10):
        rem = (rem << 1) ^ ((rem >> 9) * 0x537)
    if (data << 10 | rem) != bits:
        raise ValueError("format bits fail their check")
    level, mask = "MLHQ"[data >> 3], data & 7
    if (ver, level) not in BLOCKS:
        raise ValueError("level %s is not in this script's table" % level)

    def function(x, y):
        if (x < 9 and y < 9) or (x >= n - 8 and y < 9) or (x < 9 and y >= n - 8):
            return True                     # finders, separators, format bits, the dark module
        if x == 6 or y == 6:
            return True                     # timing
        a = ALIGN[ver]
        return abs(x - a) <= 2 and abs(y - a) <= 2

    masks = [lambda i, j: (i + j) % 2 == 0, lambda i, j: i % 2 == 0, lambda i, j: j % 3 == 0, lambda i, j: (i + j) % 3 == 0,
             lambda i, j: (i // 2 + j // 3) % 2 == 0, lambda i, j: i * j % 2 + i * j % 3 == 0,
             lambda i, j: (i * j % 2 + i * j % 3) % 2 == 0, lambda i, j: ((i + j) % 2 + i * j % 3) % 2 == 0]
    stream, right, up = [], n - 1, True
    while right >= 1:
        if right == 6:
            right = 5
        for v in range(n):
            y = n - 1 - v if up else v
            for x in (right, right - 1):
                if not function(x, y):
                    stream.append(dark(x, y) ^ masks[mask](y, x))
        right -= 2
        up = not up
    nblocks, ncheck = BLOCKS[(ver, level)]
    total = len(stream) // 8
    words = [sum(stream[8 * i + b] << (7 - b) for b in range(8)) for i in range(total)]
    ndata = total // nblocks - ncheck
    if total % nblocks:
        raise ValueError("blocks of unequal length are not in this script")
    blocks = [[words[i * nblocks + b] for i in range(ndata)] + [words[ndata * nblocks + i * nblocks + b] for i in range(ncheck)] for b in range(nblocks)]
    for b, block in enumerate(blocks):
        if not syndromes_zero(block, ncheck):
            raise ValueError("block %u fails its Reed-Solomon check" % b)
    msg = [c for block in blocks for c in block[:ndata]]
    bitstr = "".join("{:08b}".format(c) for c in msg)
    if bitstr[:4] != "0100":
        raise ValueError("not a byte segment")
    count = int(bitstr[4:12], 2)
    return bytes(int(bitstr[12 + 8 * i:20 + 8 * i], 2) for i in range(count)).decode("latin-1"), level, mask


def check_matrices(path):
    with open(path, encoding="latin-1") as f:
        lines = [l.rstrip("\n") for l in f]
    i, bad, found = 0, 0, {}
    while i < len(lines):
        if not lines[i].startswith("CODE "):
            i += 1
            continue
        _, ver, m, want = lines[i].split(" ", 3)
        ver, m = int(ver), int(m)
        rows = lines[i + 1:i + 1 + m]
        i += 1 + m
        try:
            text, level, mask = decode_matrix(ver, rows)
        except ValueError as e:
            print("%s: %s" % (want[3:9], e))
            bad += 1
            continue
        ident, verdict = judge(text)
        if text != want or level != "M" or verdict != "ok":
            print("%s: decoded %r, level %s, %s" % (want[3:9], text, level, verdict))
            bad += 1
            continue
        found[ident] = verdict
        print("%s: ok (level %s, mask %u, %u bytes)" % (ident, level, mask, len(text)))
    good = report(found)
    return 0 if not bad and good == 16 else 1


def check_strings(strings):
    found = {}
    for s in strings:
        ident, verdict = judge(s.strip())
        if ident is None:
            continue
        if found.get(ident) != "ok":
            found[ident] = verdict
    report(found)
    return 0


def photo_strings(paths):
    try:
        import cv2
    except ImportError:
        raise SystemExit("--photos needs the cv2 module (OpenCV); it is not installed for this Python")
    files = []
    for p in paths:
        if os.path.isdir(p):
            files += [os.path.join(p, f) for f in sorted(os.listdir(p)) if f.lower().endswith((".jpg", ".jpeg", ".png"))]
        else:
            files.append(p)
    det, out = cv2.QRCodeDetector(), []
    for f in files:
        img = cv2.imread(f)
        if img is None:
            print("%s: not an image" % f)
            continue
        # One pass at the photo's own size misses most codes in a 12-megapixel photo of a television (7 of 16 in the
        # first console run, 16 of 16 with the passes below): grey, four sizes, three blurs, plain and Otsu's threshold.
        grey, texts = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY), set()
        for scale in (1.0, 0.5, 0.33, 0.25):
            small = cv2.resize(grey, None, fx=scale, fy=scale, interpolation=cv2.INTER_AREA)
            for blur in (0, 3, 7):
                soft = cv2.GaussianBlur(small, (blur, blur), 0) if blur else small
                for cut in (False, True):
                    x = cv2.threshold(soft, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)[1] if cut else soft
                    try:
                        ok, got, _, _ = det.detectAndDecodeMulti(x)
                    except cv2.error:
                        ok = False
                    if ok:
                        texts |= {t for t in got if t}
        texts = sorted(texts)
        print("%s: %u read%s" % (os.path.basename(f), len(texts), "".join(" " + t[3:9] for t in texts if t.startswith("q1 "))))
        out += texts
    return out


def self_test():
    ok = zlib.crc32(b"123456789") == 0xCBF43926
    p = all_payloads()
    ok = ok and len(p) == 16 and all(len(v) == CAPACITY[int(k[0])] and judge(v) == (k, "ok") for k, v in p.items())
    ok = ok and judge(p["3-08-a"][:-1] + ("0" if p["3-08-a"][-1] != "0" else "1"))[1] == "CRC DOES NOT AGREE"
    ok = ok and syndromes_zero([0] * 10, 4) and not syndromes_zero([0, 0, 1, 0], 2)
    print("qr_check self-test: %s" % ("ok" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv):
    if len(argv) >= 2 and argv[1] == "--self-test":
        return self_test()
    if len(argv) >= 3 and argv[1] == "--matrices":
        return check_matrices(argv[2])
    if len(argv) >= 3 and argv[1] == "--photos":
        return check_strings(photo_strings(argv[2:]))
    if len(argv) == 2:
        with open(argv[1], encoding="latin-1") as f:
            return check_strings(f.read().split("\n"))
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
