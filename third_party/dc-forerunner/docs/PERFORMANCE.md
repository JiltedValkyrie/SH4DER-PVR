# Performance: lessons from the Halo CE Dreamcast port

Source: the Halo CE Dreamcast port (private repo "dc-halo"). "#NN" means a dc-halo GitHub issue. "commit xxxxxxxx" means a dc-halo commit. Frame numbers are the owner's boot reports, from emulator or hardware, as recorded there. Items marked ESTIMATE were not measured on a boot. Items marked GUESS are plans, not facts.

## Rules that saved us

1. Measure first. Name the bucket that holds the frame time, then fix that bucket. In Halo the HUD guess was wrong (store-queue work did not move it), and both Boot D guesses were wrong. Only the counters showed it.
2. Budget: 33.3 ms per frame at 30 fps. About 6,000 polygons on screen. Do not call a 1,000-triangle model "light".
3. SH4ZAM for all math. A hand loop is allowed only when SH4ZAM has no equivalent, and the review must say so.
4. KOS fastest path first (direct render for big meshes, store queues, DMA). The review must name the path used, or the reason it was not used.
5. No per-frame printf. Serial at 115200 baud costs about 87-94 ms per KB. Diagnostics print once per load, scene or event, with a cap.
6. Label every number: MEASURED (a boot), ESTIMATE (a calculation) or GUESS. Only a boot log counts as proof.
7. Read the perf plan before touching render, AI, collision, animation or culling.
8. Keep the big costs out of the frame: no storage writes, no full-map scans, no walks where a single ray is enough.

## 1. CPU budget at 30 fps

