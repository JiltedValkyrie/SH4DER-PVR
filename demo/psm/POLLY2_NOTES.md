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

## Spike c4 checked against skmp's rules (2026-11-25)

**What was read.**

- The local read-only copies `third_party/skmp/polly2-rtl` (`53a62f5`) and `third_party/skmp/dreamcast-tests` (`a932980`). `P` and `T` mean the same files as above.
- `compare/c4_spike.c` and `compare/c4_model.h`, for what c4 assumes.
- No c4 console page is in the repository, so this checks c4's **design** against the rules.
- Senior's own diagnosis text was not seen. The five candidates are as lead stated them.
- Nothing was copied. The level numbers below were computed here from polly's stated rule, in a scratch script outside this tree.

### (a) Where a pixel samples, and map pass against mask pass

- **KallistiOS writes HALF_OFFSET = 7** (FACT, `KOS/kernel/arch/dreamcast/hardware/pvr/pvr_init_shutdown.c:165`). The three bits do this:
  - bit 0: the ISP samples coverage and depth at **pixel centres** (REPORTED, `P/isp.md:138, 651, 986-988`);
  - bit 1: the TSP samples at pixel (x, y) **+ half a pixel** (REPORTED, `P/tsp.md:89, 439`);
  - bit 2: half a texel is taken off, **for bilinear only** (REPORTED, `P/tsp.md:659`).
  - dreamcast-tests `halfofs` pins all eight register values on a console (REPORTED, `T:450-465`).
- **Point sampling.** The texel is floor(u × size), from a coordinate held in 1/256 texel and rounded toward minus infinity. There is no half-texel shift (REPORTED, `P/tsp.md:650, 659, 687`).
- **c4 is consistent: no shift between the passes** (FACT for our code, INF for the conclusion).
  - The map pass draws map pixel (i, j) from its centre (i + ½, j + ½).
  - The mask pass gives each vertex u = m / 128, with m the unrounded map position (`compare/c4_spike.c:433`, `c4_model.h:142-146`).
  - So a pixel at map position m reads texel floor(m), whose value was sampled at floor(m) + ½. The error is −½ .. +½ map pixel, **unbiased**.
- **One known bias.** The divider gives 1/1.0 = 0.FFFF, so "a coordinate on a texel boundary shows the texel before it" (REPORTED, `P/tsp.md:498`). This is a pull of about 2⁻¹⁶ toward lower texels, at boundaries only.
- **The screen layer** reads the mask bilinear, with the half texel taken off (bit 2). Its texel centres line up with the target's pixel centres. FACT on c4's flags, `c4_spike.c:212`.

### (b) The ramp lookup with perspective (u = 1/(2z), then the code by u·z)

All of these are REPORTED.

- **Planes.** u enters as a plane of u/w, which is divided per pixel by the TSP's own 1/w plane (`P/tsp.md:181`).
- **Plane values.** Both plane values come out with a **16-bit mantissa**, the lost bits dropped toward minus infinity (`P/tsp.md:472`). They sit on a per-tile grid 22 bits under the tile's largest exponent (`P/tsp.md:432`).
- **Divider.** A 16-bit reciprocal from a table, then a product floored at 17 bits (`P/tsp.md:489-534`).
- **Texture coordinate.** floor(q × 2^(10+8)) for 1024 texels (`P/tsp.md:650`).

**What that does to c4** (INF, not measured):

- u is good to a few units of 2⁻¹⁶ relative, with a small low bias. Near the light (u ≈ 1, one code a texel) that is about 0.02 to 0.05 of a texel.
- A pixel reads the wrong code only when its true depth lies that close to a texel edge.
- c4 puts each code at the middle of its texel (`c4_spike.c:154-155, 173`), which is the right design against this.
- No rule found gives a **systematic** whole-code error. The model's "a quarter of the pixels near the light one code off" comes from the ramp's own quantisation; polly2's divider adds only a little to it.
- The map's colour path is exact, so codes survive the round trip.
  - The map pass is decal (REPLACE), point sampled, with no fog (`c4_spike.c:194-195`).
  - 565 widens by bit replication and packs by truncation, which gives the same bits back (REPORTED, `P/tsp.md:774`, `P/tsp.md:990-1010`).
  - Our c3b 1d MEASURED 0 wrong codes of 65,536.

