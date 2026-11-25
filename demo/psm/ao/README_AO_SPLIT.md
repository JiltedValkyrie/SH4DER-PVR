<!-- DRAFT of a README.md section, written 2026-11-25 by Senior for lead to place (after "The ao effect"). Not yet in
     README.md. The make targets and the --demo build it names come after suite s7 is cut. -->

## The AO split demo

`demo/psm/ao`: one scene drawn twice, the left half of the screen one way and the right half another, so what ambient occlusion adds can be seen and its cost read off the screen. It is a separate program (ao4), not part of the library; the methods it compares are below.

| Method | What it is | Darkens |
| --- | --- | --- |
| OFF | no occlusion: ambient + sun | nothing |
| 1 BAKED | a byte a vertex, baked once (64 rays a vertex), multiplied into the ambient light | everything baked: ground, walls, the shapes |
| 2 PVR MAP | the casters drawn into the shadow target from K sky directions, slid along the ground (`demo/psm/ao_layer.h`) | the ground only |
| 3 LIGHTMAP | scene C: each flat surface as one quad with its baked AO as a texture | as 1, with far fewer vertices |
| 3+M | scene C: 3, and a small tile a mover in its own target, one multiply quad a mover on the floor | as 3, and the floor under a mover |

Scenes: A (two spheres and a box on a ground), B (two walls, a corner, a sphere), C (a living room, with the town's four hens walking in it: movers a bake knows nothing of). Looks: CLAY (grey, the default: AO shows best), COLOUR, AO ONLY. EXAGGERATE doubles the occlusion.

### Controls

| Pad | Does |
| --- | --- |
| L or R trigger | move the divider between the halves, a tile column (32 pixels) a step; a half pushed off the screen is not drawn |
| L and R together | the scene: A, B, C |
| X | the next split (A, B: OFF \| 1, OFF \| 2, 1 \| 2; C: OFF \| 1, 1 \| 3, OFF \| 3, 3 \| 2, 3 \| 3+M, 3+M \| 2, 3+M \| 3+M) |
| Y | swap the halves |
| A | EXAGGERATE x2 |
| B | the look |
| D-pad up / down | K: 2, 4, 8 sky directions (scene C: down is the hens on / off) |
| D-pad left / right | method 2's reach: 35, 60, 80, 100 % |
| Stick | the camera |
| D-pad up held + B | the effect labels on / off |
| START | between the result pages and the live view |

It runs by itself first and counts the costs, then shows result pages and a graph. The trigger keys above are ao4-h's (suite s7): not run on a console yet; up to ao4-e2 either trigger was the scene.

### The effect labels

Two lines at the foot of the live view name what each half shows, who does the work each frame and when: for example `Ambient Occlusion: L SH4 Baked` and `Ambient Occlusion (ground): R SH4+PVR Realtime<<`. SH4 or PVR is who works each frame (the PVR is named only for work the effect needs: a render target pass, a texture that carries the effect, an alpha test); Baked, Texture or Realtime is when the effect is computed, and a Realtime entry is marked `<<` and drawn in yellow. The labels are off while the program measures and on in the live view and on the result pages.

<!-- DRAFT, same status: the two programs below are on suite s7, built 2026-11-25, NOT RUN on a console. No cost here is measured. -->

## The lamp demo (ao-lamp)

The living room's lamp with a paper shade lit from inside, left half against right half with the same divider. The shade is `sh4der_sss.h` per vertex (the paper passes the bulb's light) plus an emission term; slots cut in the paper are a 32 x 32 punch-through pattern; a ring glow (`sh4der_bloom_rings`) sits at the bulb; ROOM LIGHT lights the room's vertices from the bulb.

