# hero1: a skinned hero running a street between space hangars, the cost of each part

`hero_spike.c` draws one skinned figure running a street and times each of its parts on a real console. It answers one question: what does a main character with an opaque pass, a detail overlay, a visor pass and a shadow cost here?

- **The hero.** With no file, it is the original figure built in `hero_model.h`: a diver's suit, a round helmet with a slit visor, 33 bones, 1,465 vertices, 1,660 triangles, two clips of its own.
- **The set.** The space stretch: a straight road between hangars, 2,671 triangles in 21 objects, vertex colours with the light baked. `tools/bake_strip.py` makes it from Space Kit (2.0) by Kenney (www.kenney.nl), CC0 1.0. The kit is read from its zip as downloaded (sha256 checked); nothing of it is kept in this tree, and the header is a build product.
- **The run.** He runs the road end to end and back at 5.2 m/s and turns about at each end. That speed is how fast the RUN clip's foot goes back while it is down (measured on the host from the clip), so the feet should not slide.
- **The hens.** Three stand beside the road, look about and peck. Each has a flat blob under it: a fan of 8 triangles in the translucent list. They are not in the shadow target, so nothing pops in or out as he passes. The hen is the town's (`tools/hero_hens.py` copies it out of the town's mesh header; `hero_hens.h` is a build product).
- **The camera and the shadow.** The camera stays in the street and follows him. The shadow target's window is centred on him every frame.
- **Nothing goes missing.** No stage turns the hero or his shadow off. A set triangle that crosses the near plane is cut there, not dropped. `test/hero_strip_test.c` replays the autorun on the host and fails if he or his shadow is ever hidden, by the set or by a hen.

## Files

| File | What |
|---|---|
| `hero_model.h` | the original hero (mesh, clips, textures) and a frame's steps: pose, palette, skin, light, the visor's lookup |
| `hero_file.h` | HERO.BIN: write and read (the format below) |
| `hero_spike.c` | the Dreamcast program: passes, controls, costs, the autorun and its pages |
| `hero_strip.h` | the street: the run, his place, the shadow window, which objects are in view, the near clip, the autorun's stages and camera |
| `tools/bake_strip.py` | bakes the street from the kit's zip to `hero_strip_data.h` (`--self-test` checks the tool) |
| `test/hero_strip_test.c` | host test: the autorun replayed; fails if the hero or his shadow is ever hidden |
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
| Y | he runs, or he stands at the street's start |
| D-pad up | the view from the other end of the street |
| D-pad down | the hens |
| D-pad right | the effect labels, hidden or shown |
| L trigger | zoom out |
| R trigger | zoom in |
| stick | orbit the camera (kept in the street) |
| START | the result pages |

On the pages, START goes back to the live view. A+B+X+Y goes back to the suite menu.

## The autorun

The program runs by itself first, then writes the result pages and a graph. The stages are the table in `hero_strip.h`. The opaque pass, the shadow and the hens are on in every one:

1. Everything on, he stands (4 s).
2. Everything on, he runs a whole lap (16 s; a lap is 15.4 s).
3. He runs, no detail (5 s).
4. He runs, no visor (5 s).
5. He runs, the camera farther off (5 s).

Each stage starts the run and the camera's swing again, so stages 3 to 5 see what the first 5 s of stage 2 saw.

- **By its own timer** (stage 2): the hero's SH-4 time, the set's, the shadow's, and the shadow target's PVR render.
- **Derived**: detail and visor are stage 2's first 5 s less the stage without the pass.
- **Not given**: the hero's and the set's share of the screen's PVR render, which is not apart.

Every number comes from the run that prints it, and only a run on a real console is evidence.
