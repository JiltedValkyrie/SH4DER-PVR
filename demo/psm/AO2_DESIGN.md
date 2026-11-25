# ao2: a tile an object. Design note, not built

Written 2026-11-25 by Senior for lead. **Nothing here is built or run.** A number with a ledger id is MEASURED on another scene and carried over; DERIVED is arithmetic on such numbers; the rest is ESTIMATE. The idea is the ideas page's X26, with X29 for the movers' pass (`wiki/research/depth-shading-ideas.md`).

## What it is

Method 2 (`ao_layer.h`) draws every caster from K sky directions into the shared shadow target every frame. In the town that is +10.0 ms a frame at K 4 (PVR-135), nearly all of it the SH-4 sending the same static houses again. ao2 gives each caster object its own small square of an atlas instead:

- **A static object's tile is drawn once, at load**, by the PVR: its casters from K directions, the tile's own slide matrices. After that it is a texture.
- **A mover's tile is drawn again every frame**, in a second small atlas. The tile is centred on the mover and does not turn with it.
- **Each frame the ground takes the tiles as multiply layers**: one quad an object, lying on the ground over the tile's square. The town's ground is flat and the map is top-down, so the lookup is affine and four vertices are exact (`sh4der_psm_pvr_uv_error` is 0 for a uniform map). Where two quads overlap the ground gets the product, which is the right rule for occlusion.

It darkens the ground only, as method 2 does.

## Layout for the town (27 static caster objects, 12 movers)

| Atlas | Size | Tiles | Video memory | A texel |
|---|---|---|---|---|
| Static | 512 × 256 (rows stay under 480, PVR-026) | 32 of 64 px: 27 used | 256 KB | house, 12 m + 2.1 m reach each side: 25 cm. A 2 m prop: 10 cm |
| Movers | 128 × 128 | 16 of 32 px: 12 used | 32 KB | a 2.4 m tile: 7.5 cm |

- The shared map is 256 px over 96 m: 37.5 cm a texel. So a house gains little in sharpness at 64 px; the props and the movers gain 4 to 5 times. The real gain is the cost, below.
- Reach is 60 % of `ao_layer.h`'s slide (ao4's default; at 100 % a 6 m wall reaches 3.5 m and reads as extra shadows on the host preview).
- A mover's 32 px tile is exactly one PVR tile. A tile is sized so the caster and its slides stay inside it, with one white texel round the edge for the bilinear filter; then no clip is needed between tiles.

## Costs against the shared 256 map

| | Method 2 today (ao1, K 4, both sides) | ao2 | Status |
|---|---|---|---|
| SH-4 a frame, static casters | about 7.3 ms (1,164 triangles a direction at 1.82 ms, SH4-071) | 27 quads, 108 vertices: about 0.06 ms | DERIVED (522 ns a vertex) |
| SH-4 a frame, movers | in the 10 ms | 8 people × 30 + 4 hens × 20 = 320 caster triangles, one side, K 4: 1,920 vertices, about 1.0 ms by vertices sent; more by SH4-072 (one side saves the sending, not the transform and the test) | DERIVED |
| PVR a frame | +1.4 ms mean (PVR-135) | a movers' pass: floor about 3.2 ms (PVR-004), or about 0.3 to 0.4 ms cut to its 12 tiles (PVR-092, X29); plus the fill of 39 translucent quads on the screen | ESTIMATE; the quads' fill is OPEN |
| At load | 0 | K 8 × (1.82 + 1.21) ms: about 24 ms, once | DERIVED |
| TA a frame | 68 KB a direction (34 one side) | movers' pass about 38 KB; screen +39 quads, under 6 KB | ESTIMATE (60 bytes a triangle) |
| Video memory | 0 (route A's target) | 288 KB | exact |
| Passes a frame | the map pass it already has | one more target pass | |

So the SH-4 side falls from about 10 ms to about 1 ms (DERIVED). The PVR side is the unknown: 39 blended quads over the ground can cost more fill than one map did. A house's quad is about 16 m square, and near the camera several overlap.

## What can go wrong

1. **The depth union.** On the console it gave one darkening a caster, not one a direction (ao0, PVR-127). With one object in a tile there is no second caster, so the union may hold exactly here; ao0 did not test that case. If it does not, the 20 town objects that are open shells need both sides with plain blending and darken twice where their front and back overlap. This is test T1 and it decides the look.
2. **Fill.** See above. If it is too much, the static tiles can be flattened once at load into the shared map's resolution (variant B of `AO_METHOD.md`, ao3): one quad, and the sharpness is given up.
3. **A third pass a frame.** The movers' atlas is one more target pass with its own render-done wait (polled). Uncut it costs the 3.2 ms floor; the cut needs the tile-list edit of the c4 tile stage or a user tile clip (CORE 18: a dummy polygon before each switch).
4. **Movers under a static tile** get both darkenings multiplied. That is right for occlusion, and is what the hens under the table in ao4's room show with method 2.
5. **Not for a perspective map**: the lookup is affine only top-down.

## Tests a spike would run (one disc entry, autorun, two pages)

| | Question | Measure |
|---|---|---|
| T1 | One open-shell object in one tile, union, both sides: one darkening a direction? | texels that differ from the host model, as ao0 counted |
| T2 | 27 tiles at load | ms for the whole atlas; a picture of the atlas on screen |
| T3 | The quads' fill | PVR ms with 0, 12 and 39 quads at three camera places |
| T4 | The movers' pass | PVR ms uncut, and cut to 12 tiles |
| T5 | The look | CLAY, one place, ao1 against ao2 side by side (ao4's split) |

## Recommendation

Build it only if T1 and T3 can be answered first in a small spike (one house, a few quads): they decide whether ao2 is better than variant B. If the owner's first question after ao4 is "can the room's hens have this cheaply", the movers' half alone (the 128 × 128 atlas, 12 quads) is the smallest useful piece, and it needs no static atlas at all next to a baked room.
