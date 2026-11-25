# demo/psm: what polly2-rtl and dreamcast-tests say about the PVR rules we use

Research only. Both repositories were read on 2026-11-25 from a shallow clone in a scratch folder outside this tree. No code, tables or files were copied into this repository. Rules are given in words with a pointer to the place they are stated.

**Repositories read.**

- `github.com/skmp/polly2-rtl`, branch `r8/develop`, commit `53a62f59` (2026-11-25).
- `github.com/skmp/dreamcast-tests`, commit `a9329805` (2026-11-20).

**Paths.**

- `P` = `polly2-rtl/docs/spec/`.
- `T` = `dreamcast-tests/README.md`.
- Line numbers are those of the commits above.

**Marks.**

- **REPORTED**: polly2-rtl's spec states it. Its own tags matter:
  - `[M test]`: "measured on a console by that dreamcast-tests test" (`P/tsp.md:47`). Their console, not ours.
  - **GAP**: what refsw3 does with no console test behind it.
- **MEASURED (ours)**: our own console runs, from `compare/NOTES.md` (c1, c3b).
- **FACT**: arithmetic done here on their stated rule, or facts about our own files.
- **UNKNOWN**: not found.

**What these repos are** (REPORTED, polly2-rtl `README.md`, `P/tsp.md:12-14`):

- a SystemVerilog model of the PowerVR2 CORE, said to be bit-exact and cycle-accurate;
- its rules are transcribed from refsw3 (nulldc3 `refsw3/refcore`), "the console, bit for bit, on every regression test and on all 786 polly2-data dumps";
- each rule names the dreamcast-tests test that pins it.

## Licence

- **No licence file** in either checkout. FACT: the root of polly2-rtl is `.gitignore Makefile README.md docs rtl tb tools`.
- For polly2-rtl, GitHub's repository API reports "Other" with spdx `NOASSERTION`, but `GET /repos/skmp/polly2-rtl/license` returns 404. For dreamcast-tests no licence is reported.
- With no licence granted, the default is all rights reserved. That is inference, not legal advice.
- **So:**
  - read and cite them;
  - do not copy their files, tables or code;
  - ask skmp before using any of it in our code.

  This matches lead's "no code copied".

## (1) The bump unit

- **Formula** (REPORTED, `P/tsp.md:846-861`, [M bumpfn 952 regions, bump, bumplight]):
  - **I = clamp(K1 + K2·SIN[S] + ±K3·COS[S]·across(|R − Q|), 0, 255)**;
  - each product drops its low 8 bits before the next step;
  - `across` is a quarter-wave lookup of the COS table, indexed by the low 6 bits of a = |R − Q|. Its sign comes from the quadrant a >> 6: negative in quadrants 1 and 2;
  - the result texel is white RGB with alpha I.
- **The tables are measured, not computed sines.** Two 256-entry tables, "each is a line per 8 entries, not a sine" (`P/tsp.md:856-858`, Appendix B `P/tsp.md:1256-1296`). SIN[0] and COS[255] are not pinned.
- **Sources of S, R, K and Q** (REPORTED, `P/tsp.md:848-849`):
  - texel byte 0 = R (the angle), byte 1 = S;
  - the light is the offset colour: **K1 = A, K2 = R, K3 = G, Q = B**.
- **Q is per triangle**: the **last vertex's offset colour "as sent"**, not interpolated on Gouraud and not through the 1/w planes on flat polygons (REPORTED, `P/tsp.md:884-889`, [M supsample2 p.30, bumplight]; `T:1757-1765`). With an intensity shadow inside a volume it is the scaled v3 offset colour.
- **No offset is added to a bump texel.**
  - The combiner uses I in place of the texel (`P/tsp.md:860-861, 876`).
  - With modulate, the base colour is multiplied by I. This is a console result that refsw2 had wrong (`T:183-188`).
  - No vertex fog for a bump map; table fog applies (`P/tsp.md:929-931`).
