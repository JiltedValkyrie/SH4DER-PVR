# Graphics lessons for a Dreamcast port

Practical rules from the Halo CE Dreamcast port (dc-halo). Written for a port from <source platform>, using <source engine> and KallistiOS (KOS). Sources: the Halo repo docs (docs/archive/graphics/, docs/PERF_NOTES.md, docs/plans/PERF_PLAN.md, docs/BUILD.md), its commit log, and its GitHub issues (#nnn). Issues are cited by number, commits by hash.

Labels used below:
- MEASURED: from a Dreamcast serial log or a Halo overlay row. Settings were recorded with it.
- CODE: a constant read from source. Exact.
- EST: an estimate, not measured.
- GUESS: not verified. Test before use.

Units: KB = 1024 bytes. 30 fps = 33.3 ms per frame.

## Rules that saved us

1. Measure before you fix. Halo's first guesses (emit path, store-queue pips, HUD) moved nothing. Per-pass timers did. Add a timer row first, then optimise (PERF_NOTES, Boot C).
2. Keep a VRAM ledger per map. End-of-load free must stay above the reserve (448 KB in Halo). Check it on every build.
3. Total free VRAM is not contiguous free VRAM. Upload failures came from fragmentation (#372, #125, #289). Load big textures inside the level load, with a shrink-and-retry.
4. Clamp lit colour at 0 on both sides. A negative float wraps into other colour channels on SH-4 (#340).
5. Smooth alpha ramps need 8-bit alpha. ARGB4444 has 16 alpha levels and shows steps (#405).
6. Every model needs a real far cut. A far model equal to the near one cost 13 fps in one hangar (#139).
7. Gate each effect on its own cost, not on the whole reserve. Bloom never started in one build (#336).
8. Flycast is not hardware. Screen reads come back black (#277), and emulator GPU timing does not model fill rate. Trust only real-hardware logs for numbers.
9. A silent gate is a bug. Model chrome was refused for about three weeks by a stale check (#186). Log once per load when a feature starts or is refused.
10. Keep logs quiet. Serial is 115200 baud: 1 KB = 94 ms. A per-frame print dropped one cutscene to 8 fps. Log once per event.

## 1. PVR rendering model

- No vertex shader. Every vertex is SH-4 work, about 2 to 3 us per model vertex (PERF_NOTES, Boot F). The CPU is the bottleneck. On one Halo level the CPU sat near 100 % and the GPU at 14 to 20 % (#378).
- Tile-based deferred renderer. Lists: opaque, translucent (autosorted by the PVR), punch-through. Halo's cards draw on the translucent list, and the port runs no punch-through list (IMPOSTORS.md; PERF_PLAN: migration "not doing", GPU 9 to 14 % busy).
- Tile bins hold a fixed number of primitives (16 by default). Overflow falls back to larger bins. CODE formula (VIDEO_BUDGET_COSTS.md): bin bytes x tiles x (1 + overflow) x 2. One extra list at 640x480 = 131 KB. With AA 2X it is 262 KB.
- Translucent autosort fixes order but costs fill. There is no depth prepass, so the far side of a translucent body shows through its near side (SH4DER_CORTANA doc).
- Order matters. Alpha blend then additive does not commute. Halo nudged the rain's depth so it always sorts after the alpha body (SH4DER_CORTANA doc, line about rain order).
- Double buffering: one vertex buffer made the CPU wait 7.5 to 10.2 ms a frame (PERF_NOTES). KOS already allocates both TA banks, so the second buffer cost 0 VRAM. Enabled 2026-10-19.
- Submit cost: pvr_prim about 60 to 100 cycles a vertex; direct render (pvr_dr_target / pvr_dr_commit) about 30 to 40 cycles (EST, Parity's cycle counts, #278).
- Where the time really went: the model vertex loop was 75 % of model time (PERF_NOTES, Boot E). Changing the emit step moved nothing.

Rule for your port: budget SH-4 vertices and triangles, not GPU. Use KOS direct render from day one for any mesh over 100 vertices. Target about 6,000 polygons on screen for 30 fps (Halo team rule). One Halo frame at 5,879 triangles took 37.7 ms (#378, MEASURED).

## 2. VRAM budget (8 MB)

MEASURED on Halo, Balanced preset, 640x480 unless noted:
- Texture pool after pvr_init: 1,831 KB (AA off), 1,817 KB (AA 2X). The rest of the 8 MB goes to the TA vertex banks (2 MB), tile lists and the frame buffer.
- 320x240 AA 2X leaves 3,418 KB for textures.
- Lazy reserve: 448 KB must stay free at end of load. Effects, weapons and model sphere maps load later.
- Fit pass: at load, mip levels are dropped until free VRAM reaches the reserve. a30_b dropped 32 levels (512 KB). b30a dropped 58 levels (712 KB). Gameplay maps ended only 0 to 15 KB above the reserve. Any new VRAM user therefore costs mips somewhere else.
- Effects: BLOOM + RAYS 17 KB, SH4FTS 16 KB, GRAIN 8 KB (b7 log, #340). Holo flip-books 106 KB.
- HUD font atlas as PAL4: 96 KB saved (commit 06fa1835).
- Shadow volume list at 640x480: 131 KB.

Lessons:
- Keep a table: per map, resident textures, sum, and free after load. Halo's is docs/archive/graphics/VIDEO_BUDGET_COSTS.md. Each row needs its settings (resolution, AA, preset, map, tool). Older Halo rows lacked preset names and could not be compared.
- An effect's VRAM gate needs its own size plus a margin. Bloom needed its size + 32 KB, later 48 KB, so a 40 KB font load could not fail once effects started (commits 8add570c, 626479da).
- Cut mip levels before you cut resolution. Write the cut order down.
- **Lazy upload hides VRAM cost** (Senior, #404 rain tile). A pass that is off at LOW never uploads its texture, so turning it on adds VRAM there only. Every "turn this on at LOW" change states the first-draw VRAM in KB per map (the rain tile was at most 43 KB, ESTIMATE).

## 3. Texture formats

Bytes for one texture with mips (PALETTE_TEXTURES.md, from Halo bakes; MEASURED sizes, CODE math):

| Format | 512 px | 256 px |
|---|---|---|
| Raw RGB565 or ARGB4444 | 699,050 B | 174,762 B |
| PAL8 (256 entries, ARGB8888 palette) | 350,549 B | 88,405 B |
| PAL4 (16 entries) | 174,826 B | 43,754 B |
| VQ (256-entry codebook) | 89,430 B | 23,894 B |

Formats:
- VQ: one 256-entry codebook of 2x2 texel blocks, shared by all mip levels. Entries are 565 or 4444. A 512 VQ texture is about 1/8 of raw. This is the default for everything except small UI.
- Banding: VQ entries are 16 levels per channel in 4444. Truncation (c >> 4) made long flat runs: mean run of one colour 29.7 texels against 4.3 in the source. Fix: ordered dither before VQ, bake only, zero extra bytes (smooth banding 6.48 to 2.60; mean run 29.7 to 1.3). Use rounding, not truncation.
- Dither only gradients (skies, glows, planets). Noise on detail textures wastes codebook entries. Dither only textures drawn near 1:1 or minified. A dither on a large magnified gradient shows as a woven dot pattern, which the art lead rejected on first sight (Cortana, #405).
- Alpha: 1555 = 1-bit alpha (cut-outs). 4444 = 4-bit alpha (blends). 565 = no alpha. Decide per texture at bake, from source alpha or key colour. Never default it.
- Key colour bugs: Halo 2 palm fronds were DXT1 with no cut-out, got 565, and showed dark blocks (#163, fix 5f46213f). Flood pop particles showed their black key (#196, fix ba4d5aeb).
- Paletted: PAL8 = 256 entries, 4 banks. PAL4 = 16 entries, 64 banks. Palette RAM holds 1,024 entries. One entry format per frame: 4444 caps colour at 4 bits, 8888 keeps 8 bits. KOS warns that bilinear filtering with 8888 entries roughly halves fill rate (not measured on Halo). The palette is read at render time, so at most 4 PAL8 palettes can be on screen in one frame. Use: volume light shafts moved to a 64 px PAL8 texture with 8-bit alpha, 4,096 B against 7,510 B (#405). The palette entry format is one global PVR register: a texture that sets ARGB8888 silently breaks every 4-bit palette written earlier (HUD font, pause menu). Every palette user rewrites its entries when the format changes, or the level sets one format once (Cortana, #405 review). Keep one palette bank map in one header and test that the banks do not collide (Cortana, #405).
- Mips: the fit pass drops them (above). Halo detail maps stayed at 128 px. World base maps went to a 256 px cap (F80), which added 784 KB on a10.
- Halo's bump maps use native PVR bump: normals are stored as palette entries, and the PVR lights per pixel (DETAIL_MAPS doc, 2026-10-20).
- **4444 and 565 are the same bytes. Additive on black saves no VRAM once a texture is VQ** (Cortana, research 2026-11-15, no commit). 231 of 264 ARGB4444 textures were already VQ, so the format flip saved 0 KB. Cost a format change from the bake record, per texture, before you believe a saving.

Rules for your port:
- Convert the source's block-compressed textures (DXT1/3/5 or similar) to VQ at bake time: decode to 565 or 4444, dither gradients, then VQ.
- Keep textures with smooth alpha (glass, glows, haze) at 4444 with dither, or PAL8 with 8-bit alpha. Plain 4444 shows alpha steps (#405).
- Untile the source platform's texture layout at bake. Ship PVR-twiddled data only. No runtime untile.
- Hero textures (faces, armour) may get PAL8 at 256 px. This is GUESS until the budget confirms it. A Low preset must not cap hero faces. Texture caps take a short exception list, costed in KB (Cortana, #404 memo: a Performance cap of 128 px hit the actor the cutscene camera sits on, +16 KB in play, not fixed yet).

## 4. Texture upload failures

Symptom: serial row "Texture upload failed: tag, KB, VRAM free" and an UPFAIL counter.
- #280: a 23 KB upload failed with 31 KB free. Lazy uploads in play had taken free VRAM down from 448 KB.
- #372: d20 hangar had 185 to 195 KB free but no contiguous 23 KB block. The load step had a shrink ladder, the lazy upload did not. Fix edb279fa: the Chief's armour mask loads inside level load.
- #125: a50 BSP needed 3,580,920 B contiguous; largest free block was 3,579,904 B, 1,016 B short.
- #289: d20 hangar had 11 MB free but largest hole 3,396 KB (heap fragmentation after section switches).
- #393: BSP end-of-load was under the 2 MB free gate (d20 hangar 754 KB, d40 trench 957 KB lowest).

Rules:
- Check the largest contiguous block, not total free.
- Everything a map needs loads inside the level load, with a shrink ladder. Late loads have no retry.
- Log the contiguous size with every failure.
- Gate BSP loads on free RAM, and say the number when the gate fails.

## 5. LOD tiers, far models, impostors, polygon budget

- Budget: about 6,000 polygons on screen for 30 fps (Halo team rule). Halo's actor rule: once a frame passes 5,000 triangles, other actors drop to low (PERF_NOTES).
- Far cut per model. A crate with far = near had 222 vertices each, a hangar ran 13 fps, frames 70 to 85 ms (#139). Fix: a real far cut for every map. Rule (#156): every LOD must be rebuilt from committed scripts at bake time, never hand-edited in a blob.
- Hand cuts beat auto decimation on silhouettes. Auto decimation of the Pillar of Autumn was called a "triangle disaster" (PERF_NOTES). Hand-cut results: Flood captain 2,650 to 1,294 triangles (#328); Jackal far 349 triangles beside 525 near (#254, d7b5feb5); elite far 361 triangles (dbadb3d2).
- Two tiers for hot enemies: near = low, far = sealed cut. Jackals (#254) and Flood (#376) had this problem.
- Hidden instances: 9 cryo tubes in view, 742 vertices each, were an estimated 16 to 21 ms of SH-4 vertex work (#126, EST, not measured). Fix: a per-instance draw-off in the map editor.
- Super-far tier for characters below about 24 px on screen: a ~100-triangle box (GFX-31, #265).
- Cutscenes ignored dynamic LOD and ran 15 fps (#337). Fix 5df9c37e: floor 80 % for baked cutscenes, 70 % in-level. Frame rate must be guarded in cutscenes too.
- Impostor cards (58 views, 8x8 atlas, 89 KB per 512 atlas) were retired for gameplay on 2026-10-29. A card freezes moving limbs. Cards survive only in cutscenes: on x10 the bridge fell from 30 fps to about 19 when seven fighter-bomber cards became 4,217-vertex meshes (IMPOSTORS.md, F53). Rule: keep meshes for anything that animates. Use cards only for static far props.
- **Story heroes keep their own LOD override** (Senior, #404 Cortana on OBJECT DETAIL LOW, RAM 0, VRAM only the rain tile). Heroes never drop to the far mesh or lose their look pass. The cheapest override is "use the Balanced row", since that blob is resident anyway. Check what lazy texture upload adds at the low setting.

## 6. Culling: portals, BSP, PVS

- Root cause (CULLING.md, 2026-10-16): the camera room was guessed from overlapping cluster boxes. An actor in the next room counted as visible. Whole rooms were submitted per chunk: about 12,000 triangles while facing one wall.
- Fix: find the camera leaf with a point test on the BSP, then its cluster. Walk only portals that the camera can see. Rearward portals reject. Traversal overflow falls back to the PVS, never to "show nothing".
- Numbers: a10a has 675 subclusters over 33 clusters (median 64 surfaces each). BSP chunks split by facing normal (6 axis buckets, under 24 triangles merged). A cone test skips a chunk whose triangles all face away.
- Measured: 70.5 % of drawn BSP vertices were class 0 (no outcode bits set, inside the frustum) on a10. A direct loop with no outcode test is safe for those chunks (c520a620).
- BSP cost on Halo d40: 9.6 ms of a 37.7 ms frame (#378).
- Chunks must be small: a room-sized box per chunk sent the whole room. Halo splits to about 32 wu (PERF_NOTES).
- Audit check: a culling result that is computed and then bypassed costs time for nothing. Check every call site.

Rules for your port: use the source's zone and portal graph as the PVS, if it has one. Find the camera zone by point test. Split big surfaces into chunks of about 32 wu, each with a tight bound and a normal cone.

## 7. Alpha, translucency and sorting

- The translucent list is autosorted, costs fill, and can overflow its bins (section 1).
- Measure the translucent pass by part. Halo's TRSPLIT row (commit 99e36b62) splits it into tint, veil, glass reflection, env, matcap, additive and alpha, in ms and vertices.
- Glass: on oblique glass the matcap pass peaked near white over the veil. Halo caps the matcap vertex colour at 0.5 (f4f3c6cb), so the peak is light grey.
- Dead bodies: a translucent fade with dead alpha looked mottled. A uniform fade fixed it (DC_GPU_FREE_WINS_AUDIT).
- Enemy fade-in: 0.5 s linear on first appearance (45005be3). This is a DEPARTURE from OpenCE, which has no spawn fade. It is campaign-only.
- Per-frame reflection budget: 3,000 transformed vertices (BASIC), 6,000 (MEDIUM). Over budget, the cap moves in one chunk at a time, and a 30 % fade hides the cut (PERF_NOTES).
- Matcap: an additive GEQUAL pass, 8 KB VRAM, rebuilt after every PVR re-init.

## 8. Additive versus alpha blend

- Additive: glows, muzzle flashes, lights, cards. Never fog additive effects (PERF_NOTES 2026-10-19).
- Additive looks wrong on black when the source is a gradient. Halo kept gradients on alpha blend and left the brightness unchanged (#405).
- The Covenant shield model is ONE x ONE additive, unlit white by shader name (585da185, #141).
- Light effects came out too bright (#110, open). Tune alpha, not colour, first.
- Light cost: a light halo is 13 vertices and 18 triangles per light on the translucent list (JET_GLOW.md). Count it against the translucent vertex budget.
- **Read the source blend mode per shader before changing a blend** (Cortana, #405 mock). Census on one 4444 map: alpha blend 47, add 40, alpha multiply add 1. A light shaft expected to be additive was alpha blended in the source; drawn additive it was a blown slab 3.6 times brighter. Run a read-only per-tag census (a script over the tags, not names or memory) before any blend or format work.

## 9. Fog

- PVR fog is a per-pixel table. It costs the SH-4 nothing until the fog changes. Registers are rewritten only on change (PERF_NOTES 2026-10-19).
- Far-clip edge: draw distance cut a hard line against the fog colour. Fix: a second fog layer ramping 0 to 1 over the far band (atmofog_table_clipped, 2026-10-20; also at 100 % from 2026-10-29). Skip the ramp when the fog colour is black.
- Indoor fog: fog must fade to black indoors (#312, commit bae72da6). A lighter fog colour made the draw distance edge ugly.
- Sky dome: the dome's alpha is cloud cover, so the fog clear colour shows through it (#27, open). The source takes dome alpha from its map and draws the dome solid. Fix: bake the dome opaque.
- Sky warp: large dome triangles warp with camera angle, and the floor / ocean band is missing (#296, open). Cause not diagnosed. Rule: cut big sky and ground triangles into smaller ones before the PVR.

## 10. Lighting

- Vertex lighting runs per vertex on the SH-4. Object lighting was estimated at +0.3 to +0.6 ms on 10 to 13k model vertices (EST, 2026-10-19).
- Negative light wrap (#340, fix 044af04f). A light's falloff went negative just outside its edge and subtracted from ambient. SH-4 ftrc wraps a negative channel into others: negative blue becomes yellow, negative green becomes magenta. Fix: clamp ambient and light terms at 0 once per object (zero per-vertex cost). It showed in pause as well, so test the pause screen too.
- Weapons under dynamic lights came out magenta (#371, open, not diagnosed). A per-part row was added (3de40603). Add the same kind of row to your port early.
- Light modes: Halo kept two, SIMPLE and FULL (5ebed66a), because the middle modes gave little frame-time difference. Halo asked for cutscenes to force SIMPLE (#294, open).
- Lightmaps are the biggest bitmap class. Upper bounds (bitmaps.json): a10 2,893 KB, c20 4,803 KB, d40 3,618 KB. Not all resident at once.
- Dynamic light fade and budget should shrink when frames run slow (#260, open).
- **A lit vertex colour below 0 wraps; it does not clamp** (Cortana, #340, fix 044af04f). The SH-4 float to int convert is signed and the colour pack clamped only at 1, so one light term subtracting outside its radius gave a rainbow gun. Clamp every lit channel at 0 with `shz_fmaxf` before the pack, and pin it with a test. Never trust "the maths cannot go negative".
- **Floor, do not flip** (Parity, #340 NEGLIT, pack_rgb with no floor). Clamp values to the range the pack stores: a negative channel wrapped into the next byte. Guard at the producer, then add a serial row for the first offender.

## 11. Self-illumination, shaders, effects, post-processing

- Layers: multiply or add? Halo's HUD stacked 7 added layers and washed to pale. Multiply-only fixed it (F78). Self-illumination adds to light and texture multiplies (F82).
- Copy the combiner exactly. The spire beam was a plain white bar: the port added a near-white layer where the source multiplies it (#194, 32b4dd54).
- Stage constants: the Covenant shield's blue lived in source stage constants the port dropped, so it drew white (#141). Read the source combiner before porting a shader. Do not guess the colour.
- Post effects (sh4der): BLOOM + RAYS 17 KB VRAM, SH4FTS 16 KB, GRAIN 8 KB. The frame read for them took 1,196 us on an 80x60 grid (#340 log). The frame read needs real hardware (black on Flycast, #277).
- Defaults: Bloom and Godrays off on Balanced (285ef052). Grain on every preset including Performance (#394). Judge grain cost on real hardware.
- No post effects in multiplayer. This is the Halo team rule.
- Effects stay behind a DEBUG row, default off, until measured (#303).
- Anything that needs the frame read or a second full-screen pass needs its own cost line in the budget.
- CRT and chromatic aberration are research items with their own VRAM cost (#155, #199, #262). Mock first.

## 12. KOS paths: direct render, store queue, DMA

- pvr_dr_target and pvr_dr_commit write vertices straight into the TA queue. Halo used them in ta_vertex, hud.c and the particle cards (4314e137, #278). The particle card path sits behind a DEBUG row, default off.
- Store queues did not move the HUD cost. Pip count mattered, submission did not (3.2 to 3.8 ms with a full shield, 0.8 ms empty). Measure the loop, not the write.
- Prefetch the vertex two ahead (SHZ_PREFETCH). Allocate the cache line before writing a 32-byte record.
- Sort model vertices by node so the matrix reloads once per node, not per vertex (convert_models.py).
- Keep loop state in locals. The model triangle loop gained an estimated 0.5 to 1.2 ms (0716b0cf, EST).
- Model strips: a triangle sharing the last edge adds one vertex to a running PVR strip. Estimate -40 to -60 % submitted vertices (PERF_PLAN phase 10, gated).
- BSP chunks are strips (length, then indices). A replay tool must decode them the same way or it reads the wrong triangles (PERF_NOTES, 2026-10-21).
- SH4ZAM first for all math. Halo had 606 SH4ZAM calls and no libm trig or sqrt. Hand loops only when no equivalent exists.
- Build flags: KOS was rebuilt with -DNDEBUG -DDBGLOG_DISABLED to quiet KOS asserts and dbglog (PERF_PLAN). Use -m4-single for real builds. The -m4-single-only fallback hides compiler crashes.

## 13. What cost hours

- Looking at the GPU first. Halo was CPU-bound at about 100 % CPU and 14 to 20 % GPU (#378). Early PVR-side work did not move frame time.
- Silent gates. Model chrome was refused for weeks, since 2026-10-27 (#186, fix b631282d). Bloom never ran (#336). Neither showed in the log.
- Half-run bake chains. Running one bake step alone broke the b10 images. Run each chain whole (docs/BUILD.md, commit cb74d719).
- Test pins on hashes. Each campaign source-file commit moved a baseline pin in Halo's test_mp_guest, and that produced many pin-only commits. Test behaviour, not file hashes.
- Stale outputs. A stale conversion passed until a build check caught it (3207d40e). Rebake, never hand-edit.
- Emulator numbers. PERF_NOTES: emulator GPU timing may not model the PVR fill rate, so real-hardware numbers win.
- Auto decimation. The triangle counts looked fine. The decimated far Chief (153 triangles) lost his shins and floated his arms. The hand cut won.

## 14. Look reviews and mocks

- **A mock off the real baked bytes beats an argument** (Cortana). Decode the shipped texture blob, re-encode the candidates with the real encoder, and render a labelled sheet (now, candidate, 8-bit reference, plus a 3x crop) on a simulated backdrop. Label every mock "simulated, not a capture", keep the brightness numbers on the sheet, and redo the final pair on a real frame. The art lead ruled in one pass each time.
- **Judge at the size it will be seen, and say what the picture is not** (Gouache, #376, #389, #394, #405). Numbers alone gave wrong answers: a far model "saving 2 triangles" was really a 54 % cut; a far crate with one dark face read as the hole the owner had reported; a dither that removed the banding looked like woven cloth. Every visual change comes with a near/far or before/after sheet at real screen size, with true labels and a line on what is simulated. The verdict repeats that limit ("sheets only, not in game") and gives the tester a short watch list. A mock verdict is never reported as seen on the console. The designer's look verdict comes before the change lands (Parity: same rule for default flips).

## 15. What your source game may bring (checklist)

Plan these from the start. Items marked GUESS are not checked against your game's data.

- Textures: the source uses block-compressed formats with a tiled layout. Plan: untile and decode at bake, then VQ (dither gradients; 4444 or PAL8 for smooth alpha). Never ship tiled data.
- VRAM per area: source levels are probably much larger than a Halo room (GUESS). Build the per-area ledger (resident textures, sum, free after load) before the first map. Stream by area, not by level, from the start.
- Meshes: source-era characters are dense (GUESS: well above 6,000 polygons at full detail). Plan three tiers plus a super-far tier. Hand-cut far versions for the main characters and the squad, as Halo did for Jackal and Flood.
- Skinning: the source skins on the CPU or GPU of its platform. On SH-4 that is per-vertex work. Pre-sort vertices by bone node and reload the matrix per node.
- Lighting: dynamic lights plus lightmaps. Clamp every term at 0 (#340). Two light modes. Cutscenes use the simple mode.
- Post processing: screen-read effects such as bloom, depth of field, motion blur and HDR each cost a frame read (1.2 ms on Halo's 80x60 grid) plus VRAM. Start with none. Add grain or bloom only as measured options.
- Culling: zones and portals in the source become the PVS and portal graph. Split large surfaces to about 32 wu.
- Fog: distance and height fog in the source. Use the PVR fog table with a far-band ramp (section 9). Indoor fog must be black.
- Sky: keep the dome opaque and split its large triangles (#27, #296).
- Testing: Flycast for logic only. Real-hardware logs for every number in this file.
