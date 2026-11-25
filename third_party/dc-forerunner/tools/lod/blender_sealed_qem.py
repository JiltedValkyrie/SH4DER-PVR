"""Sealed quadric decimation of a welded triangle mesh (runs INSIDE Blender; numpy is Blender's). The Halo DC port's sealed cut as functions.

Every object of the source OBJ is a part; all parts are welded by position into one triangle mesh (each face keeps its part and its per corner
UV), so neighbouring parts and neighbouring UV charts share their border vertices and a cut can never open a crack between them. Quadric half-edge
collapse, the kept vertex keeps its position; a moved face takes its new corner UV from its own affine texture map, and that extension must land
on a removed face's UV at the kept vertex (same affine chart, tiling wrap allowed), so the texture stays where it was (slivers under SLIVER
wu^2 excepted). Rules:
- verts of a part not being cut, and verts on an open or a non-manifold edge, never move (outline, windows and doors kept, no cracks);
- a part border edge may only be collapsed along itself, from a vertex with exactly 2 border edges;
- a vertex with no border edge may collapse along any edge; the others are fixed;
- link condition, no face flip (normals within 60 degrees), and no turn of the front side (the engine's winding rule: TODO(SOURCE) below);
- no small connected piece (within its part, at most `piece_small` triangles) below `piece_floor` of its triangles, no piece below `piece_min`
  (thin straps and belts would otherwise collapse away first: their two sides' planes agree, so their quadric error is small);
- no collapse that loses more than `area_tol` of the surface around the moved vertex (a flat region keeps its area exactly);
- stop at `ratio` of the cut parts' triangles or when the error passes `maxerr` (model units);
- `fixed` vertices never move: sealed_cut runs holes() after a cut, fixes the vertices round each spot where the cut shows sky and the source a
  surface, and cuts again (`near`: a spot with source sky close by is a moving outline, not a hole: a character's silhouette);
- `vgroup`: a vertex collapses only into a vertex of its own group (a skinned part's bone, so every kept vertex keeps its skin weight).

Ported from the Halo DC port's tools/assets/blender_sealed_qem.py (MIT, same project owner). Functions that assume Halo's file formats or import
rules carry a TODO(SOURCE) comment: they need the source engine static / skeletal mesh reader and the engine's winding rule before use. Names of Halo-side
callers (blender_cut_*.py, import_3d) are kept only to mark where they were used; those scripts are not in this tree.
"""
import collections
import heapq

import numpy as np

LINE_W = 10.0
SEAM_W = 1.0


def build(objs, weld):
    """-> P (n,3) welded positions, faces [[a,b,c]], fuv [[uv,uv,uv]], fpart [part index], fnrm [[n,n,n]] (the source's corner normals:
    the game winds and culls each triangle by them, the engine winding rule); fan-triangulated."""
    key, P, faces, fuv, fch, fnrm = {}, [], [], [], [], []
    for ci, ob in enumerate(objs):
        me = ob.data
        uvd = me.uv_layers.active.data if me.uv_layers.active else None
        cn = [tuple(c.vector) for c in me.corner_normals]
        remap = []
        for v in me.vertices:
            k = (round(v.co.x / weld), round(v.co.y / weld), round(v.co.z / weld))
            if k not in key:
                key[k] = len(P)
                P.append(tuple(v.co))
            remap.append(key[k])
        for p in me.polygons:
            vs = [remap[i] for i in p.vertices]
            uv = [tuple(uvd[li].uv) if uvd else (0.0, 0.0) for li in p.loop_indices]
            nr = [cn[li] for li in p.loop_indices]
            for j in range(1, len(vs) - 1):
                tri = [vs[0], vs[j], vs[j + 1]]
                if len(set(tri)) == 3:
                    faces.append(tri)
                    fuv.append([uv[0], uv[j], uv[j + 1]])
                    fch.append(ci)
                    fnrm.append([nr[0], nr[j], nr[j + 1]])
    return np.array(P, np.float64), faces, fuv, fch, fnrm


def normal(P, f):
    a, b, c = P[f[0]], P[f[1]], P[f[2]]
    n = np.cross(b - a, c - a)
    ln = np.linalg.norm(n)
    return n / ln if ln > 1e-12 else None, ln


