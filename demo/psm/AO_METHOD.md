# Ambient occlusion in the town: two methods, their costs, and which to build

Written 2026-11-25 by Senior for lead and Junior. **Nothing here has run on a console.** Every cost is an ESTIMATE unless a ledger id stands beside it; those are measured on other scenes and carried over.

The demo flips live between OFF, method 1 and method 2 (X). Both multiply into the town's existing light; neither needs a reload to switch.

## Method 1: baked AO, per vertex, zero PVR cost

**What.** At load the SH-4 casts 16 short rays (4 m) over the hemisphere of each static model vertex and of each texel of a ground map, against the static caster triangles and the ground plane. A ray that hits counts by how near the hit is. The result is one byte a vertex and one byte a ground texel.

**Why rays and not `sh4der_ao.h`'s crease term.** The crease term needs two faces that share an edge. The town is boxes that pass through each other and through the ground: a wall and the ground it stands on share no edge. So the crease term finds almost nothing here. It stays the right tool for a single welded mesh; the note in the demo will say so.

**How it is switched.** A per-vertex AO byte multiplied at draw, not a second copy of the colours. The town's colours are already made again every frame (`psm_colour`: albedo × (ambient + sun × lit)), so there is no AO-free colour to keep a copy of. AO goes on the ambient part alone: `ambient × ao + sun × lit`. OFF reads the byte as 255.

| Cost | Value | Status |
|---|---|---|
| Load | about 3 s once: (9,700 vertices + 16,384 ground texels) × 16 rays × about 24 triangles × 62 cycles (RT.1, ray2) | ESTIMATE |
| Frame, models | +3 multiplies a vertex in `psm_colour`: about 0.4 ms for 9,700 vertices | ESTIMATE |
| Frame, ground | 0 ms on most frames: the ground's table `glut[256]` becomes `glut[16][256]` (16 AO levels), made again only when the sun has moved. About 1.2 ms on a frame that remakes it | ESTIMATE |
| Frame, ground AO lookup | about 1.3 ms, only on a frame where the ground lattice moved (one bilinear read a ground vertex) | ESTIMATE |
| RAM | 12 KB model bytes, 16 KB ground map (128 × 128 over the 96 m grid), 7 KB ground bytes, 16 KB table: about 51 KB | exact from the sizes |
| Video memory, TA vertices, PVR time | 0 | by construction |

**Limits.** The ground's AO is only as fine as the ground lattice, which is coarse far from the eye. Movers get none and cast none.

## Method 2: AO from the PVR's shadow map, in route A's own target

**The idea.** Sky light is light from every direction. A shadow from one tilted direction is one sample of it. Route A already has the PVR draw the casters into a target, from the sun, and lays that target over the ground. Method 2 draws the casters into **the same target** K more times, each time as seen from another sky direction, each time as a faint grey. Where several of those shadows overlap, the ground is darker. The one layer that already carries the sun's shadow then carries the AO too.

**Why it is cheap.** For a flat receiver, the shadow from direction d is the caster slid along d onto the ground. That slide is a matrix. So direction k needs only a different matrix for the same caster vertices: `A_k = M × slide_k`, where M is route A's map matrix. No new target, no new layer, no new pass.

**One shadow a direction, not one a triangle.** Two triangles of one direction that overlap must darken once. Each direction is drawn at its own depth with the test "greater" and depth write on: the first triangle of a direction writes its depth, the second fails the test at the same pixel. This needs the translucent list drawn in the order sent, which the town already asks for (`autosort_disabled`). **Whether the PVR does this in a render-to-texture pass is OPEN** (PVR-046 is the nearest ledger row). The fallback is plain blending, where overlaps darken twice: rougher, still AO-like.

**Directions.** K azimuths evenly round the sky at 60° elevation, turned half a step for the next ring. At 60° a 6 m wall darkens the ground 3.5 m out, which is the reach of contact darkening. K = 1, 2, 4, 8 are selectable; each is one grey step of darkness.

