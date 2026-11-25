# demo/psm: `sh4der_bump.h` against the c3b and c4 measurements

Review note, 2026-11-25. Notes only: no code changed, nothing run. The question asked: could the library's bump module use the compare chain's results (a measured level table instead of the cosine; window edges measured at boot)?

The marks are the ledger's: MEASURED, FACT, REPORTED, ESTIMATE, OPEN. The IDs are from `wiki/hardware/`.

## Short answer

**Not directly. Since v0.14, `sh4der_bump.h` does not use the bump unit.**

- The header says so (FACT: `include/sh4der_bump.h:9-14`, and the v0.14 line at :43).
- Commit `f8e8222` removed the bump-unit pass, the hot pass and the offset-colour argument. Its message adds: "Built, not checked on a real console".
- The module keeps the PVR bump texel layout only as a storage format, so maps from pvrtex still load.
- The lighting is done by:
  - nine grey textures drawn as add passes (highlight power 2);
  - then the picture, blended DESTCOLOR x DESTCOLOR.

So c4's level-table finding (PVR-096) has nothing to act on in the module. **No change is recommended to the header for it.**

c3b and c4 did measure things that bear on the module's own passes. Those are below.

## What c3b / c4 measured that bears on the module

| Finding | ID | Bearing on `sh4der_bump.h` | Status of the bearing |
|---|---|---|---|
| A second pass at equal depth with "equal or nearer" leaves 0 holes | PVR-097 (MEASURED) | The module's grey passes and picture pass are drawn GEQUAL with no depth write over the base (AGENTS.md, "Call order: normal map"). c4 is the first console evidence that this pass structure leaves no holes, in another program. | ESTIMATE that it carries over |
| Blend products drop their low 8 bits; factor 255 counts as 256 | PVR-041 (c3b MEASURED at threshold 1; the rule REPORTED, skmp) | Every grey pass is (vertex weight x grey texel) added ONE x ONE; the picture pass multiplies twice. If each product loses up to one level, nine passes can leave the flat level up to about 9 levels under the 0.5 the method assumes. A flat texel would then come out a little darker than the picture. | ESTIMATE (worst case; the rounding per pass is REPORTED, not measured on these passes) |
| A palette entry is one value a scene | PVR-043 (MEASURED) | The grey maps are PAL8 through `sh4der_bump_grey_palette`. The ramp must be set once and not changed between passes. The header already says the palette format is one register for every bank. | consistent; no change |
| A full-screen translucent layer costs about 3,112 us at 640 x 480 | PVR-002 (MEASURED) | Nine grey passes and the picture over a whole screen would be about 31 ms of fill. The module cuts the floor into lit cells, so only the lit part pays. | ESTIMATE (ten layers x PVR-002) |
| Translucent paletted 8-bit layer cost | — | not measured: h1 timed paletted 8-bit only on the opaque list (3,034 us, PVR-006) | OPEN |
| A non-twiddled bump texture did not hang | PVR-070 (MEASURED, one run) | Not used: the module's textures are not drawn through the bump unit. | none |

## Could the module use the compare chain? Four ways, ranked

1. **Calibrate the flat level against the real blend rounding.** This is the useful one.
   - The highlight method rests on "a flat texel stays the picture" (header :18). With the reported per-product truncation, the flat level after nine passes may sit a few levels low.
   - A host model of the passes under skmp's rule (each product shifted right by 8, factor 255 as 256) would give the bias. The client would add it to the flat level, or the module would give a corrected `0.5 x (1 − strength x k)`.
   - Console check: one flat cell, the nine passes at known weights, the frame read back. The same read-back method as c4 test 1.
   - ESTIMATE that it is worth a level or more; OPEN until modelled.
2. **Bring the bump unit back for lighting, with a measured level table.**
   - If a client ever wants the hardware bump unit again (one pass instead of ten), take its level from a table measured at boot, not from Sega's K1 + K2 sin S + K3 cos S cos(R − Q).
   - For smooth lighting, a few codes off near the window's edges (443 of 40,960 pairs at window 32, PVR-096) would probably not show. For a threshold it does: that error is c4's acne.
   - So the boot measurement matters for compares, not for shading (ESTIMATE).
   - The measuring code belongs to the compare chain (c4b, Q56). A shared helper could serve both later.
3. **A toon highlight by thresholding the grey sum.** Not possible as built.
   - The grey passes sum into the frame's colour, a 565 screen with no alpha.
   - Punch-through and the 1555 threshold act on alpha (PVR-060, PVR-028).
   - A toon highlight would need the sum in a 1555 target's alpha: a second render target and a pass to lay it back. Not worth it against R1 of `RECIPES_NOTES.md` (a point-sampled ramp per vertex).
4. **Self-shadowing of the relief per pixel.** Out of reach.
   - A horizon map needs a per-pixel light angle against a per-texel horizon.
   - The bump comparator has one Q a polygon (PVR-045), and the module's cells carry one light direction each, so at best the cell gets a per-pixel compare against a single angle.
   - That is a texture-space compare (`RECIPES_NOTES.md` R7) with M = the texel's horizon angle toward a fixed azimuth. It works only for a light that turns in elevation alone. Not recommended.

## Open items on the module, unchanged by c3b / c4

- Which way the azimuth turns against u on a console (`frame.flip`; header :31-32).
- The 565 palette's banding.
- No console timing of the module's passes.

Both open items are in AGENTS.md "Call order: normal map". c3b and c4 did not test them.

## Tests that would settle the useful item

| Test | Where | What it gives |
|---|---|---|
| Host model of the nine grey passes and the picture pass under skmp's per-product rounding, against the real-number method | `test/bump_round_test.c` (not written) | the flat level's bias in levels, per strength |
| One flat cell drawn with the module's passes at known weights into a target, read back | a page of a console image (none named) | the measured bias; MEASURED once run |