def pieces(faces, fch):
    """Per face its connected piece (faces of one part sharing a vertex)."""
    parent = list(range(len(faces)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    first = {}
    for fi, f in enumerate(faces):
        for v in f:
            k = (fch[fi], v)
            if k in first:
                ra, rb = find(first[k]), find(fi)
                if ra != rb:
                    parent[ra] = rb
            else:
                first[k] = fi
    return [find(fi) for fi in range(len(faces))]


def decimate(P, faces, fuv, fch, fnrm, cut, ratio, maxerr, sliver_area, uv_tol, stats=None, piece_floor=0.25, piece_min=8, piece_small=128,
             area_tol=0.15, fixed=(), vgroup=None, mirror=None):
    """Collapse the faces of parts with cut[part] true. -> alive per face, cut triangles before, after. A moved corner takes the source
    normal at the kept vertex from the removed face it matched (same chart), so no triangle turns its front away. fixed: vertex indices
    that never move (round the holes() spots). vgroup: per vertex a group a vertex only collapses within (a skinned model's bone).
    mirror: per vertex its mirror twin (itself on the plane, -1 none: fixed); every collapse is made on both sides at once."""
    stats = stats if stats is not None else collections.Counter()
    n = len(P)
    alive = [True] * len(faces)
    piece = pieces(faces, fch)
    piece_live = collections.Counter(piece)
    piece_floor_n = {c: max(piece_min, int(k * piece_floor)) if k <= piece_small else piece_min for c, k in piece_live.items()}
    vf = [set() for _ in range(n)]
    for fi, f in enumerate(faces):
        for v in f:
            vf[v].add(fi)
    locked = np.zeros(n, bool)

    def edge_faces(a, b):
        return [fi for fi in vf[a] if b in faces[fi]]

    def corner_uv(fi, v):
        return fuv[fi][faces[fi].index(v)]

    def is_seam(a, b):
        fs = edge_faces(a, b)
        if len(fs) != 2:
            return False
        f0, f1 = fs
        return fch[f0] == fch[f1] and any(abs(corner_uv(f0, v)[0] - corner_uv(f1, v)[0]) > 1e-5 or abs(corner_uv(f0, v)[1] - corner_uv(f1, v)[1]) > 1e-5
                   for v in (a, b))

    def is_line(a, b):
        fs = edge_faces(a, b)
        return len(fs) != 2 or fch[fs[0]] != fch[fs[1]]

    def nbrs(v):
        return {w for fi in vf[v] for w in faces[fi] if w != v}

    for fi, f in enumerate(faces):
        if not cut[fch[fi]]:
            locked[f] = True
    for v in range(n):
        if not locked[v] and any(len(edge_faces(v, w)) != 2 for w in nbrs(v)):
            locked[v] = True
    for v in fixed:
        locked[v] = True
    if mirror is not None:
        for v in range(n):
            if mirror[v] < 0:
                locked[v] = True

    Q = np.zeros((n, 4, 4))
    for fi, f in enumerate(faces):
        nn, _ = normal(P, f)
        if nn is None:
            continue
        p = np.append(nn, -nn.dot(P[f[0]]))
        K = np.outer(p, p)
        for v in f:
            Q[v] += K
    for fi, f in enumerate(faces):            # border and seam edges: a plane through the edge, perpendicular to the face
        nn, _ = normal(P, f)
        if nn is None:
            continue
        for j in range(3):
            a, b = f[j], f[(j + 1) % 3]
            w = LINE_W if a < b and is_line(a, b) else SEAM_W if a < b and is_seam(a, b) else 0.0
            if w:
                d = P[b] - P[a]
                m = np.cross(d, nn)
                lm = np.linalg.norm(m)
                if lm < 1e-12:
                    continue
                m /= lm
                p = np.append(m, -m.dot(P[a]))
                K = w * np.outer(p, p)
                Q[a] += K
                Q[b] += K

    def extend_uv(fi, u, p):
        """UV at p from face fi's own affine texture map (u = the corner that moves)."""
        f, uv = faces[fi], fuv[fi]
        a, b, c = P[f[0]], P[f[1]], P[f[2]]
        e1, e2, d = b - a, c - a, p - a
        d11, d12, d22 = e1 @ e1, e1 @ e2, e2 @ e2
        den = d11 * d22 - d12 * d12
        if abs(den) < 1e-18:
            return uv[f.index(u)]
        s1 = ((d @ e1) * d22 - (d @ e2) * d12) / den
        s2 = ((d @ e2) * d11 - (d @ e1) * d12) / den
        return (uv[0][0] + s1 * (uv[1][0] - uv[0][0]) + s2 * (uv[2][0] - uv[0][0]),
                uv[0][1] + s1 * (uv[1][1] - uv[0][1]) + s2 * (uv[2][1] - uv[0][1]))

    def same_uv(a, b):
        du, dv = a[0] - b[0], a[1] - b[1]
        return abs(du - round(du)) < uv_tol and abs(dv - round(dv)) < uv_tol

    def line_edges(v):
        return [w for w in nbrs(v) if is_line(v, w)]

    def cost(u, v):
        if locked[u] or (vgroup is not None and vgroup[u] != vgroup[v]):
            return None
        le = line_edges(u)
        if le and (len(le) != 2 or v not in le):
            return None
        q = Q[u] + Q[v]
        h = np.append(P[v], 1.0)
        return max(h @ q @ h, 0.0)

    ver = [0] * n
    heap = []

    def push(v):
        for w in nbrs(v):
            for a, b in ((v, w), (w, v)):
                c = cost(a, b)
                if c is not None:
                    heapq.heappush(heap, (c, a, b, ver[a], ver[b]))

    for v in range(n):
        if not locked[v]:
            push(v)
    tcut = sum(1 for fi in range(len(faces)) if cut[fch[fi]])
    target = int(tcut * ratio)
    cur = tcut
    lim = maxerr * maxerr
    def plan(u, v):
        """The checks of collapsing u into v -> (shared, moved, side_uv, side_n), None when one fails (counted in stats)."""
        shared = edge_faces(u, v)
        if len(shared) != 2:
            stats['shared'] += 1
            return None
        if any(piece_live[piece[fi]] - sum(1 for g in shared if piece[g] == piece[fi]) < piece_floor_n[piece[fi]] for fi in shared):
            stats['floor'] += 1
            return None
        opp = {w for fi in shared for w in faces[fi] if w not in (u, v)}
        if nbrs(u) & nbrs(v) != opp:
            stats['link'] += 1
            return None
        le = line_edges(u)
        if le and (len(le) != 2 or v not in le):
            return None
        moved = [fi for fi in vf[u] if fi not in shared]
        area0 = sum(normal(P, faces[fi])[1] for fi in vf[u])   # twice the area around u, before and after (a thin strap folds flat:
        area1 = sum(normal(P, [v if w == u else w for w in faces[fi]])[1] for fi in moved)   # little error, much area)
        if area1 < (1.0 - area_tol) * area0:
            stats['area'] += 1
            return None
        side_uv, side_n = {}, {}
        targets = [corner_uv(fi, v) for fi in shared]
        target_n = [fnrm[fi][faces[fi].index(v)] for fi in shared]
        sliver = sum(normal(P, faces[fi])[1] * 0.5 for fi in shared) < sliver_area
        for fi in moved:
            side_uv[fi] = extend_uv(fi, u, P[v])
            match = [k for k, t in enumerate(targets) if same_uv(side_uv[fi], t)]
            if not sliver and not match:
                stats['uv'] += 1
                stats['reject'] += 1
                return None
            own = np.array(fnrm[fi][faces[fi].index(u)])   # of the matched (or, for a sliver, any) removed face: the normal nearest the face's own
            side_n[fi] = max((target_n[k] for k in (match or range(len(target_n)))), key=lambda t: float(own @ np.array(t)))
            n0, _ = normal(P, faces[fi])
            nf = [v if w == u else w for w in faces[fi]]
            n1, _ = normal(P, nf)
            if n0 is None or n1 is None or n0.dot(n1) < 0.5:
                stats['reject'] += 1
                return None
            # the engine winding rule winds each triangle along the sum of its corner normals: the game's front side must not turn round
            j = faces[fi].index(u)
            before = n0 @ np.sum(np.array(fnrm[fi]), axis=0)
            after = n1 @ np.sum(np.array([side_n[fi] if k == j else fnrm[fi][k] for k in range(3)]), axis=0)
            if before * after <= 0.0:
                stats['facing'] += 1
                stats['reject'] += 1
                return None
        return shared, moved, side_uv, side_n

    def apply(u, v, pl):
        nonlocal cur
        shared, moved, side_uv, side_n = pl
        for fi in shared:
            alive[fi] = False
            piece_live[piece[fi]] -= 1
            for w in faces[fi]:
                vf[w].discard(fi)
        for fi in moved:
            j = faces[fi].index(u)
            faces[fi][j] = v
            fuv[fi][j] = side_uv[fi]
            fnrm[fi][j] = side_n[fi]
            vf[v].add(fi)
        vf[u].clear()
        locked[u] = True
        Q[v] += Q[u]
        cur -= 2
        stats['done'] += 1
        ver[v] += 1
        for w in nbrs(v):
            ver[w] += 1
        push(v)
        for w in nbrs(v):
            push(w)

    while heap and cur > target:
        c, u, v, vu, vv = heapq.heappop(heap)
        if vu != ver[u] or vv != ver[v] or locked[u]:
            continue
        if c > lim:
            stats['stop_cost'] = float(c)
            break
        if mirror is None:
            pl = plan(u, v)
            if pl is not None:
                apply(u, v, pl)
            continue
        mu, mv = mirror[u], mirror[v]
        if mu == u:   # a vertex on the mirror plane collapses only along it
            if mv != v:
                stats['mirror'] += 1
                continue
            pl = plan(u, v)
            if pl is not None:
                apply(u, v, pl)
            continue
        # both sides at once or neither (the user, 2026-10-22: a cut belly plate came out lop-sided); the two neighbourhoods must not touch
        if mu < 0 or mv < 0 or locked[mu] or cost(mu, mv) is None or (vf[u] | vf[v]) & (vf[mu] | vf[mv]):
            stats['mirror'] += 1
            continue
        pl, pm = plan(u, v), plan(mu, mv)
        if pl is None or pm is None:
            stats['mirror'] += 1
            continue
        apply(u, v, pl)
        apply(mu, mv, pm)
    return alive, tcut, cur


def sealed_cut(P, faces0, fuv0, fpart, fnrm0, cutp, spec, fixed=(), vgroup=None, mirror=None):
    """The sealed cut with its hole loop: decimate, snap_corners, then (spec "holes") look from cameras round the cut parts as the game draws
    the model; where the source showed a surface and the cut shows sky, fix the vertices within `radius` and cut again (at most `rounds`).
    spec: ratio, maxerr, sliver, uv_tol, holes {step, rays, clear, margin, radius, rounds, near}; opt-in collapse limits, decimate's own defaults
    when absent: collapse_area_tol (0.15), piece_floor (0.25), piece_min (8). NOT the spec's old "area_tol" key, which sealed_cut never read and the
    existing cuts still carry (Halo-side cut tables, 0.3), so no existing cut changes. -> faces, fuv, fnrm, alive (the source lists are
    left as they were), cut triangles before and after, stats, report {cameras, rounds, fixed, holes_left}."""
    from mathutils.kdtree import KDTree
    hc = spec.get("holes")
    if hc:
        soup0 = game_soup(P, faces0, fnrm0, [True] * len(faces0))
        cv = sorted({v for fi, f in enumerate(faces0) if cutp[fpart[fi]] for v in f})
        cams = view_cameras(soup0, P[cv].min(0) - hc["margin"], P[cv].max(0) + hc["margin"], hc["step"], hc["clear"])
        dirs = sphere(hc["rays"])
        seen = views(soup0, cams, dirs)
        kd = KDTree(len(cv))
        for v in cv:
            kd.insert(P[v], v)
        kd.balance()
    src = source_corners(faces0, fuv0, fpart, fnrm0)
    fixed, rounds = set(fixed), []
    for _ in range(hc["rounds"] if hc else 1):
        faces, fuv, fnrm = [list(f) for f in faces0], [list(u) for u in fuv0], [list(nr) for nr in fnrm0]
        stats = collections.Counter()
        alive, before, after = decimate(P, faces, fuv, fpart, fnrm, cutp, spec["ratio"], spec["maxerr"], spec.get("sliver", 1e-4),
                                        spec.get("uv_tol", 0.5 / 256), stats, fixed=fixed, vgroup=vgroup, mirror=mirror,
                                        **{k: spec[key] for k, key in (("area_tol", "collapse_area_tol"), ("piece_floor", "piece_floor"), ("piece_min", "piece_min")) if key in spec})
        snap_corners(faces, fuv, fpart, fnrm, alive, src, stats=stats)
        if not hc:
            break
        lost = holes(game_soup(P, faces, fnrm, alive), cams, dirs, seen, soup0, hc.get("near", 0.0))
        rounds.append([after, len(lost)])
        if not lost:
            break
        for p, fi in lost:   # the lost face's corners and every cut vertex round the spot
            fixed.update(faces0[fi])
            fixed.update(v for _, v, _ in kd.find_range(p, hc["radius"]))
    report = dict(cameras=len(cams), rounds=rounds, fixed=len(fixed), holes_left=rounds[-1][1]) if hc else {}
    return faces, fuv, fnrm, alive, before, after, stats, report


def source_corners(faces, fuv, fch, fnrm):
    """(vertex, part) -> the source's own (uv, normal) corners there (snap_corners puts the cut back onto them)."""
    out = collections.defaultdict(set)
    for fi, f in enumerate(faces):
        for j, v in enumerate(f):
            out[(v, fch[fi])].add((tuple(fuv[fi][j]), tuple(fnrm[fi][j])))
    return out


def snap_corners(faces, fuv, fch, fnrm, alive, src, snap_uv=0.03, snap_dot=0.7, stats=None):
    """Each live corner takes the source corner at its vertex (same part) nearest in UV (tiling wrap, the face's own tile kept) whose normal is
    within snap_dot, when that UV is within snap_uv. A moved face took its own UV extension and a removed face's normal, so faces sharing a
    vertex disagree by a hair, and the importer weld (one game vertex per distinct position, uv and normal) would store a vertex per face: the
    snapped cut has no corner the source did not have (blender_cut_vehicle.py does the same)."""
    stats = stats if stats is not None else collections.Counter()

    def wrap(d):
        return abs(d - round(d))
    for fi, f in enumerate(faces):
        if not alive[fi]:
            continue
        for j, v in enumerate(f):
            uv, nr = fuv[fi][j], fnrm[fi][j]
            best, bd = None, snap_uv
            for suv, sn in src[(v, fch[fi])]:
                dot = sum(a * b for a, b in zip(nr, sn))
                d = max(wrap(uv[0] - suv[0]), wrap(uv[1] - suv[1])) + 0.01 * (1.0 - dot)
                if d < bd and dot > snap_dot:
                    best, bd = (suv, sn), d
            if best is not None:
                du, dv = round(uv[0] - best[0][0]), round(uv[1] - best[0][1])
                fuv[fi][j], fnrm[fi][j] = (best[0][0] + du, best[0][1] + dv), best[1]
                stats['snapped'] += 1
            else:
                stats['unsnapped'] += 1


# TODO(SOURCE): Halo file / import rule (winding, weld, OBJ layout); replace with the source engine reader's rule before use.
def game_vertices(faces, fuv, fch, fnrm, alive):
    """Distinct (part, vertex, uv, normal) corners of the live faces: the vertices the importer weld gives the game."""
    return len({(fch[fi], v, tuple(np.round(fuv[fi][j], 5)), tuple(np.round(fnrm[fi][j], 4))) for fi, f in enumerate(faces) if alive[fi]
                for j, v in enumerate(f)})


# TODO(SOURCE): Halo file / import rule (winding, weld, OBJ layout); replace with the source engine reader's rule before use.
def write_obj(path, header, mtllib, P, faces, fuv, fch, fnrm, alive, parts):
    """OBJ of the cut parts in the source's axes. parts: [(part index, object name G<n>_P<n>, material)]. The vt and vn rows are shared and a
    normal's row is its game value (rint(n * 127), the importer), so the importer weld makes one game vertex per distinct position, uv and normal.
    Blender's own exporter would not: it hands back each custom normal re-encoded per corner fan, a hair apart, one vertex per face."""
    lines = [f"# {header}", f"mtllib {mtllib}"]
    vts, vns = {}, {}

    def vt_row(uv):
        text = f"vt {uv[0]:.6f} {uv[1]:.6f}"
        if text not in vts:
            vts[text] = len(vts) + 1
            lines.append(text)
        return vts[text]

    def vn_row(n):
        n = np.asarray(n, np.float64)
        n = n / (np.linalg.norm(n) + 1e-20)
        key = tuple(int(x) for x in np.rint(n * 127))
        if key not in vns:
            vns[key] = len(vns) + 1
            lines.append(f"vn {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}")
        return vns[key]
    base = 1
    for ci, name, mat in parts:
        own = [fi for fi in range(len(faces)) if alive[fi] and fch[fi] == ci]
        used = sorted({v for fi in own for v in faces[fi]})
        idx = {v: base + k for k, v in enumerate(used)}
        lines += [f"o {name}", f"usemtl {mat}"] + [f"v {P[v][0]:.6f} {P[v][1]:.6f} {P[v][2]:.6f}" for v in used]
        base += len(used)
        for fi in own:
            lines.append("f " + " ".join(f"{idx[v]}/{vt_row(fuv[fi][j])}/{vn_row(fnrm[fi][j])}" for j, v in enumerate(faces[fi])))
    with open(path, "w", encoding="utf-8", newline="\n") as fp:
        fp.write("\n".join(lines) + "\n")


# TODO(SOURCE): Halo file / import rule (winding, weld, OBJ layout); replace with the source engine reader's rule before use.
def write_back(objs, P, faces, fuv, fch, fnrm, alive, todo):
    """Replace each cut part's mesh with its surviving faces (positions shared with its neighbours, per corner UV and normal)."""
    for ci in todo:
        idx, co, polys, uvs, nrms = {}, [], [], [], []
        for fi, f in enumerate(faces):
            if not alive[fi] or fch[fi] != ci:
                continue
            poly = []
            for v, uv in zip(f, fuv[fi]):
                if v not in idx:
                    idx[v] = len(co)
                    co.append(tuple(P[v]))
                poly.append(idx[v])
                uvs.append(uv)
            nrms.extend(fnrm[fi])
            polys.append(poly)
        me = objs[ci].data
        mats = list(me.materials)
        me.clear_geometry()
        me.from_pydata(co, [], polys)
        uvl = me.uv_layers.new(name="UVMap")
        for i, u in enumerate(uvs):
            uvl.data[i].uv = u
        for m in mats:
            me.materials.append(m)
        me.update()
        me.normals_split_custom_set(nrms)   # the OBJ export writes them (the importer then takes no normal from a nearest vertex)


def open_edge_length(P, faces, alive):
    """Total length of the edges with one live face (the mesh's holes and outline): a sealed cut keeps the source's."""
    count = collections.Counter()
    for fi, f in enumerate(faces):
        if not alive[fi]:
            continue
        for j in range(3):
            a, b = f[j], f[(j + 1) % 3]
            count[(min(a, b), max(a, b))] += 1
    return float(sum(np.linalg.norm(P[a] - P[b]) for (a, b), k in count.items() if k == 1))


# Hole check: what the game shows from inside. A sealed cut opens no crack, yet a seat edge or a panel lip that moves a centimetre can bare the
# hull's inside, which the game culls (its back faces), so the sky shows through. Cameras on a grid over the cut parts (plus a margin) that
# keep `clear` wu from every surface and meet no back face first (open space, not inside a seat) look along `rays` directions; a hole is a
# ray that meets a front face in the source and none in the cut.

# TODO(SOURCE): Halo file / import rule (winding, weld, OBJ layout); replace with the source engine reader's rule before use.
def game_soup(P, faces, fnrm, alive):
    """The live faces as the engine winding rule draws them: wound along their summed corner normals, the weak ones (|cos| < 0.3) both ways.
    -> (BVHTree, front vector per triangle, weak flag per triangle, face index per triangle)."""
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    verts = [Vector(p) for p in P]
    tris, weak, src = [], [], []
    for fi, f in enumerate(faces):
        if not alive[fi]:
            continue
        nn, _ = normal(P, f)
        if nn is None:
            continue
        s = np.sum(np.array(fnrm[fi]), axis=0)
        d = float(nn @ s)
        tris.append(list(f) if d >= 0 else [f[0], f[2], f[1]])
        weak.append(abs(d) < 0.3 * (float(np.linalg.norm(s)) + 1e-20))
        src.append(fi)
    fn = [(verts[t[1]] - verts[t[0]]).cross(verts[t[2]] - verts[t[0]]) for t in tris]
    return BVHTree.FromPolygons(verts, tris, epsilon=0.0), fn, weak, src


def trace(soup, o, r, first_only=False):
    """-> (distance, face) of the first front face along the ray (None: sky); first_only: 'front', 'back' or None for the first face met."""
    bvh, fn, weak, src = soup
    travelled = 0.0
    for _ in range(16):
        loc, _, idx, dist = bvh.ray_cast(o, r)
        if idx is None:
            return None
        travelled += dist
        front = weak[idx] or fn[idx].dot(r) < 0
        if first_only:
            return 'front' if front else 'back'
        if front:
            return travelled, src[idx]
        o = loc + r * 1e-5
        travelled += 1e-5
    return None


def sphere(n):
    from mathutils import Vector
    g = np.pi * (3 - np.sqrt(5))
    out = []
    for i in range(n):
        z = 1 - 2 * (i + 0.5) / n
        rr = np.sqrt(1 - z * z)
        out.append(Vector((np.cos(g * i) * rr, np.sin(g * i) * rr, z)))
    return out


# TODO(SOURCE): Halo file / import rule (winding, weld, OBJ layout); replace with the source engine reader's rule before use.
def view_cameras(soup, lo, hi, step, clear, rays_valid=128):
    from mathutils import Vector
    dv = sphere(rays_valid)
    cams = []
    nx, ny, nz = [max(1, int((hi[k] - lo[k]) / step)) for k in range(3)]
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                c = Vector([lo[a] + (idx + 0.5) * (hi[a] - lo[a]) / nn for a, idx, nn in ((0, i, nx), (1, j, ny), (2, k, nz))])
                if soup[0].find_nearest(c, clear)[0] is not None:
                    continue
                if sum(1 for r in dv if trace(soup, c, r, True) == 'back') <= 0.01 * rays_valid:
                    cams.append(c)
    return cams


def views(soup, cams, dirs):
    """-> (distance, face) arrays [camera, ray] of the first front face met (nan, -1: sky); numpy, ten million rays fit in Blender."""
    dist = np.full((len(cams), len(dirs)), np.nan, np.float32)
    face = np.full((len(cams), len(dirs)), -1, np.int32)
    for ci, c in enumerate(cams):
        for ri, r in enumerate(dirs):
            t = trace(soup, c, r)
            if t is not None:
                dist[ci, ri], face[ci, ri] = t
    return dist, face


# TODO(SOURCE): Halo file / import rule (winding, weld, OBJ layout); replace with the source engine reader's rule before use.
def holes(soup, cams, dirs, seen, soup0=None, near=0.0):
    """seen: the source's views(). -> (point, face) of the source surface the cut lost, per hole ray. near (wu, with soup0 the source): a lost
    spot with source sky within `near` of it (12 rays round it) is the outline moving, not a hole (a character's silhouette is all sky)."""
    dist, face = seen
    out = []
    for ci, c in enumerate(cams):
        for ri in np.nonzero(face[ci] >= 0)[0]:
            r = dirs[ri]
            if trace(soup, c, r) is not None:
                continue
            if near > 0.0 and soup0 is not None:
                t = r.orthogonal().normalized()
                b = r.cross(t)
                k = near / float(dist[ci, ri])
                if any(trace(soup0, c, (r + (t * np.cos(a) + b * np.sin(a)) * k).normalized()) is None for a in np.arange(12) * (np.pi / 6)):
                    continue
            out.append((c + r * float(dist[ci, ri]), int(face[ci, ri])))
    return out
