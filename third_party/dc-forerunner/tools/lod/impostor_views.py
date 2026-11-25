"""Impostor math for far LOD cards: view ring, z-buffered orthographic cell render, sun shading, bleed, box shrink. Pure numpy.

Ported from the Halo DC port's tools/assets/bake_impostors.py (views, rasterize, light, bleed, render_view) and xfar.py (shrink), with the
Halo model loader, the texture sampler and the Halo blob writer removed. The render takes a triangle soup and a texel callback; the caller
owns the mesh format (source engine reader: TODO(SOURCE) in cross_quads.py) and the texture decode.

Method (after oot-sat's build_npc_sprites.py): the mesh itself, rendered offline, orthographic and z-buffered, from a fixed set of directions. At
run time the engine picks the view nearest the camera. Sized for a PowerVR-class GPU: 58 views (side ring of 16, rings of 12 at +-35 degrees,
8 at +-65, one straight up and one straight down) on an 8 x 8 grid, 64 px cells, 1-bit alpha at 0.5. No mirrored views: most models are not
symmetric.

Coordinates: model space, Z up. A view is (d, right, up): d = unit direction from the model towards the camera, right / up = the image axes.
Image row 0 is the top. Cell texels are square: texel = 2 * half / cell.

Self test: python impostor_views.py  (needs numpy; prints the view count and checks a unit square covers its cell)
"""
import math

import numpy as np

RINGS = ((90, 1), (65, 8), (35, 12), (0, 16), (-35, 12), (-65, 8), (-90, 1))   # (elevation degrees, views)
SS = 4               # supersampling per texel
SUN = (-0.30, -0.40, 0.86)   # sun direction (model space, unit-ish); the engine's own light
YAW_SAMPLES = 8
AMBIENT, SUN_GAIN = 0.45, 0.55


def views(rings=RINGS):
    """-> list of (d, right, up): the camera ring, elevation by elevation, azimuth in equal steps."""
    out = []
    for elev, count in rings:
        e = math.radians(elev)
        for k in range(count):
            a = 2 * math.pi * k / count
            d = np.array([math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)])
            f = -d
            ref = np.array([0.0, 0.0, 1.0]) if abs(d[2]) < 0.999 else np.array([1.0, 0.0, 0.0])
            right = np.cross(f, ref)
            right /= np.linalg.norm(right)
            up = np.cross(right, f)
            out.append((d, right, up))
    return out


def rasterize(xy, tris, size):
    """Pixel-centre coverage of every triangle. -> (triangle, pixel index, barycentrics (n,3)); no culling."""
    a, b, c = xy[tris[:, 0]], xy[tris[:, 1]], xy[tris[:, 2]]
    area = (b[:, 0] - a[:, 0]) * (c[:, 1] - a[:, 1]) - (c[:, 0] - a[:, 0]) * (b[:, 1] - a[:, 1])
    lo = np.minimum(np.minimum(a, b), c)
    hi = np.maximum(np.maximum(a, b), c)
    x0 = np.clip(np.ceil(lo[:, 0] - 0.5), 0, size).astype(np.int64)
    y0 = np.clip(np.ceil(lo[:, 1] - 0.5), 0, size).astype(np.int64)
    x1 = np.clip(np.floor(hi[:, 0] - 0.5), -1, size - 1).astype(np.int64)
    y1 = np.clip(np.floor(hi[:, 1] - 0.5), -1, size - 1).astype(np.int64)
    w = np.maximum(x1 - x0 + 1, 0)
    h = np.maximum(y1 - y0 + 1, 0)
    count = np.where(np.abs(area) > 1e-12, w * h, 0)
    keep = np.nonzero(count)[0]
    tri = np.repeat(keep, count[keep])
    start = np.repeat(np.cumsum(count[keep]) - count[keep], count[keep])
    local = np.arange(len(tri)) - start
    px = x0[tri] + local % w[tri]
    py = y0[tri] + local // w[tri]
    fx, fy = px + 0.5, py + 0.5
    A, B, C, ar = a[tri], b[tri], c[tri], area[tri]
    l0 = ((B[:, 0] - fx) * (C[:, 1] - fy) - (C[:, 0] - fx) * (B[:, 1] - fy)) / ar
    l1 = ((C[:, 0] - fx) * (A[:, 1] - fy) - (A[:, 0] - fx) * (C[:, 1] - fy)) / ar
    l2 = 1.0 - l0 - l1
    inside = (l0 >= -1e-7) & (l1 >= -1e-7) & (l2 >= -1e-7)
    return tri[inside], (py * size + px)[inside], np.stack([l0, l1, l2], 1)[inside]


def light(normals, sun=SUN, ambient=AMBIENT, gain=SUN_GAIN, yaw_samples=YAW_SAMPLES):
    """Sun shading with the sun's horizontal part averaged over yaw, so one card serves every placement's heading:
    ambient + gain * mean(max(0, n . sun)) over yaw_samples rotations of the horizontal part."""
    h = math.hypot(sun[0], sun[1])
    lit = np.zeros(len(normals))
    for k in range(yaw_samples):
        phi = 2 * math.pi * k / yaw_samples
        lit += np.maximum(normals @ np.array([h * math.cos(phi), h * math.sin(phi), sun[2]]), 0.0)
    return ambient + gain * lit / yaw_samples


