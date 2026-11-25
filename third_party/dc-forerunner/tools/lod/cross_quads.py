"""Cross-quad far cards for trees, plants and rocks: two crossed quads over a 128 x 128 atlas (front | side, 64 x 128 texels each).

Ported from the Halo DC port's tools/assets/xfar.py: the card layout (bake_card), the box shrink and the atlas split. The mesh is an input
(positions + normals + UVs + triangles, any engine), the render is impostor_views.render_cell, and the texel decode is the caller's.
The engine blob writer (the Halo port's 'DCXC' header, 64 bytes, the texture at offset 64) is NOT ported: it is the Halo runtime's format.
TODO(SOURCE): write the card in the source engine material / texture format and the engine's placement rule (the Halo port drew cards only for trees,
plants and rocks, past the object distance, and only where a card cost less VRAM than the mesh did).

Layout (model space, Z up; the card stands on the lowest drawn vertex):
  height  = max(tree height, 2 * tree width) * (1 + 2 * GUTTER)   (the 1:2 cell holds the tree whichever way it is wider than tall / 2)
  half_width = height / 4   (the card's cell is 64 x 128: width 2 * half_width, height)
  FRONT quad: the XZ plane, x from cx - half_width to cx + half_width, atlas u 0..0.5, camera direction (0, -1, 0)
  SIDE  quad: the YZ plane, y from cy - half_width to cy + half_width, atlas u 0.5..1, camera direction (1, 0, 0)
  Both centred on (cx, cy), bottom at base_z, top at base_z + height. Image row 0 is the top of a quad (v = 0 at the top).

Use: layout = card_layout(positions); quads = cross_quad_vertices(layout); atlas = atlas_from_views(front_rgba, side_rgba)
where front_rgba / side_rgba are impostor_views.render_cell outputs: the FRONT view is FRONT, the SIDE view is SIDE (above), each HI = 512 square.

Self test: python cross_quads.py  (a 2 m x 2 m x 6 m test box: prints the layout and the quad count)
"""
import numpy as np

import impostor_views as iv

GUTTER = 0.02     # margin around the tree, as a share of the card height
CELL_W, CELL_H = 64, 128
ATLAS = 128
HI = 512          # the render's square side; the card's 1:2 cell is its middle half
FRONT = (np.array([0.0, -1.0, 0.0]), np.array([1.0, 0.0, 0.0]), np.array([0.0, 0.0, 1.0]))   # (towards the camera, screen right, screen up)
SIDE = (np.array([1.0, 0.0, 0.0]), np.array([0.0, 1.0, 0.0]), np.array([0.0, 0.0, 1.0]))


def card_layout(positions, gutter=GUTTER):
    """positions (n,3) of the drawn vertices -> dict(height, half_width, base_z, cx, cy, radius, centre, lo, hi, tree_width, tree_height)."""
    pts = np.asarray(positions, np.float64)
    lo, hi = pts.min(0), pts.max(0)
    centre = (lo + hi) / 2
    width = max(hi[0] - lo[0], hi[1] - lo[1])
    height = max(hi[2] - lo[2], 2.0 * width) * (1.0 + 2 * gutter)
    base_z = float(lo[2]) - gutter * height
    mid = np.array([centre[0], centre[1], base_z + height / 2])
    radius = float(np.linalg.norm(pts - mid, axis=1).max())
    return dict(height=float(height), half_width=float(height / 4), base_z=base_z, cx=float(centre[0]), cy=float(centre[1]),
                radius=radius, centre=mid, lo=lo, hi=hi, tree_width=float(width), tree_height=float(hi[2] - lo[2]))


def cross_quad_vertices(layout):
    """-> (positions (8,3), uvs (8,2), triangles (4,3) int). Vertices 0..3 the FRONT quad, 4..7 the SIDE quad; corners ordered
    bottom-left, bottom-right, top-right, top-left. Winding is the engine's front-face rule (TODO(SOURCE): check it)."""
    cx, cy, z0, h, hw = layout["cx"], layout["cy"], layout["base_z"], layout["height"], layout["half_width"]
    z1 = z0 + h
    front = [(cx - hw, cy, z0), (cx + hw, cy, z0), (cx + hw, cy, z1), (cx - hw, cy, z1)]
    side = [(cx, cy - hw, z0), (cx, cy + hw, z0), (cx, cy + hw, z1), (cx, cy - hw, z1)]
    uv_front = [(0.0, 1.0), (0.5, 1.0), (0.5, 0.0), (0.0, 0.0)]
    uv_side = [(0.5, 1.0), (1.0, 1.0), (1.0, 0.0), (0.5, 0.0)]
    pos = np.array(front + side, np.float64)
    uvs = np.array(uv_front + uv_side, np.float64)
    tris = np.array([[0, 1, 2], [0, 2, 3], [4, 5, 6], [4, 6, 7]], np.int64)
    return pos, uvs, tris


def atlas_from_views(front_rgba, side_rgba, bleed_steps=8):
    """Two HI x HI RGBA renders (or their 64 x 128 crops) -> the 128 x 128 atlas: front left, side right, bled into the transparent texels."""
    if front_rgba.shape[0] == HI:
        q = HI // 4
        front_rgba = front_rgba[:, q:HI - q]
        side_rgba = side_rgba[:, q:HI - q]
    atlas = np.zeros((ATLAS, ATLAS, 4), np.uint8)
    atlas[:, :CELL_W] = iv.shrink(front_rgba, CELL_W, CELL_H)
    atlas[:, CELL_W:] = iv.shrink(side_rgba, CELL_W, CELL_H)
    return iv.bleed(atlas, bleed_steps)


if __name__ == "__main__":
    # a 2 m x 2 m x 6 m box standing on z = 0 (units: the caller's, metres here)
    corners = np.array([[x, y, z] for x in (-1, 1) for y in (-1, 1) for z in (0, 6)], np.float64)
    lay = card_layout(corners)
    pos, uvs, tris = cross_quad_vertices(lay)
    print("height %.2f half_width %.2f base_z %.2f centre (%.2f, %.2f)" % (lay["height"], lay["half_width"], lay["base_z"], lay["cx"], lay["cy"]))
    print("quad vertices %d, triangles %d (the cross-quad is 4 triangles)" % (len(pos), len(tris)))