- **R is filtered "round the circle" under bilinear filtering.** The shorter way between two texels is taken; S is filtered as a colour (REPORTED, `P/tsp.md:827-838`, [M bumpbil]).
- **The light persists** (REPORTED, `P/tsp.md:878-901`, [M offspersist, bumplight]):
  - `offsKept`: the last textured, offset-bit primitive's light stays in a register across tiles, renders and soft resets;
  - it is 0 after power-on;
  - a bump map **without** the Offset bit is lit by it.

  This is a trap for a compare pass: always set the Offset bit on our bump polygons.

**Against our c3b (FACT, arithmetic here).** The rule was run with our c3b settings:

- K1 128, K2 0, K3 127, S 0;
- R = M − 64 (`compare/sub_model.h:44`);
- M = 128.

| M − Q | polly2 I | polly2, 6-bit | console c3b 1b | match |
|---|---|---|---|---|
| 92 | 222 | 220 | 223 | yes |
| 28 | 207 | 204 | 207 | yes |
| 4 | 142 | 140 | 142 | yes |
| −4 | 111 | 108 | 109 | **yes** |
| −4 | 111 | 108 | 109 | **yes** |
| −36 | 32 | 32 | 28 | one 6-bit step low |
| −100 | 49 | 48 | 48 | yes |
| 1a: Q = 255 | 125 | 124 | 125 | yes |

- The strip c3b called "low, not explained" (109 where our cos model had 116) is the measured tables' value.
- The one miss, M − Q = −36, is not explained here.
- At M = Q, the rule gives I = 126. That is just under the 128 threshold, so the result is deterministic. c3b's "equal depths undecided" therefore does not come from the bump arithmetic. One candidate is which texel a pixel samples (INF, untested).

## (2) The punch-through alpha test

- **Rule** (REPORTED, `P/tsp.md:1071-1081`, [M alphatest, fbpack, dumprules cell 22]; `T:171-175`):
  - **alpha < PT_ALPHA_REF fails**: alpha is set to 0;
  - **otherwise it passes**: alpha is set to 255;
  - the threshold is the same for every alpha at references 0, 1, 127, 128 and 255, whether alpha comes from the texture or the vertex;
  - only bits 7..0 are pinned; higher bits are a GAP.
- **The blend still runs on a failing pixel.**
  - "A failing pixel drawn with ONE/ZERO writes alpha 0 [M fbpack]".
  - With SRC_ALPHA / INV_SRC_ALPHA it leaves the pixel [M dumprules].
- **Inside the PT list the depth mode does nothing, and ZWriteDis is ignored** (REPORTED, `P/isp.md:1394-1417`, [M depth page 0, dumprules page 8]).
- **What explains our 5b and 5c (INF).**
  - Their headers used blend ONE/ZERO, depth "always" and no depth write (`compare/NOTES.md:149-151`). By the rule above, failing pixels are still written, with alpha 0.
  - So "65,536 drawn" is the expected picture, not a dead alpha test.
  - 5a used KallistiOS's default SRC_ALPHA blend and worked.
  - Test to confirm: rerun 5b with SRC_ALPHA / INV_SRC_ALPHA.
- **Modulate rounding.** Texel 255 × vertex 255 gives 254 (REPORTED, `P/tsp.md:869`, `T:174`).

## (3) The fog table

REPORTED, `P/tsp.md:907-931`, [M tspw, fog, wmeas, slivers_dzspec].

- **Input.** W is the TSP's own 1/w, with a 16-bit mantissa.
  - FOG_DENSITY is an 8-bit mantissa in 1.7 format with a signed 8-bit exponent.
  - fogW = density × W, clamped to [1, 256): below 1 becomes 1, and 256 or more becomes 256 − 2⁻¹⁶.
  - W ≤ 0, or a density mantissa of 0, gives fogW = 1.
- **Index.** fogW as an IEEE single gives the 7-bit index: the low 3 bits of (exponent + 1), then the top 4 fraction bits. That is 16 entries per binade, from binade 1..2 at index 0.
- **Blend factor.** The next 8 fraction bits are the blend factor bf.
- **Value.** Each entry holds two bytes. fog alpha = (lo·bf + hi·(256 − bf)) >> 8.
- **Use by FogCtrl.**
  - 00 table: rgb = (rgb·(256 − fa) + FOG_COL_RAM·fa) >> 8, alpha unchanged.
  - 11 table mode 2: rgb = FOG_COL_RAM, a = fog alpha.
  - 01 vertex: uses the offset alpha; none for bump.
  - 10 none.
