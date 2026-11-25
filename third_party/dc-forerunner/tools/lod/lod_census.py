#!/usr/bin/env python3
"""LOD census: every model on every map, every tier, checked against the LOD rules. Pure Python, reads OBJ files, writes nothing but stdout / CSV.

NEW in this repository (not a port). The Halo DC port's census (F-LOD-ALL-TIERS, #376) was a scratch script whose output lived in a
note, not in the repository, so it is rewritten here from its rules (docs/archive/graphics/LOD_CENSUS_376.md in the Halo DC port).

Layout read:   <root>/<map>/<model>/<tier>.obj      (one OBJ per tier; a missing tier file means the tier does not exist for that model)
Tier names:    --tiers near,mid,far,xfar,sxfar   (default; the first name is the near mesh the others are measured against)
Placements:    --placements FILE.csv  rows "map,model,count": census only the (map, model) pairs that are placed (count is printed, not used
               to decide); without it every map folder and model folder under root is censused.

Rules checked (each printed as a FLAG row; --strict makes any flag exit 1):
  MISSING FAR       the near exists and the far tier does not (on a placed model, a gap; the rule is "every model has a real far cut")
  FAR OVER 45%      far triangles or positions above --far-share (default 0.45) of the near's (the far cut must be clearly lighter)
  FAR NOT LIGHTER   far triangles >= near triangles (Halo's own far was heavier than the near on some models: this must not pass)
  OVER BAR          a tier other than the near with more triangles than --bar (default 600: the port's character bar; "1,000 triangles is not light")
Every tier also prints its share of the screen budget: triangles / --screen (default 6000 on screen, 30 fps).

Counts: triangles are the faces of the OBJ fan-triangulated (a quad is two triangles); positions are the distinct `v` lines of the file.
The game's vertex count (position + uv + normal per corner) is NOT computed here: TODO(SOURCE) the source engine mesh reader's vertex rule, then add
it as a column. Until then the far-share check uses positions as the vertex proxy.

Use:  python lod_census.py --root <folder with map/model/tier.obj> [--placements placements.csv] [--csv out.csv] [--strict]
Self test (no files): python lod_census.py --selftest
"""
import argparse
import csv
import os
import sys

DEFAULT_TIERS = ("near", "mid", "far", "xfar", "sxfar")
FAR_SHARE = 0.45
BAR = 600
SCREEN = 6000


def count_obj_text(text):
    """-> (triangles, positions) of an OBJ text. Faces with more than 3 corners are fan-triangulated; negative indices are relative."""
    positions = 0
    tris = 0
    for line in text.splitlines():
        if line.startswith("v ") or line == "v":
            positions += 1
        elif line.startswith("f "):
            corners = len(line.split()) - 1
            if corners >= 3:
                tris += corners - 2
    return tris, positions