def render_cell(p, n, tris, texel_fn, view, cell, centre, half, ss=SS):
    """One cell, RGBA uint8 (cell x cell). p (V,3) positions, n (V,3) unit normals, tris (T,3) int indices.
    texel_fn(tri, corners, bary, normal) -> (rgb (k,3) float 0..255, alpha (k,) 0..255) for the k fragments; a fragment with
    alpha < 128 is cut (1-bit alpha, as the engine's alpha-tested shaders). The nearest surviving fragment per supersample wins.
    centre: model-space point in the middle of the cell; half: half the cell's world size."""
    d, right, up = view
    size = cell * ss
    scale = size / (2.0 * half)
    colour = np.zeros((size * size, 3))
    depth = np.full(size * size, -np.inf)
    pc = p - centre
    xy = np.stack([pc @ right * scale + size / 2, size / 2 - pc @ up * scale], 1)
    z = pc @ d
    tri, pix, bary = rasterize(xy, tris, size)
    if len(tri):
        corners = tris[tri]
        zf = (z[corners] * bary).sum(1)
        nrm = (n[corners] * bary[:, :, None]).sum(1)
        nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
        rgb, alpha = texel_fn(tri, corners, bary, nrm)
        ok = alpha >= 128
        pix, zf, rgb = pix[ok], zf[ok], rgb[ok]
        if len(pix):
            order = np.lexsort((-zf, pix))   # per pixel, nearest to the camera first
            first = np.ones(len(order), bool)
            first[1:] = pix[order][1:] != pix[order][:-1]
            win = order[first]
            colour[pix[win]] = rgb[win]
            depth[pix[win]] = zf[win]
    mask = np.isfinite(depth).reshape(cell, ss, cell, ss)
    rgb = colour.reshape(cell, ss, cell, ss, 3)
    cover = mask.mean((1, 3))
    total = (rgb * mask[..., None]).sum((1, 3)) / np.maximum(mask.sum((1, 3)), 1)[..., None]
    out = np.zeros((cell, cell, 4), np.uint8)
    out[..., :3] = np.clip(total, 0, 255).astype(np.uint8)
    out[..., 3] = np.where(cover >= 0.5, 255, 0)
    return out


def bleed(rgba, steps=8):
    """Spread colour into transparent texels (alpha stays 0), so filtering and mips do not darken the edges."""
    rgb = rgba[..., :3].astype(np.float64)
    known = rgba[..., 3] > 0
    for _ in range(steps):
        acc = np.zeros_like(rgb)
        cnt = np.zeros(known.shape)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            k = np.roll(known, (dy, dx), (0, 1))
            acc += np.roll(rgb, (dy, dx), (0, 1)) * k[..., None]
            cnt += k
        grow = ~known & (cnt > 0)
        rgb[grow] = acc[grow] / cnt[grow][:, None]
        known = known | grow
    out = rgba.copy()
    out[..., :3] = rgb.astype(np.uint8)
    return out


def shrink(rgba, w, h):
    """Premultiplied BOX shrink of an RGBA uint8 array to w x h (keeps the edge colour), then 1-bit alpha at 0.5."""
    from PIL import Image
    a = rgba.astype(np.float64) / 255.0
    pm = a.copy()
    pm[..., :3] *= pm[..., 3:4]
    s = np.asarray(Image.fromarray((pm * 255).round().astype(np.uint8), "RGBA").resize((w, h), Image.BOX)).astype(np.float64) / 255.0
    al = s[..., 3:4]
    rgb = np.where(al > 0, s[..., :3] / np.maximum(al, 1e-6), 0)
    out = np.zeros((h, w, 4), np.uint8)
    out[..., :3] = (rgb.clip(0, 1) * 255).round()
    out[..., 3] = np.where(al[..., 0] >= 0.5, 255, 0)
    return out


if __name__ == "__main__":
    v = views()
    print("views:", len(v), "(58 expected for the default rings)")
    # a unit square in the z = 0 plane, seen from straight above, filling a cell of half size 0.5: the cell must be fully covered
    p = np.array([[-0.5, -0.5, 0], [0.5, -0.5, 0], [0.5, 0.5, 0], [-0.5, 0.5, 0]], float)
    n = np.tile([0.0, 0.0, 1.0], (4, 1))
    tris = np.array([[0, 1, 2], [0, 2, 3]])
    top = [x for x in v if x[0][2] > 0.99][0]
    cell = render_cell(p, n, tris, lambda t, c, b, nr: (np.full((len(t), 3), 200.0), np.full(len(t), 255.0)),
                       top, 16, np.zeros(3), 0.5)
    cov = float((cell[..., 3] == 255).mean())
    print("cell coverage: %.3f (about 1.0 expected)" % cov)
