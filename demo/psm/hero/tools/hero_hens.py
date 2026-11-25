#!/usr/bin/env python3
"""SH4DER-PVR: hero1, the town's hen template for the room: copies the hen's arrays and its colours out of the town's
generated mesh header into a small one.  Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

    python demo/psm/hero/tools/hero_hens.py <build>/psm_town/psm_town_mesh.h <out dir>   writes <out dir>/hero_hens.h

The town's header is a build product (tools/bake_town.py); so is this one. Nothing is changed: the same numbers,
the colours renamed hero_hen_pal (the palette entries the hen uses, in order).
"""
import os
import re
import sys

NAMES = ("psm_town_hen_v", "psm_town_hen_n", "psm_town_hen_c", "psm_town_hen_sv", "psm_town_hen_sl", "psm_town_hen_cv", "psm_town_hen_ct")


def block(src, name):
    m = re.search(r"static const [^\n]*\b%s\[[^\n]*=\s*\{.*?\};" % name, src, re.S)
    if not m:
        raise SystemExit("no %s in the town's header" % name)
    return m.group(0)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src = open(sys.argv[1], encoding="utf-8").read()
    defs = re.findall(r"#define PSM_TOWN_HEN_\w+ \d+u", src)
    pal = block(src, "psm_town_pal")
    rows = re.findall(r"\{\s*([-0-9.ef]+)\s*,\s*([-0-9.ef]+)\s*,\s*([-0-9.ef]+)\s*\}", pal)
    used = sorted({int(x) for x in re.findall(r"\d+", block(src, "psm_town_hen_c").split("{", 1)[1])})
    out = ["/* hero1: the town's hen template, copied by demo/psm/hero/tools/hero_hens.py from %s. A build product. */"
           % os.path.basename(sys.argv[1]), "#ifndef HERO_HENS_H", "#define HERO_HENS_H"] + defs
    out += [block(src, n) for n in NAMES]
    out.append("#define HERO_HEN_PAL0 %du   /* the first palette entry the hen uses */" % used[0])
    out.append("static const float hero_hen_pal[%d][3] = {" % (used[-1] - used[0] + 1))
    out += ["    { %s, %s, %s }," % rows[i] for i in range(used[0], used[-1] + 1)]
    out += ["};", "#endif", ""]
    os.makedirs(sys.argv[2], exist_ok=True)
    with open(os.path.join(sys.argv[2], "hero_hens.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))
    print("hero_hens.h: %d defines, palette %d .. %d" % (len(defs), used[0], used[-1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
