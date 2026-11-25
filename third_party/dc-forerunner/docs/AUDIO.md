# Audio: lessons from the Halo CE Dreamcast port, for a port of a <source platform> game

Practical guidance for a Dreamcast port of a <source platform> game (<source engine>, <source format> sound banks and audio, KallistiOS). The lessons come from the dc-halo port (Halo CE on Dreamcast). The notes below were written from dc-halo's docs and issues, not from a running copy of its code.

Tags used in this file:
- [FACT] read in dc-halo docs or code.
- [BOOT] seen in a dc-halo boot log or issue. Some were never heard.
- [EST] our arithmetic, not measured.
- [GUESS] not checked. Test before relying on it.

Main dc-halo sources: its audio notes (tiers, residency, permanent cuts), its build doc (disc profile), its executive actions log and its performance notes. Issues: #24, #45, #53, #93, #130, #191, #206, #258, #363.

## Rules that saved us

1. Count what the engine really loads. The planner missed pinned weapon, HUD and device sounds, so each boot found new "NOT loaded" blocks (#53, #93, #45). Model the real resident set, then boot to check it.
2. Every audio decision gets numbers: AICA bytes per map, ring milliseconds, disc sectors, worst BSP. No number, no merge.
3. Never run one audio step alone. The converter steps depend on each other. A partial rebake broke one map's audio and five data tests, and the b10 images were superseded. Run the whole chain per map.
4. Convert each clip once, at its final rate and gain. Store rate and gain in the index. A gain applied twice made every footstep 6 dB quiet (#258).
5. Disc reads never happen inside an audio callback or a frame. Read in a load tick or a planned prefetch step, sector-aligned.
6. Silent failure is a bug. Every failed block prints one line with the reason and the KB. Gate the release on zero "NOT loaded" rows for must-have sounds.
7. Each looping sound is its own voice. One music stream that gets replaced by a second loop makes the music drop in every cutscene.
8. Priorities go in a data table (a tier order), decided once. Not per-boot tweaks.
9. Judge streaming on hardware, not in an emulator. Flycast reads the disc instantly. A real drive stalls.
10. Disc-size cuts live behind the disc profile. The bake record says which profile made the audio.

## 1. Hardware budget

- AICA sound RAM is 2 MB. The KOS driver image takes 192 KB (AICA_RAM_START 0x30000), leaving a pool of 1856 KB. [FACT]
- Dialogue streams reserve 64 KB. A 64 KB margin leaves the sound-effect budget at 1728 KB on maps with dialogue and 1792 KB on the others. [FACT]
- Later the 96 KB plan margin was dropped and runtime demand was measured against 1851 KB (2026-11-05, F15.aica). The rule: must-have sounds keep at least 64 KB free. [FACT]
- 64 hardware channels. One-shots use a round robin that does not check whether a channel is still playing, so a new sound can cut an old one. Loops take channels from the same 64. [FACT]
- Boot numbers: a30 1749 of 1787 KB (#53), c10 1739 of 1787 KB (#93), d40 1739 of 1787 KB (#45). [BOOT]
- Put the budget constants in one header and pin them with a test. The builder and the runtime must read the same numbers.

**Rule for the next port:** take the pool from your KOS build and check it in the first serial log. [EST: same KOS driver, same 2 MB.]

## 2. ADPCM vs PCM, sample rates

- dc-halo uses 4-bit AICA ADPCM for sound effects, dialogue, ambience and voices. [FACT]
- dc-halo's title and music streams are PCM16 stereo (DCM2). At 44.1 kHz that is 176.4 KB/s, four times the bytes of ADPCM. [FACT]
- Re-encoding is lossy twice: source ADPCM is decoded, then encoded again as AICA ADPCM (both 4 bit). Quality loss is accepted, but count it. [FACT]
- dc-halo rate ladder:
  - Sound effects: 11025 Hz flat at first. Class A 16538 Hz (1.5x). Class S 22050 Hz (the retail rate). [FACT]
  - Dialogue: 22050 Hz (retail). Cut to 16 kHz only on the CD-R profile. [FACT]
  - Cutscene voice: 44100 Hz mono (one channel played in the first port). Cut to 33 kHz on CD-R. [FACT]
  - Enemy vocals: 11025 Hz. dc-halo committed 8 kHz vocals for c10 and d20 when AICA ran out (#130). Boot check and the listening test were still open. [FACT: committed, not yet proven]
- The AICA length counter is 16 bit. A clip longer than 65534 samples cannot play, so a lower rate is the fix. Never drop below the old rate. [FACT]
- A rate change changes size directly. A clip at 16538 Hz costs 1.5x its 11025 Hz bytes. Count the bytes of each rate step before you pick it. [FACT]

**Rule for the next port:** ADPCM everywhere in the first pass. ADPCM at 4 bits per sample costs about 48 KB/s for 48 kHz stereo, so a track of N seconds costs about 48 x N KB. [EST] Use PCM only where a stream cannot be ADPCM. It costs 4x.

## 3. Sample conversion pipeline

- dc-halo pipeline: read the source files (from SOURCE_EXTRACT), decode the source format, resample with ffmpeg, encode with KOS `wav2adpcm`, write one manifest row per permutation. [FACT]
- The manifest holds source rate, class rate, gain, skip fraction and length. Later steps read it. Nothing guesses a value that the manifest already has. [FACT]
- Gain: a permutation plays at permutation gain x gain modifier x scale, once. dc-halo applied the modifier in two steps. Footsteps came out 6 dB quiet on every surface (#258, open). Give each gain one owner.
- Loops: a looped ADPCM clip must end in digital silence. The ADPCM predictor carries across the loop wrap, so a clip that ends mid-wave clicks. dc-halo cut loop clips to end in silence. Its error model gives at most 22 LSB against a fresh start. [FACT; the model is not heard, EST]
- Permutation choice: keep a played mask and the last pick, as the source engine does. Without the mask, a two-line vocal repeated about half the time. [FACT]
- If you change a sound reference, the planned class rates change. Then `--skip-audio` is refused late (exit code 2) after the slow steps have already run. Run the full convert after such a change. [FACT, BUILD.md]

## 4. Music streaming

- dc-halo streams music and long dialogue with KOS `snd_stream`. KOS refills a ring when half of it has played. [FACT]
- Refill window = (half the ring in samples) / rate. dc-halo's dialogue ring is 16 KB (32768 samples, mono ADPCM): 743 ms at 22050 Hz, 371 ms at 44100 Hz, 1486 ms at 11025 Hz. [FACT, EST]
- dc-halo's PCM music at 44.1 kHz stereo with a 64 KB AICA buffer per channel has a 371 ms refill window. [FACT]
- Its first design read the disc inside the poll. Each refill was a blocking disc read, and a seek back to the music cost up to the whole 371 ms. The fix: the poll copies from RAM, and the disc read happens in a separate step (64 KB slots, 128 KB at load ticks). [FACT]
- Counters to add before any fix: `poll gap max`, `late` (gap over half the buffer), `dry` (buffer empty, an audible hole), `ring miss`. They are printed in dc-halo's VERBOSE build. [FACT]
- Crackle on a real tester build (dc-halo build 97) was never fully explained. Five hypotheses were listed: dry ring, a DMA race between two refills (fixed with ping-pong buffers), a bogus position register, voices cut by the round robin, and volume wrap (checked, not the cause). Get the counters first, then pick the fix. [FACT: open]
- Loop seam: dc-halo restarts the loop at the file start with a 100 ms equal-power crossfade. The other option is the silent-end clip from section 3. [FACT]
- Volume ramps (fade in, fade out) run per tick. The stop waits until the ramp ends. [FACT]

**Read-ahead ring pattern (for your port).** A source port coded this pattern and had not booted it when these notes were written. Boot it before you trust it.
- A reader thread owns every disc read: 16 KB, sector-aligned, 32-byte aligned, into a read-ahead ring of blocks (the source port used 8 blocks; size the ring from your own budget).
- The music fill step only copies from that ring, and counts `underruns` (one serial row at exit).
- Cost: the ring size in main RAM (<your number> KB), nothing in AICA. The `late`, `dry` and poll-gap counters above are still to add.
- [FACT: code; BOOT pending]

## 5. Dialogue and voice streaming

- dc-halo does not keep dialogue resident. It streams it from disc through four pooled KOS streams: voice, music and two loop slots. A start takes the slot already playing that tag, else a free one, else the oldest lowest-priority one. [FACT]
- Four mono streams reserve 64 KB of AICA RAM. One channel per stream, not two. [FACT]
- Each dialogue index entry carries its own rate and channel count, so the runtime needs no change when a rate changes. [FACT]
- A stream start runs in the frame: open, prefill both halves, queue. dc-halo logs `start max` for it. Not measured yet. [FACT]
- Rule used: scripted dialogue streams from disc. Short combat voices stay resident per area. Core story lines beat ambient chatter.
- Measured on a10: vocals were 333 KB of 1289 KB in the worst BSP. [FACT]
- Voice phasing (unload voices after their encounter) was designed, then measured and rejected. Spatial phasing saved 0 to 24 KB. The best temporal case helped only some BSPs, and trigger order is not in the bytecode (runtime predicates). Measure the saving before you build the scheduler. [FACT]

**For your source game:** dialogue is often the largest audio by bytes. [GUESS] Plan the streamed dialogue budget per map before the resident combat voices.

## 6. 3D positional sounds

- A one-shot takes its volume and pan at start. There is no per-clip update call in the KOS sfx manager. [FACT]
- A loop gets volume and pan updates once per 30 Hz tick, quantised (volume steps of 4, pan steps of 8). At most 8 AICA packets per tick, and 8 loops in total. [FACT]
- Distances come from the source engine's class table (for example, dialogue 3 to 20 world units). Do not invent them. [FACT]
- Pan is taken relative to the camera. A fast mover pans wrong for a second or two. [FACT]
- Stereo clips cannot play positionally on AICA. Positional sounds need mono. [FACT]
- Object-bound script sounds lost their object in the converter (#191, open). They played centred instead of from the object. Count the sounds that have an object and check each one has a position. [FACT]
- Ducking: while a dialogue class plays, other classes drop by a factor from the source table. [FACT]

## 7. Priorities, classes and voice stealing

- Source class table: each class has a maximum number of live instances per definition (4 for most, 6 for projectile impacts) and a preemption time (60 ms to 1 s). Use these, not new numbers. [FACT]
- dc-halo T1 (never cut): HUD warnings, involuntary sounds of drivable engines and guns, player weapons, grenades, player footsteps, doors of the current BSP, pickups, material responses. [FACT, AUDIO_TIERS.md]
- Cut order when short: fire class S down to A, heavy-tag permutations, response variety, melee permutations, ambient detail, chatter. [FACT]
- The rule was variety before chatter. The lead ruled on 2026-11-07 that d40 and a30_a put chatter or allies first, because enemy voices are core gameplay audio. c10 was not extended. [FACT]
- Ambient goes first on a30 (2026-10-28). Gunfire, footsteps and vehicle sound never give way. [FACT]
- Multiplayer announcer cut out mid-line (#206, open). Suspected voice stealing by weapon fire or vehicle loops. Not measured. [GUESS] Give announcer lines a reserved class that fire and ambience cannot preempt.

## 8. Sound effect blocks per area

- dc-halo splits sound effects into groups: common, script, one per BSP (area), and play blocks (footsteps, ambient loops, vehicle loops, vocals). [FACT]
- A tag lives in exactly one group, with its permutations adjacent. A tag in two groups gave a partial set at play time. [FACT]
- A block is read as one contiguous run and fails whole. The failure line names the reason: `SFX block <name> (<n> clips) NOT loaded: no AICA room for <KB> | no RAM | read failed | not contiguous`. [FACT]
- After a scene unload, the heap had free bytes but no hole big enough for the read buffer. The block failed with no message, so one area had silent feet and a silent ambient bed for the whole BSP. Fix: halve the read run down to the longest clip, and print a line on each halving. [FACT]
- Silent guns: weapon sounds that the per-BSP walk did not reach were in no group, so they never played (a30/b30 needler, c10/d20 rocket launcher, d20 shotgun and flamethrower). Fix: every weapon tag is a root of the common group. The build fails if a sound named in the weapon table is in no group. [FACT]
- Footsteps split per BSP was lossless but saved nothing on the tight maps (the worst BSP held every material). Built, then turned off. [FACT]
- Permutation cuts (lead ruling, a30, b40, c40): keep the first 3 permutations of impact and fire classes, drop the rest. On a30: 185 permutations, 949 KB, quality steps 9 to 12. Every distinct sound stays. [FACT, AUDIO_PERM_CUTS.md]
- The budget gate counts what the engine pins, not only the play blocks. Device sounds at 2 permutations, weapons and the HUD counted together. Before this, a30 was 5 KB over at run time and b40's doors were silent. [FACT]

## 9. CD-R disc profile cuts

- Full quality for emulator, GDemu and the single image. Cuts apply only when the play mode is `gdrom`. Each cut is written to docs/BUILD.md with the MB it saves. [FACT]
- An 800 MB CD-R has 405,000 sectors. Burn target 392,850 sectors (3 % free). mkdcdisc warns past 360,000. [FACT]
- dc-halo cuts: `map_mono` 92.5 MB (mono music for the missions), `music_rates` 61 MB (22050 Hz), `voice_rates` 14.6 MB (cutscene voice 33 kHz, dialogue 16 kHz). [FACT]
- Disc 2 was 36.9 MB over the CD-R limit before the cuts. The OUTPOST songs moved to 44.1 kHz and it went about 101 MB over. [FACT]
- The bake record (`encoding.profile` in the audio manifest) makes the tools refuse an image baked with other cuts. Switching the profile means rebaking the audio. [FACT]
- Memory and speed cuts apply in every mode. Only size cuts for the CD-R are gated. [FACT]

**For your source game:** the source disc may be far bigger than Halo's, and may need more than one disc. [GUESS] Count sectors per map and per disc before you pick any rate.

## 10. Disc reads and stalls

- KOS iso9660 reports a 512-byte block size to stdio. A plain `fread` then reads 512 bytes at a time, and each 2 KB sector becomes its own GD-ROM command. Each cache miss also aborts the CD stream. [FACT]
- Fix used: an unbuffered file (`setvbuf _IONBF`), sector-aligned reads, a 32-byte aligned destination, one command per 16 KB stage. [FACT; the multi-sector path is not verified on hardware]
- Load-time reads feed the audio tick. A 200 KB index read runs in 64 KB chunks with a tick between. [FACT]
- Issue #24 (closed 2026-11-15): a disc read stuck on KOS "Previous DMA request" under Flycast. The existing game-side retry did not clear it, and the issue says a real fix probably belongs in KOS. Seen in multiplayer loads, not in the campaign. [FACT]
- A read of about 300 ms near a ring's refill window can cause a dropout. Watch the stream counters on the first roll of any long stream. [FACT]

**Rule:** no disc read inside the audio poll. The poll only copies from RAM.

## 11. Weapon and vehicle audio

- A weapon has several sounds: fire, reload, empty, charge, zoom, impact, detonation, bounce responses. All must be reachable by the packer (section 8, silent guns). [FACT]
- Zoom: the zoom-in and zoom-out tags play at a scale per zoom level. A two-level sniper is silent at its first zoom level in the source. Copy that. [FACT]
- Vehicles: engine loops are attachments (looping sounds). Suspension and crash one-shots fire above thresholds (suspension rise over 0.3 per tick; a velocity change over 0.02 units per tick with contact). [FACT]
- A Warthog engine loop is silent without a driver (its seat tag). Check the seat before blaming the loop. [FACT]
- Vehicle loops load before ambient on maps with vehicles. A 1.8 MB area block filled the budget first before this rule. [FACT]
- A sentinel's beam has no firing sound in the source. Playing the pistol shot for it was wrong. Use the impact sound only. [FACT]

**For your source game:** vehicle and turret loops follow the same load order. [GUESS on counts; measure per map.]

## 12. Other pitfalls seen

- Level music did not stop when a cutscene's music started. The source stops other music only for a loop with a "stops music" flag. Check flags before adding a stop. [FACT]
- Scripted looping sounds never started on d40 and a10 (script skip, #363, open). Run a census of loop starts per map. [FACT]
- A melee-broken door was silent (#187, open). It is linked to the object-loss bug (#191). [FACT]
- A stream start that failed without a message looked like "no sound at all". Print the reason once per failure. [FACT]

## 13. What cost hours

1. Partial rebakes. The audio step ran alone on nine maps. The outputs drifted: one map's audio index was 4,632,672 B in a 4,603,552 B pak, and five data tests failed. Repair needed the full chain per map and a clean full test run. [FACT, EXECUTIVEACTIONS 2026-11-10]
2. A planner model out of date with the engine. Each boot found a new "NOT loaded" block (#53, #93, #45, the d40 carrier sound). Rule: the boot row decides; fix the model to match it.
3. Silent guns found only on boots. Rule: the build fails on any sound that is in no group.
4. Silent heap failure after unload. Rule: print a line for every failure.
5. Crackle on real hardware. Five hypotheses, new counters added, no fix proven. Rule: counters before fixes. The emulator never showed it.
6. Music dropping in cutscenes. One stream was replaced by a foley loop. Rule: each loop is its own voice.
7. Gain applied twice (#258). It was found by ear on one build.
8. Voice rate fallbacks (#130). Each map needed its own voice-block check.

## 14. Quality tiers, hero sounds and residency (BINDING, owner rulings carried from dc-halo 2026-11-16)

Sections 7 to 9 are Halo history. This section is the rule set for this port. Change it only via Lead, after a boot proves it wrong. dc-halo sources: `docs/archive/audio/AUDIO_TIERS.md` (Lead ruling 2026-10-31), `docs/archive/audio/AUDIO_RESIDENCY.md`, `tools/audio/sfx_tiers.json`.

### 14.1 Quality by image

- Emulator, GDemu, single image: FULL quality. Retail rate, no size cut. Disc size is no reason to cut.
- CD-R (`gdrom` profile) only: size cuts (mono music, lower music rate, lower voice rate), each one written to the build doc with its MB. The bake records the profile; tools refuse a mixed bake.
- Memory and speed cuts (AICA RAM, main RAM, CPU) apply in EVERY image. Those are the only cuts the full-quality images take.

### 14.2 Hero sounds: never reduced

Hero = the player hears it every second or it carries the story. Retail rate, every distinct sound kept, never dropped for room:
- Music (streamed, retail rate; mono only on CD-R).
- Story dialogue and cutscene voice (streamed).
- HUD / UI feedback and warnings.
- The player's own weapons, powers / abilities, grenades, reload / empty.
- Player footsteps (1 permutation in gameplay is enough).
- Engines and guns of a vehicle the player drives.
- Doors, lifts and machines of the current area, pickups.

### 14.3 Tier order for AICA residency (gameplay; cutscenes keep their own budget)

| Tier | What | When short |
|---|---|---|
| T1 core | hero sounds of 14.2 that live in AICA | never cut |
| T2 involuntary | enemy pain, death, scream (distinct clips) | never cut |
| T3 ambient | ambient beds (streamed on a free voice stream) and ambient detail one-shots | re-encode lower first, drop last; no silent moments |
| Variety | 2nd+ permutations, responses first; floor 1 permutation + random pitch per play | cut before ambient |
| T4 chatter | ally / enemy barks, group comms, shouting | refused first |

Cut order when short: rate steps of non-hero classes, heavy-tag permutations (keep first 3 of impact / fire classes), response variety, melee permutations, ambient detail, chatter. Every distinct sound stays playable; only copies and rate steps go. Enemy voices that carry gameplay information count as core for that area (Lead ruling per area, logged).

### 14.4 Quality ladder (non-hero sound effects)

- Three rates per class: S = retail rate, A = 1.5 x the floor (her "+50 %" floor), B = floor. Every class starts at B.
- A data table (`sfx_tiers.json` pattern) lists upgrade steps, most valuable first: `[class, tier]`. The bake takes each step in order and keeps it when the area's resident total (core + loops + voices + the step) still fits the AICA budget minus a margin (dc-halo 96 KB); else skips it. Result written per area (`sfx_quality.json`), read by the converter.
- Order of steps: weapon fire, footsteps, damage taken, explosions, reload / ready, impacts, game events, doors to A; then the same top classes to S.
- Rates are fixed at bake time. No per-boot tweak.

### 14.5 Residency: load near the trigger, leave after

- No disc read for a sound while it plays (one-shots read only at a load or prefetch point).
- Stays resident whole area: weapons, impacts, footsteps, HUD, loops (ambient, vehicle, machines: a loop restarts when the player comes back), doors / machines, anything a checkpoint revert can re-arm.
- Phased (one-shot groups): encounter voices while their actors live or are about to spawn; scripted effects with a once-only trigger; device sounds used in one sub-area (cheapest win: phase by area, not by trigger).
- Budget by the worst SIMULTANEOUS set, not the whole-area union. The ladder (14.4) then buys back rate steps.
- A block loads whole or fails with one named row: `SFX block <name> (<n> clips) NOT loaded: no AICA room for <KB> | no RAM | read failed | not contiguous`.
- Every sound named in a weapon / power / HUD table must be in a group; the build fails if not (Halo's silent guns).

## Title music pattern (generic)

A title track is a streamed ADPCM file read from disc. It is never resident.

- **Source chain:** write the chain from the source's music table to the sample file as one row per link (table row, cue, bank entry, sample). Hand-decode it if you must, and boot to confirm. Your source's audio is likely a compressed format, so the first decode step is a converter, not a copy.
- **Bake:** decode the source format with ffmpeg to 48 kHz stereo PCM16 (or keep the source rate), then KOS `wav2adpcm -n -i -t` writes the ADP file. Check the output length against the source length.
- **Manifest:** keep the source rate and length in the manifest, the same way dc-halo does, so later steps read them. Do not resample a second time.
- **Ring:** two halves per channel, sized in KB. Refill window = (samples in half the ring) / rate. Use the formula, not a copied number. For comparison, dc-halo's 16 KB dialogue ring at 22050 Hz gives 743 ms.
- **Poll:** the poll step runs every 20 ms, well inside the refill window. It must not touch the disc. The reader ring from section 4 stays ahead of it.
- **Loop:** the cue loops indefinitely. The fill seeks to the file start at end of file. Check the seam for a click. If it clicks, use dc-halo's fix: a short equal-power crossfade or a silent-end clip (sections 3 and 4). [GUESS on the click; test by ear and with a seam check]
- **Budget:** AICA pool about 1856 KB (same KOS). Set a per-map budget before writing any loader. [EST]
- **Dialogue and voices:** stream dialogue from disc with the four-stream pool. Keep combat voice blocks resident per area. Use the tier order in section 7. [GUESS on the mix; the source's dialogue volume is not counted yet]

## What your source game may bring

- Dialogue and music are the biggest audio by bytes in many games. Count them per map before you pick rates.
- Sound banks may hold more clips than the tier table can cover. Run the "in no group" check (section 8) first.
- Movies or cutscene video in the source may need converting or dropping. Decide early, as a tracker row.
- Music cue chains may stop other music on a flag. Check the flags before adding a stop (section 12).
- Disc: count the audio bytes per map and per disc, then choose the cuts (section 9).

## First-boot checks for the port's audio

- Title music: the counters (`poll gap max`, `late`, `dry`) on the real drive, and whether any refill runs past its window.
- Reader thread: read the `underruns` row at exit; it must be 0 on a real drive. Count disc commands per 16 KB read.
- Loop seam: listen at the wrap of the title track and check for a click.
- Every failed block prints a reason line. No silent failure.
- AICA: print free bytes after the title load and after the first gameplay map.