### (c) Bump levels near c4's operating point (K1 128, K3 127, S 0)

Computed from polly2's rule and tables (REPORTED rule `P/tsp.md:846-861`, Appendix B; FACT arithmetic).

- **The texel is byte 0.** R = the map code, and S = 0 for a 565 target whose high byte is 0. So K2 does nothing; c4 sets K2 to 0 anyway.

| a = \|R − Q\| | 64 | 92 | 96 | 97 | 98 | 100 | 102 | 128 |
|---|---|---|---|---|---|---|---|---|
| polly2 | 125 | 47 | 40 | 38 | 35 | 32 | 28 | **3** |
| c4_level (our cosine) | 128 | 47 | 38 | 36 | 34 | 30 | 26 | **1** |

- **Slope at the w = 32 threshold (38).** polly2 falls about 2.5 levels a code, our cosine about 2.
- **Lowest level.** polly2's is 3, not 1: (127 × 255) >> 8 = 126, then (126 × 255) >> 8 = 125, and 128 − 125 = 3. `c4_threshold[16] = 10` therefore sits on a flatter, higher floor.
- **The lit window** (level under the threshold), FACT:

  | Half-width w | Threshold | polly2 window | c4 model window |
  |---|---|---|---|
  | 16 | 10 | a = 116 .. 139 (24 codes) | 114 .. 142 (29 codes) |
  | 32 | 38 | 98 .. 157 (60 codes) | 97 .. 159 (63 codes) |
  | 64 | 128 | 64 .. 191 (128 codes) | 65 .. 191 (127 codes) |

  So under polly2's rule the window is **1 to 3 codes narrower at w = 32** and **5 narrower at w = 16**.
- **polly2 takes |R − Q| as plain integers, not (R − Q) mod 256.** I(a) and I(256 − a) differ by one table step, 2 to 3 levels near the threshold. For example I(97) = 38 and I(159) = 40.
  - c4's Q = R + (w − b) + 128 wraps past 255 for large R.
  - So one edge of the window moves by about one code depending on whether Q wrapped (INF).
- **c3b's two odd readings.**
  - "109 where the cosine says 116" is a = 68. polly2 gives 111, which is the console's 6-bit 108 .. 111: **explained**.
  - "28 where the cosine says 30" is a = 100. polly2 gives 32, while the console read 28 .. 31: **not explained**. It needs a = 101 .. 102. In c3b 1b, M was one value along the row, so a texel shift cannot supply it.
- **Our level table.** c4's table is a cosine (`c4_model.h:150-170`). polly2's tables must not be copied in (no licence). c4 page 1 measures the level against our table at the three thresholds: that is the place to replace it with our own console numbers.

### (d) Nearly edge-on triangles

All REPORTED.

- **Coverage** (`P/isp.md:739-772, 897-931`).
  - A pixel is drawn when its **centre** is inside all edges, with a tie zone of about 2⁻¹⁴ pixel. A tie pixel goes in on a left or top edge and out on a right or bottom edge.
  - Rows run from min floor(y) to max floor(y + ½), with a "row 31" quirk.
  - A sliver narrower than a pixel draws only the pixels whose centres it holds. It can have gaps, and **draw nothing** in many rows.
  - A repeated corner or three collinear corners draw nothing.
  - **Three equal corners fill whole 32-pixel tile rows** (`P/isp.md:772`). c4's band cutter drops polygons of fewer than 3 corners (`c4_model.h:254-270`). An all-equal band is unlikely but not excluded: a check to add.
- **FPU_PERP_VAL**, where polly2 flattens planes when |det| ≤ this register.
  - KallistiOS writes 0 at 0x084, which it names TEXTURE_CLIP (FACT, `pvr_init_shutdown.c:159`).
  - With 0, only a determinant of exactly 0 is flattened (`P/isp.md:594-607`, `P/tsp.md:356-394`).
  - Its coverage is unchanged, and a zero-area triangle draws nothing anyway.
  - So a near-edge-on face keeps its real, very steep planes.
