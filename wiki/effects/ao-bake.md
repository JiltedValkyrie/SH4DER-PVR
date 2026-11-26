---
title: "Bake tool [ao-bake]: baked occlusion over any triangle list (host tool)"
tags: [tool, ao, bake, host]
updated: 2026-11-26
sources: [tools/ao_bake_core.h, test/ao_bake_test.c, demo/pl4nar/tools/map_ao.c, demo/psm/ao/tools/room_bake.c, demo/psm/ao/build.py]
---
# Bake tool [ao-bake]: baked occlusion over any triangle list (host tool)

**What you will learn:** how a build-time tool bakes one byte of ambient occlusion a vertex from a triangle list, what the inside-solid fill and the street wrap do, and what the host run proves.
**For:** a programmer baking a level's vertex occlusion on the host before the Dreamcast build, or adding a client to the two that exist.

## What it does

A host-only, libc-only header (`tools/ao_bake_core.h`, not a console header). A point casts 64 cosine-weighted rays over its hemisphere, each against the occluder triangles whose bounding sphere comes within 2.5 m. A hit at distance `t` takes `1 - t / R` of a ray. The byte is `255 (1 - sum / rays)`. The rules are those of `demo/psm/ao/tools/room_bake.c`, for a mesh that is not the room.

Three extras for real scenes:

- **Inside a solid.** When more than a quarter of the rays meet the back of a closed triangle first, the point sits inside a closed object (sunk into a wall, a prop pushed into a building). `aob_at_in` bakes it a little way into its own triangles; `aob_fill` takes the mean of unflagged neighbours on a grid.
- **Wrap.** `aob_wrap` adds the occluders near either end of a street that repeats every `period` metres again on the far side, so the two ends of a seam bake the same byte.
- **Ray set.** The 64 directions are a radical-inverse set built in `aob_init`; they are not the console's set (see Open).

## Call order

Host build tools only; the console never calls it.

1. `aob_init(&b, ot, closed, n)`: the occluders (caller's arrays, kept). Returns 0 on no memory.
2. Per vertex: `aob_at(&b, p, n, &inside)` gives the open share 0..1; `aob_byte` makes the byte. If `inside`, try `aob_at_in` for the object's own vertices, or leave the flag for `aob_fill`.
3. A street: `aob_wrap` first, then `aob_init` on the wrapped list.
4. `aob_free(&b)`.

`demo/pl4nar/tools/map_ao.c` is the client that runs it: 38 models at 2.5 m, and a 2 m road grid at 10 m reach (scaled by `AOB_R / 10`). Its output was checked this session: a host run into a scratch folder gave a file identical to the existing `build/gen_map/map_ao_gen.c`.

## Cost

| Item | Cost | Status |
|---|---|---|
| Host time for a scene | not measured for this core; no demo README or log states one | OPEN |
| Console | none: the program keeps the baked bytes, one a vertex (the client's) | as-stated |
| Load-time bake on the console | a different routine (`demo/psm/ao/ao4_scene.h`, ao4_rays): 254 ms and 393 ms, SH4-076 | MEASURED, other code |

## Recommended limits

- `AOB_RAYS` 64 rays a point and `AOB_R` 2.5 m for a room or a street; the road bake uses 10 m at a scale of `AOB_R / 10`. ADVISED: it matches the room's bytes to 2 (see Tests).
- `AOB_INSIDE` 0.25: a point is inside when more than a quarter of its rays meet a closed back first. ADVISED.
- Bake at build time only. Do not ship it in a console build.

## Tests

`test/ao_bake_test.c`, host, plain C, run through `make test-demo` once wired (see Open):

- the placeholder room's 1,905 vertices against `room_ao_v`: 0 differ by more than 2 bytes (worst 0). Host run, 2026-11-26.
- the wrap: a floor point just under the period's end gives 0.9297, the same point before 0 with the wrap 0.9297, with no wrap 1.0000 (the box is not seen). Host run, 2026-11-26.
- the seam: z 0 and z period both 199.
- the fill: a 5 x 5 grid with a 3 x 3 flagged block fills back to 200 everywhere.

These are host checks against the room's own baked bytes. They are not a console result.

## Pitfalls

- A closed object's vertex sunk into a wall bakes black unless filled or baked inside. In the placeholder room, room_bake's run baked 32 table vertices and 8 sofa vertices inside, and filled 20 floor, 24 left-wall and 23 right-wall vertices from beside them.
- The wrap copies only the occluders within `AOB_R` of an end. Anything further out is not seen, by design.
- A byte is a multiply on ambient light, not physical light.

## Open

- **Duplicate ray set.** `demo/psm/ao/ao4_scene.h` has its own fixed Hammersley set (`ao4_rays`, `ao4_at`) for the console. It is the same rule with its own rays. A later job merges them into one core; nothing is merged yet.
- **Host time not measured.** Put a number here when a build log gives one.
- **No client in psm yet.** The psm demo's own bake is `demo/psm/psm_ao.h` (`psm_ao_bake`), a separate routine. The task named psm tools as clients; none includes this header.
- **Not in the Makefile yet.** `make test-demo` does not run `test/ao_bake_test.c`. It needs the room's data first (`demo/psm/ao/build.py --room-only --out <dir>`).

## Related

- [Corner darkening](ao.md): small-scale occlusion in `include/sh4der_ao.h` (crease darkening and a screen cavity cue). A different thing: nothing is shared.
- [Planar reflection](pl4nar.md): the street demo whose road grid is baked by `map_ao.c`.
- [Shared parts](core.md)