| Variant | SH-4 a frame | TA a frame | PVR a frame | Video memory | Verdict |
|---|---|---|---|---|---|
| **A. K directions into route A's target, every frame** | about 1.7 ms a direction (3,200 vertices: transform 43 cycles, SH4-023; send 34 cycles, c4 test 5; no clip on a uniform map) | +101 KB a direction, in the map pass's own buffer, not the screen's | +1 to 2 ms a direction (map pass 3.3 ms with few casters, PVR-100; mask pass 6.1 to 9.2 ms at 1,288 to 2,464 vertices, PVR-100) | 0 | **build first** |
| B. Static AO drawn once by the PVR into a top-down atlas; each frame one quad of it into the target, plus the movers' K slides | about 0.5 ms (12 movers × K 4) | +55 KB | about +1 ms | 128 KB (256 × 256) | build second, if A's cost a direction is confirmed |
| C. K separate maps, K separate ground layers (lead's first form) | K × the route A map pass (6.2 ms in a3e-auto) | K × 277 KB **in the screen's buffer**, which is already full at 754 KB of 1,024 KB (a3e-auto: PARAM overflow on every A still frame) | K × (3.3 ms target + about 6 ms layer) | K × 128 KB | **rejected: the screen's vertex buffer cannot take one more layer** |
| D. One top-down depth map through the bump compare (the c4 chain), blurred by band width | band cut 0.9 to 3.2 ms, send 0.3 to 0.8 ms (c4g test 5) | 22 to 79 KB | map 3.3 ms + mask 5.2 to 9.2 ms (c4g) | 128 KB + the mask | not now: it buys self-shadowing, which the demo does not need, at twice A's PVR time |

At K = 4, variant A is about +7 ms SH-4, +400 KB in the map pass's buffer (it has 1,024 KB and uses about 100 KB now) and +4 to 8 ms PVR. Variant B would cut that to about +0.5 ms and +1 ms.

**Limits.** Ground only: walls and movers receive nothing (no self-shadowing, as agreed). The steps are visible at K 1 and 2; that is the point of making K selectable. AO multiplies the whole ground colour, the sun's part too; in the sun's own shadow that is right, in full sun it is slightly too dark. v1 is for MAP UNIFORM; a warped map needs the clip the sun's casters use, at about 6 ms a direction.

## Method 1 against method 2

| | Method 1, baked | Method 2, PVR map |
|---|---|---|
| Ground | yes, at the lattice's fineness | yes, at the map's texel (finer near the eye) |
| Walls and roofs | **yes** | no |
| Movers cast it | no | **yes** (a blob under each person and hen) |
| Follows a changed town | after a new bake (3 s) | at once |
| Frame cost | about 0.4 ms | about 2 to 4 ms a direction (A); about 1.5 ms in all (B) |
| Best use | the static town | what moves |

They are not rivals: the bake carries the static town and the map carries the movers. The demo shows each alone so the owner can see what each one adds.

## What I build, in order

1. `demo/psm/ao_bake.h`: method 1's bake and its ground lookup. Host test against a brute-force reference on a small scene.
2. `demo/psm/ao_layer.h`: method 2's directions, the slide matrix, the grey and depth of each direction, and the PVR header for the union draw. Host test: the slide against a ray cast, and the union's levels in a software model of the target.
3. An image of its own, **ao1**, for suite s3: a small scene (ground, a house, a post, a mover), not the town. It answers the hardware questions before the town depends on them:
   - does the depth union work in a render-to-texture pass? (the target is read back and its grey levels counted: with the union there are exactly K + 1);
   - SH-4 ms, TA vertices and KB, PVR ms for K = 0, 1, 2, 4, 8, union on and off;
   - render-done is polled (Q53); no sleeping wait.
4. Junior's town variant calls the two headers. What he needs from me is in each header's top comment.

## What Junior needs to change in the town (his files)

- `psm_colour`: `ambient × ao + sun × lit` for models, with `ao` from `ao_bake.h`'s byte array when method 1 is on; the ground through `glut[ao >> 4][lit]`.
- Route A's map pass: after the sun's casters, for each direction k, the casters through `A_k` in the translucent list with `ao_layer.h`'s header, colour and depth. Uniform map only in v1.
- Result pages: TA vertices and KB of the map pass and of the screen, SH-4 and PVR ms, for OFF / 1 / 2.