- Target: 33.3 ms per frame. The game must hold it at the worst encounter, not the average.
- Halo's gate was "stable 30 fps or a measured floor, worst case first". Before the plan, gameplay ran at about 16 fps with a worst frame of 66 ms, CPU near 100 %, GPU 9-14 % (PERF-01).
- The limit is the CPU, not the GPU. In every Halo report the GPU sat at 6-20 % while the CPU was at or near 100 %. The SH-4 transform of vertices (models and BSP) took about 21 of 37.7 ms on the d40 map (#378).
- Halo vertex cost (boot numbers, Boot A to Boot B): BSP cost 1.2-2.1 us per transformed vertex (about 300 cycles), then 0.86-1.5 us after the render pass changes (about -35 %). Clip work fell from 950-3,200 triangles per frame to 6-71 (-95 to -99 %).
- The GPU wait: 7.5-10.2 ms per frame in every shot of Boot B. One vertex buffer meant the CPU could not start the next frame until the GPU finished. A second buffer needs VRAM; VRAM was 91-95 % used, so the fix was parked.
  - Rule: decide VRAM for the second buffer on day one. A full VRAM means no CPU/GPU overlap.
- Polygon budget: about 6,000 polygons on screen for 30 fps (Halo's working number). Examples from the port:
  - a c10 night shot: 6,086 triangles, frame 49 ms, model draw 23.2 ms (#254);
  - a d40 shot: 5,879 triangles, frame 37.7 ms (#378);
  - a single Jackal model drew 1,997 vertices and was not even visible (#254).
- Rule: keep a per-view count of triangles and vertices on screen. Halo's map editor shows it while placing objects (see TOOLS.md section 2 in the Halo repo).

## 2. KOS fastest paths

- Direct render (`pvr_dr_target` / `pvr_dr_commit`) instead of `pvr_prim` per vertex.
  - ESTIMATE from cycle counts: `pvr_prim` 60-100 cycles per vertex, direct render 30-40. On a 33 ms frame with about 22k vertices at peak, the saving was estimated at 1.5-6.6 ms (#278). The real saving is unknown until the boot test.
  - Rule: direct render owns the store queues from init to commit. No `pvr_txr_load`, `sq_cpy` or other `pvr_prim` between them. Commit one path at a time.
  - In the KOS version used, `pvr_dr_init` is a deprecated no-op. Check before relying on it.
  - Risk: a misuse corrupts the TA stream (garbage frame).
- Store queue: the port wrote each vertex twice (into a pending buffer, then copied into the queue) and read it back once. Writing it once straight into the queue block cut the base pass from about 80 to about 45 instructions per sent vertex (code count, not a boot number).
- Sky: an outcode trivial accept and reject before the clip chain. Transform once per frame.
- Store-queue HUD pips did NOT move the HUD cost (3.2-3.8 ms with a full shield, 0.8 ms empty). The guess was wrong. Measure before you assume the submit is the cost.
- Vertex DMA: listed as KOS-side work; not done in Halo (GPU idle, so not the limit). Revisit only if the CPU is free and the transfer shows up in the bucket.
- Disc reads: KOS gives stdio a 512-byte buffer on ISO9660. Every 2 KB sector then became its own one-sector GD-ROM command. The fix (unbuffered reads, sector-aligned direct reads, bounce buffer for the rest) was coded on 2026-10-22 (dc-halo PERF_NOTES entry "Disc reads unbuffered"); no gain is claimed until a boot log.
  - Rule: a loader reads whole sectors in large chunks. Test the read count per load, not only the time.
- Build flags: KOS was rebuilt once with `-DNDEBUG -DDBGLOG_DISABLED` so KOS asserts and debug logs go quiet. Game printf stays. Back up the KOS environ file before editing it.
- Full flag set (what dc-halo ships; the Makefile checks it):
  - KOS (environ.sh, KOS rebuilt once with them): `-DNDEBUG -DDBGLOG_DISABLED -O2 -fomit-frame-pointer -fno-PIC -fno-PIE`, ABI `-m4-single -ml -mfsrra -mfsca -ffunction-sections -fdata-sections`, link `-Wl,--gc-sections`.
  - Game math (every file): `-ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno` + SH4ZAM headers. With `-ffast-math` a NaN check like `v == v` or `!(x >= 0)` folds away: use a bit-test `finite_f32`.
  - Never `-m4-single-only`: environ.sh falls back to it silently when sh-elf-gcc cannot start (MinGW DLLs not on PATH). Slower, and it hid a compiler crash. The Makefile refuses it.
  - Per file, after an A/B on hardware: hot per-frame files (collision, skinning, mesh draw, particles) in `HOT_O3` (`-O3`); cold files (save, settings, menu state, loaders) in `COLD_OS` (`-Os`) to pay for the -O3 text. Never raise the default for everything.
  - Not on: `-flto` (tried in KOS environ, left off), `-fstack-protector`.
- Review rule (owner order, 2026-11-11): every review names the KOS path used, or says why not with a number. No line = not PASS.

## 3. SH4ZAM for all math

- SH4ZAM is the math library (pinned v0.10.0 in dc-halo). The audit found 606 references and no libm trig or sqrt in hot paths.
- Review gate: hand-written math that has an SH4ZAM equivalent = SEND BACK.
- #132 (ordered 2026-11-08): replace hand-written math everywhere with SH4ZAM. The first files were the bipmove and object collision code.
- #240: a contribution with hand math and +8,756 B of unmeasured cost was sent back.
- Rounding: the SH-4 `fipr` and `xmtrx` do not round like IEEE. A host check against the target needs a tolerance (about 65 ulps at 4e-6 of the magnitude, in one test). Do not expect bit equality with a PC build.
- Rule: each review line says "SH4ZAM: call X" or "no equivalent: why".

## 4. Serial logging cost

- Serial is 115200 baud. Measured in Flycast: 1,081 bytes took 94 ms, about 87 us per byte. On a real console with a cable the figure is read from KOS, not measured.
- A full map load log is about 40 KB, about 3 s of serial. A QUIET log is about 4 KB.
- #89: the log itself hung the frame. HITCHX1 rows read "log N us" equal to the frame: 1.2-1.9 s on the first frames after a load. Fix: QUIET tier on tester images; FULL only on test images.
- #301 (open): a test log reached 4.83 MB, 45,997 lines, about 7.5 min of serial time. A gate-rows tier cut about 70 % (3.4 MB): keep the gate rows (LOADRAM, HEAP after load, VRAM ledger, upload failures, errors) and drop the list dumps.
- Rules:
  - No per-frame print without a once-guard or a cap. A per-frame watch line on one cutscene map ran at 8 fps, and each KB costs about 94 ms of serial time. The rule banning per-frame prints followed (2026-11-01).
  - Diagnostics print once per load, scene or event. Halo keeps a cap of 40 hitch rows.
  - A test image starts at the gate-rows tier; a debug switch can turn on the full flood for one session without a new image.
  - Each diagnostic gets its own guarded block, a once-guard, a cap and a release no-op macro. Run the neighbouring tests too, not only your own (Cortana, #340: extra rows inside another diagnostic's block broke its pins in the full suite).

## 5. Hitches

- Frame hitches are a bucket problem. Name the bucket from counters first (see section 8), then the cause.
- #12 (closed): hitches in a level with no cause in the report. Needs a serial log or the hitch rows to locate it.
- #89, autosave: a VMU save wrote for 1.3-1.7 s once every five minutes. Fix: checkpoints in RAM only (#79). Rule: no storage write inside a frame, no autosave that blocks.
- #122 (open): hitches of 100-170 ms on one map, with the physics bucket at 57-122 ms of each. Suspects listed: a placement rotation change, a rebake, a door box. Next step: per-pass counters (collision profile rows), not a guess.
- #289 (fixed in code, boot pending): a load failed with "no contiguous 3.54 MB block" while 11 MB was free. The largest hole was 3,396 KB after section switches. Rule: check the largest free block, not the total free RAM. Load big blocks first, in a fixed order.
- The sim (AI, physics, script, actors) was 1-5 ms of a 70 ms frame in Boot A. Render was 85-95 % of the frame. Check the buckets before you spend a week on AI.

## 6. Collision and AI cost

- #383 (fix coded, boot pending): sentinel flyers paid the ground walk. Sync probes averaged 19.6 ms a call, worst 62 ms, 2,256 ms of hitch time on one map. The walk made up to 32 mover calls.
  - Fix: flyers use one BSP ray, as the original engine does.
  - Host numbers: ground walk 109 us, ray 1.1 us. ESTIMATE: 20 ms -> 0.2 ms per call.
  - RAM +4 B bss, text +788 B, VRAM 0.
- #382 (open): AI body probes cost 6-8 ms a frame. Fix path: a per-actor probe memo.
- #373 (open): a vehicle run slowed to a crawl while collision tests ran. Measure each pass.
- Rule: a sync probe must not walk a biped path for a hover unit.
- Rule: when the original engine uses one ray, the port uses one ray. Check its source file and line in the commit message.
- Halo's audit found AI traces the original engine never does, and no dormancy for actors outside the potentially visible set. Both were tracked as phases in the perf plan. Check the original's call sites before adding a trace.
- **Pair searches run on live bodies only, with a cap** (Junior, #387). Separation over the live sentinel set costs k(k-1)/2 pairs for k = 8 to 12 alive, not 32. Size every N-squared pass on the alive count of the hardest difficulty, and put the sweep count on a cap. The cost figure from the #387 work is 0.02 to 0.05 ms with 8 to 12 alive.
- **Unbounded per-frame work needs a cap** (Parity, #387 sentinel sweep, cap 8). A pair search or sweep with no bound made a serial flood and a frame spike. Cap it per frame and print once per load.

## 7. Culling and LOD

- Counters that show a broken cull (Boot A): `byp` 8-28 bypasses on the a10 level (near-plane bail, though the level has at most 58 clusters), `ovf` 0 (no portal overflow), clip 950-3,200 triangles through the slow clip path. The clip path was the cause; the fast path fixed it.
  - Rule: each cull gets a counter. Bypass counts show a cull that is computed and then ignored.
- Chunk visibility is baked (`.vis` files) and locked. Changing it needs the keep-identical proof (TOOLS.md section 1.2 in the Halo repo).
- #168 (open, low priority): far polygons dropped on a big map (gaps in a far ridge). Rule from the owner: if a fix risks other things, leave it. Read first, cost it, then decide.
- #254 (Jackal, planned): two bands for the character. Near: Halo's super-low tier (525 triangles, 748 vertices drawn). Far: a sealed cut of the super-low model (349 triangles, about 575 vertices), then a card, then hidden. The switch distance follows the object-detail setting. The plan gives about 41 KB of RAM per map with Jackals (plan figure, not measured).
- Rule: every mob and every map gets its LOD tiers, and the census lists the gaps. The owner rule is "real far cuts everywhere, one character at a time" (2026-11-15).
- #337: cutscenes ignored automatic LOD and ran at 15 fps with nothing lowered. Rule: cutscene paths go through the same LOD choice as gameplay.
- Tip: a far mesh with the same 1,000 triangles everywhere wastes the budget. Cut the far tier first.

## 8. Profiling tools

- VERBOSE panel (Halo): Frame, Pie, Memory, KOS and a BSP view. The BSP view has a LAST SLOW block that names the part and the heap, uploads and packs of that frame. A `SLOW` serial line marks the same frame.
- Counter rows (Halo): TRACE (sight, fire, steer, pick, move, proj), COLL (point, box, face tests), CULL (walks, overflows, rejects, bypasses, clip), SIM (ai, gp, drop), FXSPLIT (fx, proj, update, hud). Each is an add per event, so off-state cost is near zero.
- Overlay cost: an estimate of about a dozen timer reads per frame (ESTIMATE). Compare FPS with the overlay on and off.
- The KOS counters (`perf_cntr`) read zeros in an emulator. Hardware numbers differ.
- Gate rows (every test image): LOADRAM, HEAP after load, HEAPWALK, VRAM LEDGER, upload failures, sound load failures.
- Method:
  - Compare shots at the same positions. Boot B showed different spots in Boot A; per-unit numbers were the fair comparison.
  - The log the owner sends is the test. No conclusion from a log the owner did not confirm.
  - A theory dies faster if you ask one cheap question first (Cortana, #340: "does it happen with the filter off?" killed two of Cortana's theories).
  - Label every number in the perf notes: MEASURED, ESTIMATE or HYPOTHESIS.
- Emulator vs hardware: Flycast gave a black frame read for the post-process path (#277) and a rare hang on a disc read (#24). Confirm a feature on the hardware target.

## 9. Frame numbers seen (Halo, for scale)

| When | Scene | Frame | FPS | Main bucket |
|---|---|---|---|---|
| before the plan (PERF-01) | gameplay | worst 66 ms | about 16 | CPU near 100 %, GPU 9-14 % |
| Boot A, 2026-10-18 | spawn | 70.1 ms | 14 | BSP 33.5, models 21.6 |
| Boot A | hangar, 3 actors | 110.3 ms | 9 | BSP 51.9, models 35.2 |
| Boot A | firefight, 11 live | 87.9 ms | 11 | models 42.7, BSP 28.7 |
| Boot B, after P1-P5 | firefight, 9 live | 60.2 ms | 17 | models 23.5, BSP 13.1 |
| Boot B | dark pillar | 30.1 ms | 33 | models 4.5, BSP 3.9 |
| #254, c10 night | Jackal in view | 49 ms | about 20 | model draw 23.2 |
| #378, d40 (b13) | trench | 37.7 ms | about 26 | BSP 9.6, models 11.2, vertex loop 6.7, triangle loop 5.4 |
| #92, MP Ivory Tower | 3 bots | 50-100 ms | 10-20 | MP is CPU and TA bound |

- #378 (open, 2026-11-15): every level missed 30 fps. CPU near 100 %, GPU 14-20 %. The plan: SH4ZAM batch transform and store-queue submit in the vertex loops; BSP chunk culling; LOD tiers for every mob.

## 10. What your source game may bring (checklist)

Fill in the numbers from your own first boot. Until then they are GUESS.

- Budgets for the new port: 16 MB RAM, 8 MB VRAM, 2 MB AICA. Gate: load end under 2 MB free RAM = send back; texture upload failure above 0 = send back.
- Your current costs (per build, from the tracker): RAM <your number> KB, VRAM <your number> KB, AICA <your number> KB. Music or voice streams: ring size and count, not the full file.
- Frame budget: start at 33.3 ms and 6,000 polygons on screen, as Halo did. Your models and levels are GUESS to be heavier; measure the first real map before setting a number.
- Vertex transform is the expected limit (Halo's lesson). Plan the vertex count per character tier from the start, not after the first boot.
- Music is a stream: the full file is far bigger than RAM, so do not make it resident. Halo's title music (95.4 s, 4.4 MB ADPCM) plays from two 32 KB rings.
- Source meshes and animations are data-heavy. GUESS: skinned meshes will dominate the CPU like Halo's models did. Build the LOD tiers with the first character, not later.
- Store queues and direct render apply to the source's static and skinned meshes. The same review rule applies: name the KOS path used.
- Logging: the same serial rules. A script log can flood the serial line; cap it from the start.
