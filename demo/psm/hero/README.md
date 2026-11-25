# hero1: a skinned hero on a clay set, the cost of each pass

`hero_spike.c` draws one skinned figure in a clay set and times each of its passes on a real console. It answers one question: what does a main character with an opaque pass, a detail overlay, a visor pass and a shadow cost here?

- **The hero.** With no file, it is the original figure built in `hero_model.h`: a diver's suit, a round helmet with a slit visor, 33 bones, 1,465 vertices, 1,660 triangles, two clips of its own.
- **The set.** One light-grey diffuse: a ground and two walls. The room (`ao/room_data.h`, made by `ao/build.py --room-only`) is the second set.

## Files

| File | What |
|---|---|
| `hero_model.h` | the original hero (mesh, clips, textures) and a frame's steps: pose, palette, skin, light, the visor's lookup |
| `hero_file.h` | HERO.BIN: write and read (the format below) |
| `hero_spike.c` | the Dreamcast program: passes, controls, costs, the autorun and its pages |
| `test/hero_model_test.c` | host test: budget, indices, weights, clips loop, skin against a direct blend, file round trip |
| `tools/hero_check.py` | checks a HERO.BIN by itself (`--self-test` checks the checker) |

## Swapping in other art: HERO.BIN

At start the program opens `/cd/HERO.BIN`. If the file is there and valid, it draws that file. Otherwise it draws the built-in hero, and the first text line and the result page say which one ran.

Keep your own files in `demo/psm/assets/hero/`, which git ignores, and never commit them. Run `python demo/psm/hero/tools/hero_check.py <file>` before you put a file on a disc.

### Format, version 1

Little-endian. Every block is padded with zeros to a multiple of 4 bytes.

**Header.** 32 words of `uint32`:

| Word | Value |
|---|---|
| 0 | `0x4F524548` ("HERO") |
| 1 | version, 1 |
| 2 | bones, 33 |
| 3 | vertices V, under 2048 |
| 4 | indices I, under 8192 |
| 5 | strips S, under 1024 |
| 6 | the first visor strip (strips from here to S are the visor) |
| 7 | clips, 2 (0 IDLE, 1 RUN+AIM) |
| 8 | frames a second, 30 |
| 9, 10 | frames in clip 0, clip 1 (2 .. 64 each) |
| 11 .. 19 | for the base, detail and env textures in turn: width, height, format (0 RGB565, 1 ARGB4444); sides are powers of 2, 8 .. 1024, at most 16,384 texels |
| 20 .. 31 | 0 |

**Blocks.** In this order:

| Block | Type | Count |
|---|---|---|
| parent | `uint8` | 33; 255 for the root; a parent comes before its child |
| head (each bone's joint at rest) | `float[3]` | 33 |
| position (rest pose, metres, y up, the figure facing +z) | `float[3]` | V |
| normal (unit) | `float[3]` | V |
| uv (base texture, 0 .. 1) | `float[2]` | V |
| bone 0 | `uint8` | V |
| bone 1 | `uint8` | V |
| weight on bone 0 (0 .. 255; bone 1 gets the rest; bone 1 = bone 0 is rigid) | `uint8` | V |
| strip indices | `uint16` | I |
| strip lengths (3 or more, summing to I) | `uint16` | S |
| clip 0, then clip 1: for each frame, for each bone, the skin matrix (rest to posed, 3 rows of 4, row-major) | `float[12]` | frames x 33 each |
| base, detail, env texels (row by row, not twiddled) | `uint16` | w x h each |

**Vertex order.** Vertices are sorted by (bone 0, bone 1). The program loads a matrix once per run of equal pairs, and the reader refuses an unsorted file.

**Textures.**
- The detail texture is grey about 0.5. It is drawn as a 2 x multiply, so 0.5 changes nothing.
- The env texture is a sphere map.

## Controls

| Button | What it does |
|---|---|
| X | the opaque pass on or off |
| D-pad left | the detail pass |
| B | the visor pass |
| A | the shadow |
| Y | the clip: IDLE or RUN+AIM |
| D-pad up | the set: clay, or the room |
| L trigger | zoom out |
| R trigger | zoom in |
| stick | orbit the camera |
| START | the result pages |

On the pages, START goes back to the live view. A+B+X+Y goes back to the suite menu.

## The autorun

The program runs by itself first. It holds each stage for `HERO_AUTO_S` seconds while the camera orbits on a script, then writes the result pages and a graph. The stages, in order:

1. Everything on, IDLE.
2. Everything on, RUN+AIM.
3. Opaque only.
4. Opaque and detail.
5. Opaque and visor.
6. Opaque and shadow.
7. No hero (the set alone).
8. Everything on, in the room.
9. Everything on, at twice the distance.

**What each pass adds** is a stage less stage 3. The hero's own cost is stage 3 less stage 7. Every number comes from the run that prints it, and only a run on a real console is evidence.