def count_obj(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return count_obj_text(f.read())


def scan(root, tiers, placements=None):
    """-> rows (map, model, tier, triangles, positions, file) for every model folder under root (or only the placed pairs)."""
    rows = []
    wanted = set(placements) if placements else None
    for map_id in sorted(os.listdir(root)):
        map_dir = os.path.join(root, map_id)
        if not os.path.isdir(map_dir):
            continue
        for model in sorted(os.listdir(map_dir)):
            model_dir = os.path.join(map_dir, model)
            if not os.path.isdir(model_dir):
                continue
            if wanted is not None and (map_id, model) not in wanted:
                continue
            for tier in tiers:
                path = os.path.join(model_dir, tier + ".obj")
                if os.path.isfile(path):
                    tris, pos = count_obj(path)
                    rows.append((map_id, model, tier, tris, pos, path))
                else:
                    rows.append((map_id, model, tier, None, None, None))
    return rows


def read_placements(path):
    """map,model,count rows -> {(map, model): count}. A header row is skipped."""
    out = {}
    with open(path, encoding="utf-8", newline="") as f:
        for row in csv.reader(f):
            if len(row) < 2 or row[0].strip().lower() in ("map", "#map", ""):
                continue
            out[(row[0].strip(), row[1].strip())] = int(row[2]) if len(row) > 2 and row[2].strip().isdigit() else 0
    return out


def check(rows, tiers, far_share=FAR_SHARE, bar=BAR, screen=SCREEN):
    """-> (table rows, flags). A table row per (map, model): the tiers' triangles / positions and the far share."""
    by_model = {}
    for map_id, model, tier, tris, pos, path in rows:
        by_model.setdefault((map_id, model), {})[tier] = (tris, pos)
    table, flags = [], []
    near_name = tiers[0]
    far_name = "far" if "far" in tiers else None
    for (map_id, model), tiers_here in sorted(by_model.items()):
        near = tiers_here.get(near_name, (None, None))
        far = tiers_here.get(far_name, (None, None)) if far_name else (None, None)
        share_t = share_p = None
        if near[0] and far[0] is not None:
            share_t = far[0] / near[0]
            share_p = far[1] / near[1] if near[1] else None
        row = {"map": map_id, "model": model}
        for tier in tiers:
            t = tiers_here.get(tier, (None, None))
            row[tier + "_tris"] = t[0]
            row[tier + "_pos"] = t[1]
        row["far_share_tris"] = None if share_t is None else round(share_t, 3)
        row["far_share_pos"] = None if share_p is None else round(share_p, 3)
        table.append(row)
        if near[0] is None:
            continue
        if far_name and far[0] is None:
            flags.append((map_id, model, "MISSING FAR", "no %s.obj beside %s.obj" % (far_name, near_name)))
        elif share_t is not None:
            if share_t >= 1.0:
                flags.append((map_id, model, "FAR NOT LIGHTER", "far %d t vs near %d t" % (far[0], near[0])))
            if share_t > far_share or (share_p is not None and share_p > far_share):
                flags.append((map_id, model, "FAR OVER %d%%" % round(far_share * 100),
                              "far %s of near triangles, %s of near positions" % (
                                  "%d%%" % round(share_t * 100), "%d%%" % round((share_p or 0) * 100))))
        for tier in tiers:
            t = tiers_here.get(tier, (None, None))
            if t[0] is not None and t[0] > bar and tier != near_name:
                flags.append((map_id, model, "OVER BAR", "%s %d t (bar %d t)" % (tier, t[0], bar)))
    return table, flags


def print_report(table, flags, tiers, screen):
    head = ["map", "model"] + [c for t in tiers for c in (t + " t", t + " pos")] + ["far/near t", "far/near pos"]
    print("%-6s %-22s " % ("map", "model") + " ".join("%10s" % h for h in head[2:]))
    for r in table:
        cells = []
        for t in tiers:
            for key in (t + "_tris", t + "_pos"):
                v = r[key]
                cells.append("%10s" % ("-" if v is None else v))
        fs = r["far_share_tris"]
        fp = r["far_share_pos"]
        cells.append("%10s" % ("-" if fs is None else "%d%%" % round(fs * 100)))
        cells.append("%10s" % ("-" if fp is None else "%d%%" % round(fp * 100)))
        print("%-6s %-22s " % (r["map"], r["model"]) + " ".join(cells))
    print()
    near = tiers[0]
    shares = [(r["map"], r["model"], r[near + "_tris"]) for r in table if r[near + "_tris"]]
    if shares:
        worst = max(shares, key=lambda s: s[2])
        print("near screen budget: %d triangles = %d%% of %d on screen (largest near: %s on %s)" % (
            worst[2], round(100.0 * worst[2] / screen), screen, worst[1], worst[0]))
    print("FLAGS: %d" % len(flags))
    for map_id, model, what, detail in flags:
        print("  %-6s %-22s %-16s %s" % (map_id, model, what, detail))


def selftest():
    cube = "\n".join(["v 0 0 0", "v 1 0 0", "v 1 1 0", "v 0 1 0", "f 1 2 3 4"])
    assert count_obj_text(cube) == (2, 4), count_obj_text(cube)
    assert count_obj_text("v 0 0 0\nf -1 -2 -3") == (1, 1)
    near = [("a10", "grunt", "near", 1000, 800, "p"), ("a10", "grunt", "far", 900, 700, "p")]
    t, flags = check(near, ("near", "far"))
    assert any(f[2].startswith("FAR OVER") for f in flags), flags
    ok = [("a10", "x", "near", 500, 400, "p"), ("a10", "x", "far", 200, 150, "p")]
    t, flags = check(ok, ("near", "far"))
    assert flags == [], flags
    gap = [("a10", "y", "near", 500, 400, "p")]
    t, flags = check(gap, ("near", "far"))
    assert flags and flags[0][2] == "MISSING FAR", flags
    print("selftest OK")
    return 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--root", help="folder with <map>/<model>/<tier>.obj")
    ap.add_argument("--tiers", default=",".join(DEFAULT_TIERS))
    ap.add_argument("--placements", help="CSV map,model,count: census only these pairs")
    ap.add_argument("--far-share", type=float, default=FAR_SHARE)
    ap.add_argument("--bar", type=int, default=BAR)
    ap.add_argument("--screen", type=int, default=SCREEN)
    ap.add_argument("--csv", help="write the table to this CSV path")
    ap.add_argument("--strict", action="store_true", help="exit 1 when any flag is raised")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    if not args.root:
        ap.error("--root is required (or --selftest)")
    tiers = tuple(t.strip() for t in args.tiers.split(",") if t.strip())
    placements = read_placements(args.placements) if args.placements else None
    rows = scan(args.root, tiers, placements)
    table, flags = check(rows, tiers, args.far_share, args.bar, args.screen)
    print_report(table, flags, tiers, args.screen)
    if args.csv:
        with open(args.csv, "w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(table[0].keys()) if table else ["map", "model"])
            w.writeheader()
            w.writerows(table)
    return 1 if (args.strict and flags) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