- **Against our c3b 6a and 6b.** "Far end first" and "16 entries an octave" agree. Our "KOS packs the fog scale with 7 bits of mantissa, cut" agrees with a 1.7 mantissa (FACT: consistent; KOS's packing itself is not re-checked here).

## (4) Depth: what is stored and how it compares

- **What is stored** (REPORTED, `P/isp.md:1254-1368`, [T zmeas, zblock, zexfil: 64 dumps × 307,200 depths read out]).
  - The depth is 1/w.
  - A positive depth is stored as an IEEE single **cut (truncated) to 19 significant bits on the pixel's own exponent**; the low 5 fraction bits are zero.
  - +0 and −0 are equal.
  - Negative depths and depths above 2¹²⁸ have their own ordered places.
  - The ISP compares order keys, not floats.
  - The hardware's storage width is not known; "depth about 28 bits" is marked [I] (`P/isp.md:1384-1392`).
- **Modes** (REPORTED, `P/isp.md:1394-1417`, [T depth page 0: 336 of 336]).
  - The 8 modes are new < old … new ≥ old, with bigger meaning nearer. **"Greater or equal" = mode 6: draw when new ≥ old.**
  - Opaque and pre-sorted translucent lists use the polygon's mode.
  - **Modifier volumes always use mode 4 (strictly greater), whatever the word says.** A volume triangle at exactly the pixel's depth is not in front of it.
  - The PT list and auto-sort translucent list do not use the mode; they use their own walks.
- **Ties in the sorted walks** (REPORTED, `P/isp.md:1419-1446`, [T trtie]): ties are broken by the tag's list order. "No two fragments are ever equal."

## (5) Write-out pack, dither and the 1555 threshold

REPORTED, `P/tsp.md:990-1010`, [M fbpack 5,222,400 of 5,222,400; fb555, fb565].

- **1555.** A = (accumulated alpha ≥ fb_alpha_threshold, FB_W_CTRL 23..16). Not dithered.
- **4444.** Alpha **is** dithered.
- **Without dither.** Plain truncation.
- **The 4 × 4 dither table** is in the spec (not copied here).
- **FB_W_CTRL.** packmode 2..0, dither bit 3, kval 15..8, threshold 23..16 (`P/tsp.md:84`).
- **Ours.** This agrees with c3b test 2 (MEASURED: threshold exact at 1, 2 and 128; 4444 keeps the top 4 bits).
- **Render to texture in 1555 and 4444 is a GAP for them** (`P/tsp.md:1010`, `P/tsp.md:1217`, `T:3602-3606`). Our c3b test 2 (1555 target) and the 4444 row are console data they lack, and worth offering back.

## (6) Twiddle, and bump textures that are not twiddled

- **Twiddle rule** (REPORTED, `P/tsp.md:699-719`):
  - interleave v and u bits, v in the even bits, up to the shorter side; the longer side's remaining bits go above;
  - palette formats are always twiddled;
  - with ScanOrder (raster), the MipMapped and VQ bits do not change where a texel is fetched.
- **The bump hang is narrower than Sega's errata line** (REPORTED, `P/tsp.md:840-843`, `T:3424-3436`, [M bumphang]):
  - **only a non-twiddled bump map that is filtered bilinearly** stops the render;
  - point-sampled, twiddled or VQ bump maps render to the end;
  - quads are counted per tile covered: the render stops at the **16th in the translucent list, the 8th in opaque and punch-through**;
  - **the count carries across renders until a core reset**;
  - "Why is not known."
- **Ours.** c3b 1c and 1d used a non-twiddled bump map **with point sampling** (`compare/sub_spike.c:164`, `PVR_FILTER_NEAREST`), and did not hang. That agrees.
- **Correction for `PVR_FEATURES.md`.** "#26 forces a twiddle" becomes "#26 forbids bilinear on a non-twiddled bump map; point sampling is fine (their bumphang test; our c3b one run)".

## (7) Per polygon or per pixel

| Input | Where it comes from | Source |
|---|---|---|
| K1, K2, K3, Q | the last vertex's offset colour as sent, per triangle | `P/tsp.md:884-889` |
| S, R | per texel; R filtered round the circle | `P/tsp.md:827-838, 849` |
| base colour | Gouraud planes or flat; colour rounding | `P/tsp.md:395-401, 536` |
| fog W | the TSP's own 1/w at the pixel | `P/tsp.md:907` |
| depth | per pixel, stored at 19 bits | `P/isp.md:1254` |
| alpha test, blend | per pixel | `P/tsp.md:1071-1094` |
| offsKept | one register across tiles and renders | `P/tsp.md:891-895` |

## (8) Against PVR_FEATURES.md and our results

| Ours | polly2 / dreamcast-tests | Verdict |
|---|---|---|
| Rows 1a and 139: PT depth mode, GreaterEq vs LessEq (the Sega doc contradicts itself) | In the PT list the mode does nothing; ZWriteDis is ignored there (`P/isp.md:1394-1417`) | **Settled** by their console test. |
| Rows 8 and 138: PT rule, ≥ ref or only alpha 1.0 | ≥ ref passes and sets alpha to 255; a fail sets alpha to 0, and the blend still runs (`P/tsp.md:1071-1081`) | **Settled.** Both Sega lines are halves of one rule. c1 C3 and c3b 5b/5c fit "blend still runs" (INF). |
| Row 11: bump formula (Sega) | Same shape, with measured piecewise tables, truncated products, \|R − Q\| and v3's offset colour | **Refined.** It fits our c3b 1b to 6-bit at 6 of 7 samples. |
| Row 11a: bump flat or Gouraud | The light is never interpolated; the base colour is used in modulate | Partly settled. Gouraud *base* with bump: UNKNOWN. |
| Row 12: fog | Exact index and blend arithmetic (`P/tsp.md:907-920`) | **Agrees.** Exact. |
| Row 14: blend "clamped" | Each product >> 8 on its own, then added and saturated; a factor of 255 counts as 256 (`P/tsp.md:1083-1094`, [M blend 4096 of 4096]) | **Refined.** It also **predicts c1's +1** (FACT, arithmetic): our doubling layer (src DESTCOLOR on white, dst ONE) adds (255·d) >> 8, which is 0 for d = 1, so a difference of 1 never grows. That is c3b's finding. |
| Row 18: write-out | Same, plus the exact dither table | **Agrees.** |
| Rows 23 and 137: non-twiddled bump vs CORE 26 | It hangs only with bilinear filtering (section 6) | **Settled.** Our "#26 forces a twiddle" is too strict. |
| Row 107: 565 target read as bump, R = low byte | Byte 0 = R, byte 1 = S | **Agrees** (c3b 1d MEASURED). |
| c3b "Q is one value a polygon" | v3's offset colour as sent | **Agrees.** |
| c3b 6a "a multiply that takes 1.0 as 255/256" | Modulate 255 × 255 = 254; the fog mode 00 blend is ×(256 − fa) >> 8 | A candidate cause; which fog mode 6a used was not re-checked (INF). |

Nothing found contradicts our MEASURED results.

## Could not read

- polly2-rtl's README lists `model/`, `tools/hw/` and `agents/`. **They are not in this commit**: only `tools/check_*.sh`, `gate/`, `gen/` and `p2-offline/` exist.
- refsw3 source (`nulldc3`, public) was **not read**. Every refsw3 rule above comes through polly2's transcription.
- Read in part only: `docs/timing.md`, `ARCHITECTURE.md`, `PLAN.md`, the RTL itself, and dreamcast-tests' test sources and `PIPELINE_SHAPE.md`.
- Their results are from their console(s). dreamcast-tests lists "everything on more than one console" as not covered (`T:3602-3606`).