- **"Equal or nearer" on the second pass.**
  - c4 sends **the same vertex array** to the opaque depth pass and the bump pass (`c4_spike.c:487-492`).
  - The translucent list is **pre-sorted** (`c4_spike.c:916`), so the polygon's GEQUAL is used (`P/isp.md:1407-1410`; `P/isp.md:1603-1612`).
  - polly2's stored depth is a function of the corners and the tile only: 24-bit setup, then 19 bits stored (`P/isp.md:934-958, 1254-1368`).
  - So both passes store the **same key**, and new ≥ old passes at every pixel the first pass kept (INF on REPORTED rules).
  - **No holes are expected from depth.** They would come only from coverage, and coverage is the same rule on the same edges.
  - An edge shared by two bands that leans less than a tie zone a pixel can be drawn by both (`P/isp.md:766-768`). With ONE/ZERO the later band wins those pixels: at most one band's Q, harmless.
- **The other lists, for contrast.** Autosort would ignore the depth mode and peel above the opaque key (`P/isp.md:1614-1648`), and inside the punch-through list the mode does nothing (`P/isp.md:1394-1417`). c4 uses neither.

### (e) Render targets read back as textures

- **Their tests do not cover render to texture at all** (FACT, `third_party/skmp/dreamcast-tests/TODO.md:93`, "C7 … not covered"; `T:3602-3606`). Our c3b 1d is the only console evidence: a PVR-drawn target as a bump texture, 0 wrong of 65,536.
- **Byte order.** A bump texel is the raw 16 bits, byte 0 = R and byte 1 = S (REPORTED, `P/tsp.md:781, 848-849`). So a 565 target holding code 0 .. 255 gives R = code, S = 0. That agrees with c3b 1d.
- **Non-twiddled bump with point sampling does not hang.** The hang needs bilinear filtering (section 6 above). c4's bump read is NEAREST (`c4_spike.c:206`).
- **Raster stride.** A raster texture is read with stride 8 << TexU texels (`P/tsp.md:709`), and a target is written at base + line × FB_W_LINESTRIDE × 8 (`P/tsp.md:983-984`).
  - c4's 320-wide mask is read as a **512-wide** texture (`c4_spike.c:130`), and c4 already passes a stride of 512 to `pvr_scene_begin_rtt` (FACT, `c4_spike.c:258`). The map passes 128 (`c4_spike.c:257`).
  - KallistiOS documents that argument as the "backing texture pitch (in pixels)" (`KOS/include/dc/pvr.h:922-931`).
  - So the stride matches, if KallistiOS writes it to FB_W_LINESTRIDE as documented. That was not checked in its source.

### Summary

| Candidate (as lead gave it) | skmp's rule | Verdict |
|---|---|---|
| Half-texel shift between map and mask | HALF_OFFSET 7 under KallistiOS: centres for coverage and shading, no shift with point sampling; c4's u = m/128 | **Killed** (error ±½ map pixel, unbiased) |
| Ramp off by a code through the perspective divide | 16-bit plane values, 16-bit reciprocal, 17-bit floor, coordinate floored | **Minor**: a few-percent edge effect near the light, no systematic code error |
| Bump levels against our cosine | Measured piecewise tables, truncated products, lowest level 3, \|R − Q\| not mod 256 | **Supported**: windows 1-3 codes (w 32) and 5 codes (w 16) narrower; explains c3b's 109, **not** its 28 |
| Slivers and "equal or nearer" holes | Same vertices, pre-sorted GEQUAL, depth a function of corners only; coverage by pixel centre | **Holes killed** for the mask pass. Slivers do miss centres in the **map** pass: that is c4's rim, which grow handles |
| Render-target reads (twiddle, 565 low byte) | R = byte 0, S = byte 1; non-twiddled point bump fine; render to texture untested by them | **Supported** (c3b 1d is the evidence). Stride already matched (512 and 128) |
