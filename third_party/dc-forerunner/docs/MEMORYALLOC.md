# Memory allocation: lessons from the Halo CE Dreamcast port

Practical guide for a Dreamcast port on KallistiOS (dc-<project>) made from a <source platform> game on <source engine>. The lessons come from the Halo CE Dreamcast port (dc-halo; issue and commit numbers are given as `#N` and short hashes). Numbers are from serial logs or build output unless marked:

- `[ESTIMATE]` = computed or predicted, not yet measured on hardware.
- `[GUESS]` = our judgement, no data.

Halo numbers were mostly measured in the Flycast emulator (the "hidden Flycast" runs) and confirmed by the owner's boots on real hardware where noted. Treat emulator heap numbers as evidence, not as proof. The Halo figures below are examples from one port. Measure your own and write your own numbers into your gate.

## Rules that saved us

1. **Measure free heap after every load.** Gate: a gameplay map must end its load with at least 2,048 KB free. Log it as `HEAP after load` on every map and every reload, not only the first load.
2. **One big block per section, taken at a clean heap.** Allocate the largest streamed section (BSP in Halo, the area pack in a typical source game) before any small table. Small tables then cannot split the hole it needs.
3. **A refused load prints one named row.** Never fail silently. A refused subtitle blob removed all subtitle text for the rest of d40 (#392).
4. **Every new resident byte has a number and an offset.** A "waiver" is a written decision with a number, not a default. Small waivers add up to a failed gate (see section 9).
5. **Buffers touched by the SH-4 cache, DMA or PVR use `memalign(32, n)`.** Never plain `malloc` for those.
6. **Cut your own additions before any gameplay content.** Trim unused asset data first (Halo's lean body graph freed 383-560 KB per tight map with zero player-visible loss).
7. **Never trust `isfinite` / `isnan` under `-ffast-math`.** On SH-4 it compiles to a constant 1 (#136). Use a bit test and a test that reads the compiled object.
8. **Keep logs quiet on release.** Serial at 115200 baud costs about 94 ms per 1 KB. A verbose log can cost seconds of play.
9. **Count the waivers.** Keep one headroom ledger. Each waived growth is a line in it, with a date and owner.
10. **Save format is versioned and tiny.** A VMU block is 512 bytes. Plan the save size first, then the content.
11. **One animation set per skeleton family; bake only what each area plays; dedupe identical clips.** Halo's player body graph went from 760 KB to 215 to 390 KB per map by baking only the clips the port plays, and byte-equal clips are stored once (a30, 35.5 KB less, measured on the shipped bake). See the animation notes in dc-halo (docs/ANIM.md).

---

## 1. Budget and the gate

- **Main RAM:** 16 MB total. Halo's free heap at menu time was about 7.2 MB, after about 4.9 MB of designed holds (boot 12,258 KB, menu 7,379 KB) [measured, RESOURCE_AUDIT_b12].
- **Gameplay gate:** end-of-load free heap >= 2,048 KB ("2 MB gate", dc-halo AGENTS.md, "Review gate RAM and VRAM"). Under the gate = send the change back.
- **AICA sound RAM:** 2 MB total. Halo's budget was 1,792 KB per map (1,728 on two maps) [measured].
- **VRAM:** 8 MB. Halo kept a reserve of 448 KB free at end of load and logged `VRAM LEDGER`. Under reserve = send back.
- **Static floor tool:** Halo's `campaign_floor.py` used a fixed 11 MiB floor. It is a static sum, not a peak. On one map it undercounted real use by more than 1.2 MB, so use it as a lower bound only.
- **Measure code size, do not guess it** (graphics session, #340). An estimated 450 B diagnostic measured 1,822 B in the verbose image and 48 B in release, by sh-elf-size on the object. Compile the one object before and after in a scratch copy and report the delta as measured.
- **Get a per-allocation ledger before any fix** (reviewer session, #392 memo, #393 arena shrink). The d40 arena and the subtitle floor were first decided from a 2 MB gate and an owner table that disagreed with the free-RAM number. Measure per allocation before you choose the fix.

**Rule for the next port:** pick the three numbers (heap gate, VRAM reserve, AICA budget) on day one and write them in the build gate. Halo's values are a starting point. Measure your own. Do not add them later.

## 2. Fragmentation and the streamed-section arena

**What went wrong.**
- #138 (a50): after load, muster had 1,333 KB free, but the largest hole was 896 KB across 114 free chunks. Its BSP alone was 3,580,920 B. It failed the 2 MB gate for structural reasons, not a leak.
- #393 (d40, d20): end-of-load free was 754 KB (d20_hangar) and 957 KB (d40b) [measured, v0.82 b2 boot]. The top resident item was an unnamed 3,424 KB chunk, "no asset", largest on every load. It was the BSP arena sized for d40c, not an asset. The owner table disagreed with the free number, so the first suspects were wrong.
- Halo's d40c BSP was 3,506,320 B. The largest hole was 3,497,984 B, so the load was 8,336 B short (EXECUTIVEACTIONS, 2026-11-06). Same class of bug on c20 (BSP hold 2.73 of 3.30 MB, failed twice on one switch) and on a10's opening, where four BSP holds were 2,472 B short with OOM from sbrk.

**Fix that worked.**
- One arena block sized for the biggest section of the map, taken once at a clean heap (commit 6e07784c, d40).
- At the first checkpoint on the smaller sections, shrink the arena to the size needed there. d40 went from 3,506,320 B to 3,062,112 B, giving +434 KB free [ESTIMATE, measured on boot].
- A placeholder the size of the biggest section is taken before the small tables, then freed right before the first section load (`campaign_bsp_placeholder_reserve`, docs/archive/bug/BSP_SWITCH_FRAGMENTATION.md).

**Open problem.** The placeholder covers only the first load. Mid-mission switches still take the heap after the previous section's churn. Nothing has measured how bad this gets after a long play session. Measure it with a `HEAPFRAG` row at each switch before you design a fix.

**Rule for the next port:**
- Give each area one arena block, sized from the largest baked section of that area. Take it first.
- Log the arena size and its owner in one row. Name every big block. An unnamed block is a bug in the logging, not a fact.
- Add a per-switch `HEAPFRAG` row from day one.

## 3. Leaks and growth across loads

**What went wrong.**
- Halo's per-load audit (RESOURCE_AUDIT_b12) found no per-load leak: free heap at every menu was 7,224 / 7,384 KB, and one 159 KB step appeared once and then stayed flat. Check each load against the previous menu number, not against the boot number.
- `HEAPLEAK` grew by +395 to +625 KB per cycle and settled at about +540 KB (F-HEAP-GATE, tracker). Part of it was the skull model and part was a real leak. The team could not split the two until a per-class census existed.
- Build 82 (`LEVEL_RESIDENT_RETRY`): after a death, the quick reload failed. The player saw "Campaign assets unavailable" and was dropped to the title. Hypothesis: the big file's `memalign` found no contiguous block on the second load. UNVERIFIED; the logs did not prove it. Default set to 0 (full reload on every death).

**Fix.**
- Per-class heap census rows (`HEAPCLASS`, `HEAPCENSUS`, `HEAPOWNER`) in verbose images only. Owner names come from the loading-screen asset that allocated the chunk (`src/alloc_tag.c`).
- A quick reload that fails must fall back to the full reload before any frame shows, with a reason line (`RETRY: <why>: full reload`).

**Rule for the next port:**
- Test every level change twice in a row (A to B to A, with a death in between). Compare free heap at the menu each time.
- Make the quick-retry path optional behind a switch, default off, until the logs prove it.

## 4. Chunked loading and streaming

- **Bake chunking (#314, #315, commit 25c918cb):** Halo's bitmap bake split each map into chunks with a RAM guard and OOM retry. c40 went from 99.6 s to 21.5 s. Output was byte-identical on all 23 maps. Host peak RAM ran from about 0.7 GB to 6.2 GB per map (BITMAPS_CHUNKS_315 table). Concurrency must come from measured RAM, not from core count.
- **Launcher core setting ignored (#314):** the user set 28 threads, the bake used 32. The budget was not passed to every worker. Pass one budget everywhere (workers, Numba, make -j) and check it in the log.
- **Skip steps that already pass (#315):** each bake step needs a stamp (hash of its inputs and outputs). Rebake only when an input changes.
- **Audio streams through fixed rings:** Halo dialogue and music stream through 16 KB rings, so audio sample rate costs disc space and ring margin, not main RAM. Lowering the dialogue rate freed 0 B of main RAM and 0 B of AICA RAM [measured on the ladder table].
- **Ring margin is a real-hardware issue:** the real-hardware crackle reports trace to the same 16 KB ring. Keep ring sizes in the budget and check them on hardware.

**Rule for the next port:**
- Stream the source's package format in fixed-size chunks per area. Never load a whole source package raw.
- Cut audio rates for disc space, not for RAM. Check the ring margin on hardware.

## 5. Resident vs transient vs lazy

Every allocation is one of three kinds. Name it in the log.

- **Resident:** lives for the whole map. Counts in the gate.
- **Transient:** lives for one operation (a save copy, a decode). Still counts at its peak.
- **Lazy:** uploaded or allocated on first use. Counts in the reserve, not at load.

Lessons:
- Lazy VRAM upload (b8, a30): 15 UPFAILs on particle, lens-flare and weapon textures. Fix: a lazy reserve, where effects and weapon textures upload on first use (`src/level.c`, "Lazy reserve" comment).
- Held-skull model: about 23.5 KB resident on four maps with no offset [ESTIMATE]. The review gate said send back. Decimated to about 120 triangles, about 4 KB [ESTIMATE] (EXECUTIVEACTIONS). Each resident model needs a per-map offset.
- Duplicate counting: the scratch buffer was counted twice on story maps, 77,332 B [PERF_NOTES, 2026-10-21]. Count each buffer once in the census.
- Decode caches per map: moving the bipmove cache out of the BSP struct took the BSP struct from 36,112 B to 288 B. One shared cache of 18,592 B replaced per-part caches (14-26 parts would have cost 0.5-0.9 MB) [PERF_NOTES].
- Matcap vertex cache: 1,536 cached vertices cost about +38.4 KB of heap (#378). It bought frame time, so it was accepted, but it was a trade and was logged as one.

**Rule for the next port:** keep a table of every resident item with its size and map. Lazy items go in the VRAM reserve math. A cache is always a trade against frame time: write the number next to it.

## 6. Load order

- **Small tables before the big block split the hole.** Fix: placeholder for the big block first (section 2).
- **Mask refused on a fragmented VRAM pool (#372):** the Chief's armour mask (23 KB) was refused in play because 185 of 195 KB free had no contiguous block. Fix: load the mask inside the level load, in a slot that uses the existing shrink-and-retry path.
- **`pvr_mem_available()` returns total free, not the largest hole.** `fit_room` thought the pool had room and then five uploads failed at 27-44 KB free on a30_a (RESOURCE_AUDIT_b12). Fix: test the largest hole.
- **Order of content:** first-person hands and vehicle textures before world textures (they are small and always visible). Voices before ambience in the AICA order (RESOURCE_AUDIT_b12, cut 3 and cut 4 are the ambience trims).

**Rule for the next port:** write the load order as a list in the code. Put the gameplay-critical small assets (character body, hands, weapon, UI) first, then the big area block, then ambience and extras.

## 7. Silent floors that dropped features

- **Subtitles (#392):** the subtitle blob had a 2,048 KB RAM floor. On every d40 section after the first, free heap was 1,041-1,945 KB, so the blob was refused. The voice played, the text did not. Only one line showed, and the log said nothing useful. Fix: floor lowered to 512 KB and one refused row printed once (commit 64f116f6). Cost: 3.2-11.7 KB per map with subtitles on (d40: 8 KB).
- **Pause-menu font:** a RAM guard before the dossier font load (commit e43933e6) gave a fallback font with one log line, instead of an empty screen.
- **Halo rule:** every floor that refuses something prints one row, once per load, and the feature's own UI shows the state. "It quietly did nothing" is the worst outcome.
- **Size each floor from its own cost** (graphics session, #392, fix 64f116f6). Subtitles were refused on 24 of 27 loads of one map: the gate wanted 2 MB free for a 20 KB blob. A floor guards the feature's own cost, not the whole gate. Print one line when a gate refuses.

**Rule for the next port:** every feature with a RAM floor prints `REFUSED <feature>: need N KB, free M KB` once per load. Dialogue text, subtitles and HUD fonts are the first to check, because they are the easiest to lose unseen.

## 8. Malloc failure handling

- Every `malloc` / `memalign` NULL goes to a named path: a fallback, a retry, or a clear failure. Never a black screen or a silent return.
- Halo's failure row: `LOAD FAILED: <file>: <n> bytes, <read or header | no contiguous block for the BSP>, free RAM <n> KB`. It names the file, the size asked, and the free RAM. Free RAM next to the file size is what tells fragmentation from a real shortage.
- `UPFAIL` log (dev and PARITYTEST): what was asked, VRAM free, RAM free, reason, load phase. Capped at 32 entries (`UPFAIL_LOG`), allocated on the first failure.
- #289: the d20 hangar failed with "COULD NOT LOAD LEVEL" after the Flood Keyes section had run first. The failure was an order problem that only the load row exposed.

**Rule for the next port:** one `UPFAIL`-style log for all failed allocations, with size, free, largest hole and phase. A failed area load must show a message on screen and offer a way out, not drop to the title.

## 9. Waivers add up

- Halo recorded dozens of "WAIVE" decisions against the 2 MB gate: +1.49 KB (about 0.07%), +3,036 B (0.1%), +977 B, +996 B (0.05%), +700 B (0.03%) and others. Each was small. Together they moved the gate.
- The b10 a50_control map measured 2,046 KB, 2 KB under the gate. Fix: `LEVEL_COLD_SIZE` on 20 functions that never run in gameplay, about 67 KB back [ESTIMATE until boot].
- F-HEAP-GATE stayed OPEN while maps ran on debug-image waivers (c20, d40b, d40d..h, d20).

**Rules:**
- Waivers are for debug images only. Release images do not ship under a waiver.
- Keep one headroom ledger file. Every waiver is one line with bytes, percent of the gate, owner and date.
- Re-check the ledger on each build, not only when a map fails.

**Rule for the next port:** set the gate and a per-area headroom ledger on day one. Refuse a merge that reduces headroom without a line in the ledger.

## 10. Save game size (VMU)

- **Block:** 512 bytes. A VMU file is written in blocks, and the whole file must fit free blocks.
- **Halo save cap:** `SAVE_MAX_BYTES` = 30 blocks minus overhead = 14,208 bytes. A typical a10 save was about 11 KB (24 blocks). The writer drops the units farthest from the player until it fits, and refuses ("Save too large") when the core state alone is too big.
- **Format version:** each format change refuses older saves (`SAVE_BAD_FORMAT`). It is cheaper to refuse than to migrate, but players lose progress. Version 6 refused all saves made before it.
- **Overwrite frees the old file's blocks first** (`vmufs_free_blocks`), so a save never needs double the space.
- **Settings file:** one block (`HALOCFG`). Firefight high-score table: 492 bytes at most, two blocks.
- **Write stall:** each VMU write stalls the game 1.3-1.7 s. Halo's first autosave on every checkpoint froze play. Fix: checkpoints stay in RAM and the VMU is written on quit or by an explicit SAVE row (commit history, 2026-11-07 decision).
- **Conflict in Halo's docs:** SAVEGAME.md says a card holds 128 blocks. GAME_GUIDE.md says a VMU has 200 blocks. Check the real number before you set any save budget. [GUESS: the stock VMU has 128 blocks.]

**Rule for the next port:**
- Decide the save cap in blocks first (for example 24-30 blocks), then the content.
- Save only the state that cannot be rebuilt: script or quest flags, player state, inventory, party, world changes. Rebuild the rest on load.
- Write the VMU on explicit save or quit only. Measure the write stall on hardware. Show a "SAVING" indicator while it runs.
- Version the file from the first build, and log the version in the header.
- **Derive state on load instead of saving it** (code session, #406: carried-vehicle flags are re-marked in vehicle_table_put_back, with no format change). If a flag can be derived from what is saved (position and BSP), derive it. Do not bump the save version for it.

## 11. Alignment: `memalign(32)` and the 32-byte record

- SH-4 cache lines are 32 bytes. A plain `malloc` gives 8-byte alignment, so 32-byte records straddle two lines. Halo's transform buffer (`xf`) was 8-byte aligned and every record straddled a line (PERF_PLAN item 8). Fix: `memalign(32, n)` and `shz_dcache_alloc_line` before each full record write.
- Halo uses `memalign(32, ...)` for: bipmove caches, audio ring, dialogue stream buffers, collision buffers, actor mover buffers, credits art, exec hop buffers. Keep a list; check it on each review.
- `memalign` costs up to 31 bytes of waste per allocation [ESTIMATE]. On a 16 MB machine, count that too.

**Rule for the next port:** any buffer the SH-4 cache, DMA or PVR touches is 32-byte aligned and sized in multiples of 32. Put a check in the review checklist.

## 12. Floating point under `-ffast-math`

- **Trap (#136):** Halo builds with `-ffast-math`. Under it, `isfinite()` is folded to a constant 1 on SH-4 (found by a compiler flag study). So the NaN and infinity checks in the save loader, chunk visibility, damage targets and dynamic lights validated nothing. The source looked correct; only the compiled object showed the bug.
- **Fix:** `src/finite_f32.h`, an exponent-bit test. Cost +540 B of text. (The alternative `-fno-finite-math-only` cost +1,856 B.)
- **Test:** a host harness with real NaN and infinity bit patterns, and a check on the compiled SH-4 object. The unfixed tree failed 4 of 5 checks, which proved the bug.

**Rule for the next port:**
- Source engine code often has many `isnan` / `isfinite` checks. Assume they are dead under fast-math until a compiled-object test says otherwise.
- Write a bit-level finite test in one header and use it everywhere.
- Check the compiled object, not the source, for every such check.

## 13. Logging costs memory and time

- **Serial speed:** 1 KB over serial at 115200 baud takes about 94 ms. A per-frame print will freeze the game.
- **Halo PARITYTEST log grew to 4.83 MB / 45,997 lines (about 7.5 minutes of log)**. It hid the actual gate rows. Fix: start the PARITYTEST serial log at the gate rows (commit 084e695a).
- **Heap census over serial:** the first frame after a load showed a 1.7-1.9 s hitch, because the census itself ran over serial (EXECUTIVEACTIONS, 2026-11-07). Measure the logging cost before you trust a timing number.
- **Release logs are quiet:** one line per summary and errors only. Full rows only in verbose images.
- **Halo's standard row names:** `LOADRAM` (free heap before a step), `HEAP after load` (the gate row), `HEAPFRAG` (largest hole), `VRAM LEDGER` (VRAM budget), `UPFAIL`, `LOAD FAILED`, `RAM LADDER`, `SFX block`, `Texture upload failed`, `SFX ... NOT loaded`.

**Rule for the next port:** use the same row names, one set: `LOADRAM`, `HEAP after load`, `HEAPFRAG`, `VRAM LEDGER`, `UPFAIL`, `LOAD FAILED`. Quiet release logs. Verbose only in a test image, with a rate cap on every per-frame row.

## 14. Emulator vs hardware

- Most Halo heap numbers came from Flycast (hidden emulator runs). Emulator and hardware are not the same machine, so treat emulator numbers as evidence only.
- Every number in the notes is labelled `ESTIMATE` until a hardware boot confirms it.
- Test-image builds exist only for measurement (`-PARITYTEST` name). They never ship to testers.

**Rule for the next port:** label every number as emulator, hardware or estimate. Plan one hardware boot per gate change.

## 15. What cost the most time

Ranked by the dc-halo team's judgement of time lost (no timesheet data; the order is the team's opinion):

1. **Wrong owner on a 3.4 MB block (#393).** The arena was named "no asset", so the first suspects were wrong. Fix: name every big block at allocation time.
2. **Heap gate failing by a few KB after many small waivers (section 9).**
3. **A RAM floor that dropped subtitle text with no message (#392).** The voice played; the text did not.
4. **`isfinite` under fast-math (#136).** Looked correct in source, dead in the object.
5. **BSP arena and fragmentation on mid-mission switches (section 2).** Only the first load was covered at first; the switch case is still open.
6. **Quick-retry reload failing on death (build 82).** Found late, cause still unverified.
7. **Serial log flood hiding the gate rows (section 13).**
8. **Bake concurrency ignoring the user's core setting (#314).** Easy to fix, slow to find.

---

## What your source game may bring

Check each item against your own source files. These are risks to test, not facts.

- **Packages are much bigger than RAM.** A source package can be many times the size of the 16 MB main RAM, so a raw load of a large package may be impossible. Plan per-area baked packs and stream them in chunks.
- **One budget row per area.** Area, pack size, resident size, peak, heap after load, VRAM at end of load, AICA. Fill it from a real bake, not from the unpacked size.
- **Engine allocator and garbage collection [GUESS].** A source engine with its own object allocator and GC may fragment worse than Halo did. Use a fixed arena per area and reset it on area change, rather than trusting the engine to return memory cleanly.
- **Textures [GUESS].** Source textures are often large and VRAM is 8 MB. Trim mip levels per texture, upload lazily, and log every failed upload (UPFAIL).
- **Dialogue and music [GUESS].** Stream through fixed rings, as Halo does. Check the ring margin on hardware.
- **Saves [GUESS].** Source save data may be bigger than a VMU can hold. Decide the cap in blocks first, then store only the state that cannot be rebuilt (section 10).
- **Floating point.** Source engine code often has many NaN and infinity checks. Apply section 12.
- **Subtitles and HUD text.** Large dialogue and UI text is easy to lose behind a floor. Apply the floor rule in section 7.
- **Load order.** The area's streaming pack first, then the core character and weapon assets, then extras. Never the reverse.

## Checklist for every <project> build

- [ ] `HEAP after load` logged for every area and every reload, >= 2,048 KB free.
- [ ] `VRAM LEDGER` at end of load, >= reserve.
- [ ] `UPFAIL` count is 0 on gameplay areas.
- [ ] Every big block is named at allocation time.
- [ ] Waiver ledger updated (bytes, percent, owner, date).
- [ ] No feature refused by a floor without a printed row.
- [ ] SH-4 cache and DMA buffers use `memalign(32, ...)`.
- [ ] Finite checks use the bit test, verified in the compiled object.
- [ ] Save size in blocks is within the cap, and the format version is logged.
- [ ] Release serial log is quiet.
