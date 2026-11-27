---
title: "Soft character shadows [sh4dow]: limb columns, foot cones and a heightfield sun"
tags: [effect, shadows, character, cpu, foot, heightfield]
updated: 2026-11-26
sources: [include/sh4dow_dc.h, src/sh4dow_dc.c, src/sh4dow_foot.c, include/sh4dow_heightfield.h, test/shade_test.c, test/foot_test.c, test/water_test.c, example/example.c, example/quickstart.c, example/feet.c, bench/bench.c, README.md "The sh4dow effect (soft character shadows)", AGENTS.md "Call order: sh4dow"]
---
# Soft character shadows [sh4dow]: limb columns, foot cones and a heightfield sun

**What you will learn:** how a limb casts a soft column of shade on a character's vertices, how the two foot cones are drawn, what the library's published numbers are worth, and which limits are not yet proven.
**For:** a programmer giving a skinned character soft self-shadow on the Dreamcast without a shadow map.

> **In plain words.** This makes soft shadows on a character, worked out by the processor for each vertex. Each limb (the head, a neck, an arm) casts a soft column of shade away from the light; a vertex inside a column gets darker. Under each foot a small cone of shade lies on the floor. There is no shadow picture and no extra draw, and it costs no video memory. Use it for a few characters where the processor has time to spare.

## Try this first

What it is for: soft shade on a character, per vertex, from its posed limbs.

1. Fill one `sh4dow_limb_t` per limb that casts: its joint, a sphere radius (in the vertices' units), its first-bone id, and `SH4DOW_LIMB_HEAD` / `SH4DOW_LIMB_NECK` on the head and neck (`include/sh4dow_dc.h`).
2. Start from the shipped tuning: `SH4DOW_INNER` 1.25, `SH4DOW_OUTER` 1.35, `SH4DOW_LENGTH` 1.8, `SH4DOW_HOLD` 0.8, `SH4DOW_DARK` 0.65, `SH4DOW_COLLAR` 0.8, `SH4DOW_FADE_IN` 0.1 (values from the header; README "The sh4dow effect", ADVISED).
3. `sh4dow_prepare` once per character per frame, then `sh4dow_shade` after lighting and before packing.
4. Foot cones are a second step: `sh4dow_pick_feet` once per model, then per foot per frame `sh4dow_foot_place` and `sh4dow_foot_cone`.

On a console: the shade's before and after numbers are REPORTED (below). The foot cones and the packed loops are NOT MEASURED on hardware.

Header: [include/sh4dow_dc.h](../../include/sh4dow_dc.h). Call order: [AGENTS.md "Call order: sh4dow"](../../AGENTS.md).

## What it does

A column is a cylinder that starts at a joint and runs along the light. A vertex inside a column is shaded: fully inside the core, fading to nothing at the outer radius, full along the first part of the length and fading at the end. Columns do not add: the darkest one wins. A column skips the vertices of its own bone (first bone only), and the other columns still shade them, so the limb is still shaded by the body. The colour is scaled, `rgb *= 1 - dark * strength`: a fully shaded vertex keeps `1 - dark` of its colour.

The head column can start at the neck, so the chin stays lit (the chin rule, flags on the head and neck).

The foot cone is a quad on the floor under each foot: a round heel that widens away from the light and fades out, long when the light is low and short when it is overhead. Its picture is a small texture the caller uploads once. The caller draws the quad, multiplied over the floor.

The heightfield sun (`include/sh4dow_heightfield.h`) is a separate sweep over a regular heightfield, for water and terrain. It is experimental.

## Call order

The step list is in [AGENTS.md "Call order: sh4dow"](../../AGENTS.md). In short: pose, light, fill the limbs, `sh4dow_prepare`, `sh4dow_shade`; for feet, `sh4dow_pick_feet`, `sh4dow_foot_place`, `sh4dow_foot_cone`.

## Cost

| Item | Cost | Status |
|---|---|---|
| The shade on a Dreamcast, before the column lists and the one-FTRV loop | 14.70 ms for one cutscene: six characters, 84 columns, 5,406 vertices, 33 ms frame | REPORTED: the library's author, one run; no ledger row yet (the README's section on cost, folded) |
| The shade after them, the same scene | 4.5 ms, about 0.83 us a vertex (about 166 SH-4 cycles at 200 MHz; 544 before) | REPORTED: the same run, the same author |
| Columns visited per vertex, a PC census | every column 13.3; sorted loop 5.2; column lists 2.6, over 1,063,664 skinned vertices under two lights | counted on the host (exact); not time |
| Packed lists, the per-bone loop | not measured on hardware; the bench (`bench/bench.c`) is built to time them | NOT MEASURED |
| Foot cone per foot per frame | a few dozen multiplies, a handful of dot products, three reciprocals | ESTIMATE from the code |
| RAM | `sh4dow_frame_t` about 3 KB, one per character drawn (or one reused); the foot picture 32 x 32 RGB565, 2 KB | exact, by the code |
| VRAM | 0 for the shade; the foot picture is 2 KB of texture | exact, by the code |