| Pad | Does |
| --- | --- |
| X | the next comparison: LAMP OFF \| ON, SLOTS OFF \| ON, ROOM LIGHT OFF \| ON, OFF \| OFF |
| Y | swap the halves |
| A | the camera: the room / near the lamp |
| B | the hens on / off |
| L or R trigger | move the divider |
| D-pad up held + B | the effect labels on / off |
| Stick | the camera |
| START | between the result pages and the live view |

## The depth fade test (d1)

A glow that passes through a wall is cut in a hard line. d1 tests a way to fade it there on the PVR, which has no depth texture: the scene is drawn once more into a small target as depth codes, that target is read as a bump texture whose unit compares it with the glow's own depth, and the result goes through the frame's destination alpha into the glow's blend. Three tests, a result page each: the destination alpha layers alone, the depth map, the fade on a glow 20 cm from a wall (left plain, right faded). The design is an estimate until it has run; `demo/psm/ao/NOTES.md` has its claims and their status.

| Pad | Does |
| --- | --- |
| X | right half: the fade / its alpha shown as grey |
| A | the glow's distance from the wall: 10, 20, 40, 80 cm |
| B | the far quad on / off (off shows why it is needed) |
| L or R trigger | move the divider |
| D-pad up held + Y | the effect labels on / off |
| Stick | the camera |
| START | between the result pages and the live view |

### Build

`make ao-demo SH4ZAM=...` builds the Dreamcast program (KallistiOS with `pvr_scene_begin_rtt`; python and a host C compiler make the room's data and its baked AO first). `make ao-preview` renders the halves on the host into `build/` (a software picture of the look: not the PVR's picture and no cost). `make test-ao` runs the host tests.

### What it costs

Measured on one console (Holly revision 0011, 2026-11-25, image `psm-suite-s6b`, program ao4-d), CLAY, K 4, reach 60 %. The ledger rows are in `wiki/hardware/`; the photographs are the result pages `PXL_20261125_174555888` and `174558739`.

| | Scene A | Scene B | Scene C | Status |
| --- | --- | --- | --- | --- |
| 1 BAKED adds, SH-4 | 69 us | 113 us | | MEASURED (SH4-077) |
| 1 BAKED adds, PVR | 0 | 0 | 0 | MEASURED (PVR-138: the frame does not change) |
| The bake itself, at load | 254 ms | 393 ms | at build time, on the host | MEASURED (SH4-076) |
| 2 PVR MAP adds to the frame, in the split | 10.2 ms | 7.1 ms | 9.4 ms | MEASURED (PVR-138) |
| A half sends, vertex AO against lightmap | | | 6,680 against 4,268 vertices | MEASURED (PVR-142) |
| The lightmaps' video memory | | | 64 KB | exact |
| 3+M adds to the frame (4 hens) | | | 3.5 ms | MEASURED (PVR-141) |

- **Read method 2's line with care.** In the split a half showing 2 pays a whole second target pass, and the frame carries all of it (PVR-139). A program that uses method 2 alone draws the K directions into the one shadow target it already has: about 1.8 ms (A) and 1.4 ms (B) more on the PVR, plus the SH-4's send of the directions (4.2 and 2.5 ms here). DERIVED from the page's parts, not measured as such.
- 3+M's 3.5 ms is nearly all the floor a render target pass costs whatever its size (about 3.2 ms).
- The baked byte is the cheapest by far and is the default for anything that does not move. A PVR map is for movers.

### What it does not do

- Methods 2 and 3+M darken the ground only: nothing on walls or on the shapes.
- Method 2 leaves a thin dark fringe at a wall's foot (the map is darkest under the wall and its filter carries that half a texel out). Not fixed.
- A baked method knows nothing of a mover: the floor under a hen stays as baked in 1 and 3.
- The room's trim (rails, panel mouldings) is lit with its wall's normal, so it shows by its baked AO alone: lit by its own normals, faces a pixel wide showed as dashed lines.
- Every number is from one run on one console.

Credits: `demo/psm/ao/CREDITS.md`. Lab notes: `demo/psm/ao/NOTES.md`.
