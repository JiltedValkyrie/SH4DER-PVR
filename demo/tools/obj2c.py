"""OBJ (triangles, v / vt / vn) -> models.h: per model, unindexed triangle vertices (x y z nx ny nz) and one grey level per vertex from its material's name.
Usage: python obj2c.py out.h a.obj b.obj ..."""
import sys
import zlib
from pathlib import Path

# The demo's own models (tools/make_models.py): each part's colour by its material's name. Any other material keeps the colour made from its name.
PALETTE = {
    'dude_head': [225, 190, 160], 'dude_neck': [205, 170, 140], 'dude_chest': [86, 110, 150], 'dude_arm': [96, 120, 160], 'dude_forearm': [205, 170, 140],
    'dude_pelvis': [60, 70, 95], 'dude_thigh': [60, 70, 95], 'dude_shin': [70, 80, 105], 'dude_boot': [45, 40, 40],
    'wagon_paint': [120, 150, 120], 'wagon_tailgate': [105, 135, 105], 'wagon_glass': [60, 80, 95], 'wagon_bumper': [190, 190, 195], 'wagon_grille': [50, 50, 55],
    'wagon_hub': [180, 180, 185], 'wagon_lamp': [255, 245, 200], 'wagon_rack': [90, 90, 95], 'wagon_tyre': [35, 35, 38],
    'jet_grey': [150, 160, 170], 'jet_wing': [135, 145, 155], 'jet_tail': [120, 130, 140], 'jet_glass': [50, 80, 110], 'jet_intake': [60, 65, 70],
    'jet_nozzle': [45, 45, 50], 'jet_missile': [230, 230, 225], 'jet_pylon': [100, 105, 110],
    'crate_wood': [180, 140, 90], 'crate_band': [110, 85, 55],
    'lamp_base': [70, 70, 75], 'lamp_post': [110, 110, 115], 'lamp_housing': [220, 190, 60], 'lamp_glow': [255, 250, 220],
}

out, files = sys.argv[1], sys.argv[2:]
lines = ['/* made by tools/obj2c.py: do not edit */', 'typedef struct { const float *v; const unsigned char *c; unsigned n; } demo_model_t;']
names = []
for f in files:
    name = Path(f).stem
    v, vn, tris, mat = [], [], [], 'none'
    for ln in Path(f).read_text().splitlines():
        p = ln.split()
        if not p:
            continue
        if p[0] == 'v':
            v.append(tuple(float(x) for x in p[1:4]))
        elif p[0] == 'vn':
            vn.append(tuple(float(x) for x in p[1:4]))
        elif p[0] == 'usemtl':
            mat = p[1]
        elif p[0] == 'f':
            idx = [q.split('/') for q in p[1:]]
            for k in range(1, len(idx) - 1):
                tris.append((mat, [idx[0], idx[k], idx[k + 1]]))
    fl, cl = [], []
    for mat, tri in tris:
        h = zlib.crc32(mat.encode())
        # a muted colour per material: three levels 110..220
        col = PALETTE.get(mat) or [110 + (h >> s & 0xff) * 110 // 255 for s in (0, 8, 16)]
        for q in tri:
            pos = v[int(q[0]) - 1]
            nrm = vn[int(q[2]) - 1] if len(q) > 2 and q[2] else (0.0, 0.0, 1.0)
            fl.append(', '.join('%.5ff' % x for x in (*pos, *nrm)))
            cl.append(', '.join(str(x) for x in col))
    lines.append(f'static const float {name}_v[] = {{\n' + ',\n'.join(fl) + '\n};')
    lines.append(f'static const unsigned char {name}_c[] = {{\n' + ',\n'.join(cl) + '\n};')
    names.append((name, len(tris) * 3))
lines.append('enum { ' + ', '.join(f'M_{n.upper()}' for n, _ in names) + ', M_COUNT };')
lines.append('static const demo_model_t demo_models[M_COUNT] = {\n' + ',\n'.join(f'    {{ {n}_v, {n}_c, {c} }}' for n, c in names) + '\n};')
Path(out).write_text('\n'.join(lines) + '\n')
print(names)