The 14.70 and 4.5 ms figures are a client's run, not a benchmark suite: the 4.5 ms includes the sorting, the column lists and the SH4ZAM routines changed together. A console figure for one client's scene is not a figure for another scene.

## Recommended limits

| Limit | Value | Why | Status | Scene |
|---|---|---|---|---|
| Columns a frame | `SH4DOW_COLUMN_MAX` 32 (a budget) | The frame holds that many; past it, limbs are ignored | ADVISED (the header's cap) | none |
| Limbs per character | the first client spends 14 on at most six characters | Budget set by the client, not by the maths | REPORTED (one client) | the cutscene above |
| Characters with a shade a frame | the client's choice | Cost scales with the vertices shaded | ADVISED | none |
| Radii in the vertices' space | required | A column the wrong size is a units error | ADVISED | none |
| Column lists for a ball | a ball that really holds the vertices | The list is exact only inside the ball | ADVISED (the header's rule) | none |
| `sh4dow_strength_list` on a column far thinner than its distance from the origin | keep such a limb out of the list loop, or use `sh4dow_strength` | Expanded square loses precision: about one colour step at 1:100, a wrong answer near 1:3000 | ADVISED (the header's note; test/shade_test.c) | none |
| Casters within a radius (N characters within R metres) | not set | The bench would set N and R (open question Q1) | OPEN | none |
| Foot cones on hardware | not proven | Cone not timed | NOT MEASURED | none |

A limit is PROVEN only when a measured number backs it (wiki STRUCTURE rule). None of these is PROVEN.

## Tuning

See the tuning tables in `README.md` "The sh4dow effect". The first knob is `SH4DOW_DARK` (how dark the shade gets). For a column that stains the chin, lower `SH4DOW_COLLAR`. For a flicker at the start of a column, raise `SH4DOW_FADE_IN`. The foot cone's length is `SH4DOW_FOOT_MIN_LEN` and `SH4DOW_FOOT_GAIN`.

## Pitfalls

- A shade that falls toward the light: `direction` is flipped. It is the way the shadow falls, away from the light.
- The whole character darkens: `SH4DOW_OUTER` times your radii is larger than the body.
- A foot cone points at the light: `foot.away` runs from the light toward the foot, the wrong way round.
- Cones vanish: the floor normal and `up` disagree (a wall), or the heel is more than `SH4DOW_FOOT_FADE_HEIGHT` above the plane.
- Bone skips: a vertex whose first bone is the limb's bone is not shaded by that limb, but the other columns still shade it.
- Pass `NULL` for `params` to get the shipped tuning; a bad or non-finite direction stores no columns and returns 0.

## Open

- Foot cones on a console: no run yet. Test: a Dreamcast scene with two cones, a timer round the cone calls, a photo of the floor.
- Packed loops on a console: the bench (`make bench SH4ZAM=...`) prints the three loops. Test: one console run, logged, against the every-column loop.
- The heightfield sun on a console: no figures. Test: the ocean demo's shadow pass timed on hardware.
- The ledger row for the 14.70 and 4.5 ms figures: none yet. Test: a console run of the bench, logged in `wiki/hardware/`.
- How many characters (N) within what distance (R) fit a 3 ms budget: the bench in [recommended limits](../limits/recommended-limits.md#bench-request-sh4dow-dc-n-casters--r-metres) (open question Q1).

## Related

- [aomesh](aomesh.md) and [aobake](aobake.md): baked occlusion, a different method (no per-frame columns).
- [sh4dow-dc](sh4dow-dc.md): the earlier name of this page; it now only points here.
- [What do you want to make?](../start-here/what-do-you-want.md): the starter, for a picture you have in mind.
- [Shadows and depth](../start-here/genre-shadows.md): the genre guide that puts this beside the other shadow methods.
- [sss](sss.md): transmission, which takes a visibility term such as the heightfield sun.
- [water](water.md): the ocean demo, which uses the heightfield sun.
- The library's own feather (`sh4dow_feather_in_`) is the soft edge of every effect: [core](core.md).
