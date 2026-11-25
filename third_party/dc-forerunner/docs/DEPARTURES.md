# Departures: how a Dreamcast port changes the original

Status: TEMPLATE, planning only. The rules and numbers here come from the Halo CE to Dreamcast port (dc-halo). Every Halo number is a Halo number. Every number for your source game is `<your number>` until a build proves it. Mark it ESTIMATE or UNKNOWN until then.

Source of the rules: the dc-halo repo (files `AGENTS.md`, `docs/EXECUTIVEACTIONS.md`, `docs/plans/FEATURE_GAPS.md`). Those files are the reference. This file keeps the rules in short form.

---

## 1. The gap in numbers

### 1.1 Your source platform against Dreamcast

Fill the source column first, from the platform's own spec sheet. Where you derive a peak figure (cores x clock x lanes), mark it DERIVED.

| Item | <source platform> | Dreamcast (DC 16 MB RAM, 8 MB VRAM, 2 MB AICA) | Gap | Note |
|---|---|---|---|---|
| CPU | <your number> (cores, clock, SIMD) | 1 core, SH-4, 200 MHz, in-order, SH-4 SIMD (FIPR, FTRV) | <your number> / 200 MHz per core | Clock alone is not the cost. Measure per-frame vertex and triangle counts |
| CPU peak, single precision | <your number> | about 1.4 GFLOPS (commonly cited) | <your number> / 1.4 | Derive the source figure and mark it DERIVED |
| GPU | <your number>; programmable or fixed function? | PowerVR2 CLX2, tile-based deferred renderer, fixed function, no programmable shaders | not like-for-like | No shader port. Effects are rebuilt from the fixed function pipeline (see GRAPHICS.md) |
| GPU peak | <your number> | marketing polygon and fill figures; treat as ceilings only | not like-for-like | Do not plan from PowerVR2 marketing numbers |
| Main memory | <your number>, shared or not | 16 MB SDRAM | <your number> / 16 MB | |
| Video memory | <your number> | 8 MB VRAM (textures, framebuffer, vertex lists) | n/a | |
| Framebuffer | <your number> | inside VRAM | n/a | |
| Sound | <your number> (decoder, channels, rate) | AICA (ARM7 + 64 channels) with 2 MB sound RAM; Halo's usable pool was 1,769,472 B | n/a | Count sound RAM as its own pool |
| Total memory, all pools | <your number> | 26 MB across three pools | <your number> / 26 MB | Never add the pools into one budget |
| Disc | <your number> raw, <source disc format> | GD-ROM, about 1 GB usable; CD-R, 700 MB | <your number> / 1 GB | The CD-R profile is a cut for 700 MB (see section 4.9) |
| Output | <your number> | 640x480 maximum | <your number> | A 720p output is 3x the pixels of 640x480: a real fill cost |

Unsure, mark as such until checked: the source platform's peak figures, the usable size of its disc format, the output mode the source game used, and all PowerVR2 marketing figures.

### 1.2 Halo's own step (for scale)

Halo was ported from a first-generation console (2001, 733 MHz, 64 MB unified memory, DVD) to Dreamcast.

| Item | Halo source platform (2001) | Dreamcast (Halo port) | Gap |
|---|---|---|---|
| CPU | 733 MHz, one core, SSE | SH-4, 200 MHz | about 3.7x clock |
| Memory | 64 MB unified | 26 MB across three pools | about 2.5x less |
| Disc | DVD | GD-ROM / CD-R | the port split and cut for it |

Halo was already a large step down. It shipped on Dreamcast with cuts to textures, models, LOD, audio and AI clips, and the gameplay stayed 1:1. That is the proof that the method works.

### 1.3 What that means

- Halo's step (source to Dreamcast): about 2.5x less memory, about 4x less clock.
- Your step (<source platform> to Dreamcast): <your number> less memory, <your number> less clock per core, <your number> less disc.
- Compare the two. Halo proves the method. It does not prove that your cuts will be as small. If your step is much bigger than Halo's, expect more cuts, and plan the cut list before the first build.
- A straight port of the source build will not run. Any Dreamcast port is a cut port. The only question is which cuts keep the game.

---

## 2. Why the game might still fit (the owner's feeling, argued)

The owner feels the Dreamcast can run the game, or parts of it. The argument below says the feeling is plausible for slices of the game. It is not proven for the whole game, and the first measurement may kill parts of it. Argue it part by part, never as a whole.

### 2.1 What the game needs on screen at once

- Combat spaces: count enemies, allies and draw calls per room. ESTIMATE until measured on the source build.
- Dialogue scenes: 2 or 3 characters, a fixed camera and a locked set. The cost is the faces, the voice stream and the baked light, not the crowd. A cutscene keeps its own budget (Halo's rule), so dialogue work is separate from the gameplay budget.
- Open terrain: the hard case. It needs far geometry and streaming from disc. Halo handled large far geometry with far cuts and fog. Open terrain is where the gap bites hardest.
- Crowd hubs: crowd density is a cut you can make without touching the story, as long as quest characters and shops stay.

### 2.2 What the Dreamcast does well

- PowerVR2 tile-based deferred rendering: hidden surfaces are removed before texturing, so overdraw is cheap. Translucent polygons are sorted in hardware, so the port does not need a CPU sort for them (Halo's notes call this "free alpha sort").
- VQ texture compression: 8:1 against 16-bit texels (2x2 block to one 8-bit index). Textures are the first big memory win.
- SH-4 store queues: 32-byte bursts to the PVR. Vertex submission is fast if it goes through the store queue and a direct render path.
- SH-4 SIMD (FTRV, FIPR) for matrix work. Use SH4ZAM for math (Halo rule: check it before hand code).
- Output at 640x480 is 3x fewer pixels than 720p. That is a real saving on fill.

### 2.3 What Halo taught

- Far cuts can go far while the game still plays 1:1. Halo's crate models use a far mesh of 12 to 24 triangles, only beyond 6 wu. Silhouettes change at distance; the play does not.
- Look costs go to the look: baked light instead of live light, fewer permutations of idle animation, one far mesh per model. Gameplay keeps its own rules.
- Memory is gated per map, not per game. A map that fails the gate is cut before a gameplay rule is touched.
- Disc profiles: the full quality build is for GDemu and emulators. Cuts that only save CD-R space (voice rate, voice count, music rate) apply only to the CD-R profile.
- The 6,000 on-screen polygon guide (Halo, for 30 fps) is a useful start for a Dreamcast frame budget. Your game's own count is UNKNOWN until measured.

### 2.4 Verdict (argued, to be measured)

- Plausible: <the scene types you expect to fit, e.g. title screen with music, small combat rooms, dialogue scenes, interiors at reduced crowd>.
- Open: <the heavy scene types, e.g. open terrain, the largest crowd, every map at once, all voice at full rate on one disc>.
- Not plausible without departures: running the source build as it is.

Next steps, in order:
1. Measure the source build: per-room triangle count, draw count, texture memory, audio stream count, disc bytes per level. Use your source reader (one per source format; see `tools/build_assets.py`) and the platform's reference tools to read the data, not to copy it.
2. Title screen with music on Dreamcast: memory and disc numbers from a real boot.
3. One combat room, one dialogue scene, one open-terrain patch. Each gets its own gate numbers.
4. Only then the largest crowd and the open terrain.

---

## 3. Departure rules (from the Halo project)

Halo's rules, quoted where quoted. Copy them into your port; re-measure every number.

### 3.1 Why a departure is allowed

- "Departure only for Dreamcast correctness, hardware limit, performance. Say constraint, smallest adaptation, leftover difference. Convenience no reason." (Halo AGENTS.md, "OpenCE is bible".) For your port the reference is the source build. If your game has an open re-implementation, that is the reference too (Halo's was OpenCE).
- Push back on an out-of-scope ask. Offer the original-compatible way first.
- Evidence missing? Say so, investigate. Keep three things separate in every note: confirmed, hypothesis, adaptation.
- A reference port on another platform can inform optimisation. It never beats the original.
- Before any change to gameplay, UI, audio, animation or resource lifecycle: trace the original function and its caller, and write the source address or path in the docs or the change description.

### 3.2 Who signs

- Parity (the source-game expert and code reviewer; session dc-<project>-parity) departs only for a RAM or VRAM limit, or on Lead's or the owner's ask.
- Any departure needs a tracker row: status DEPARTURE plus the reason.
- Anything visible needs the owner's yes. "No new visible look without owner say."
- **Look defaults need the designer's verdict and a cost note before the default flips** (Halo: #394 grain default, #404 LOW object detail, #405 volume-light palette). A look default goes in only with the designer's verdict on the real draw code, in both themes, and a note that names the cost. See the UI notes, section 7 (Halo: docs/UI.md).
- Lead logs the call in `docs/EXECUTIVEACTIONS.md`: date, question, each side, call, why. Lead decides when the team does not agree.

### 3.3 Memory and disc gates

Halo's values are shown. Set your own from your first measured boot.

| Gate | Halo value | Your value |
|---|---|---|
| Gameplay map end-of-load heap | under 2 MB free: send back | <your number> |
| Texture upload failures | above 0: send back | <your number> |
| VRAM free after load (reserve) | 448 KB | <your number> |
| Resident memory growth | with no offset: send back | same rule |

Rules:
- Say the cost in numbers before code: KB of resident memory, KB of VRAM, KB of sound RAM, and per map if they differ.
- No memory line in a review: not a pass.
- If a new asset does not fit, reuse a similar resident asset and write a DEPARTURE row with the reason. Do not drop the beat.
- Flavour yields first: story and gameplay are kept before any look.
- Gameplay is the gate. The cutscene keeps its own budget.

### 3.4 Multiplayer

If your source game has a multiplayer mode, keep Halo's rule ready: shared multiplayer state stays faithful to the original; look, local and bot-host changes are allowed. If it has none, skip this section.

### 3.5 Real departures from Halo

Pulled from Halo's `docs/plans/FEATURE_GAPS.md`, rows with status DEPARTURE. About 120 rows mention DEPARTURE in that file (grep count, some duplicated). These 14 teach something. Numbers are Halo's.

| ID | What changed | Why | The number | Leftover difference |
|---|---|---|---|---|
| F-DIET-B30-CLIPS | Cut AI action clips (dive, evade, land, panic, and similar) from the campaign pak | RAM | about 167 KB | Those clips never play. The port already played none, so no change on screen against today. |
| F-DIET-IDLE-PERM | Looped idle and move play one fixed permutation, not a random pick | RAM, cosmetic | none recorded | Random idle variety is gone on those maps. |
| F-CSTORAGE-FAR | Far meshes for two crate models, beyond 6 wu | Performance | 198 and 182 vertices saved per far crate; ESTIMATE 10 to 14 ms on a 27-crate frame | The source picks geometry by pixel size; Halo had one mesh per slot. Silhouette differs at distance only. |
| F-SPAWN-CAP | At most 2 new bodies placed per refresh | Hitch on a spawn wave | 22 bodies take 0.73 s | A unit appears up to N-1 ticks late. A waiting unit cannot be hit or seen. A script op on a waiting unit flushes the queue. |
| F-H2-TEXTURES | Halo 2 arena base maps to 256 px, detail maps back, only where VRAM allows | Look; VRAM headroom | Ivory Tower +288 KB of 290 KB; Midship +336 KB of 348 KB | The arena look is Halo's own; no original counterpart. Blocked where end-of-load VRAM free is under the reserve. |
| F90 | Five sound blocks refused on one map | Hardware limit (sound RAM) | 128 KB refused; 921 KB main RAM free at that step | Those loops are missing on that load. Accepted as a docs-only ruling. |
| F-CINE-SIMPLE-LIGHT-294 | Baked cutscenes use baked lamp light, not live lights | Owner ask ("for now"); performance | none recorded | The original has live lights in cutscenes. Gameplay keeps the owner's setting. |
| F-WARTHOG-GLASS-FRESNEL | Windshield diffuse uses a fresnel alpha ramp, not the authored mask | Look ("looks broken") | 7,510 B bitmap; RAM, VRAM and sound 0 | Tint and alpha differ from the authored mask. The runtime shine still draws on top. |
| F-LIGHT-HALOS | Weak halo on placed lamp flares, campaign only | Look (owner idea) | about 2 KB VRAM for the ring, first use only; plus a soft disc sprite | The original gives only the sun a corona. This adds a second look to lamps. |
| F-EYE-ADAPT | Screen brightness adapts to bright light cores; capped at 25 % | Look (owner idea); Performance turns it off | one untextured multiply quad a frame (cost not in the row) | The original has no eye adaptation. |
| F-MAP-PIN-BAKE | Non-main maps bake their own editor placement; disabled actor rows stay enabled | Owner order: "keep the spawns" | a50: 23 spawn rows kept | Disabled scenery is hidden, lights and LOD apply. The spawn rows are not an option. |
| F-RING-RELIEF | Title ring has relief shading; one glint on the title only | Look (owner ask) | scene text +804 B; star 512 B VRAM | The sun direction drives only the lens flare, not the ring's lighting. |
| F-XP-CONTENT-MANIFEST | Console join does not check every staged file | Performance; NOT measured | ESTIMATE: seconds per join; total size not measured | A damaged staged file is not refused on join. The image-time check still covers it. Status: decision pending. |
| F-NW-COMPACT-PRECISION | Network wire quantises positions to 1/256 wu, velocity to 1/1024 wu per tick | Modem and broadband budget of the server | position error at most 1/512 wu per axis | The Dreamcast only decodes; the host sends the coarse values. Lead yes on record, a hard case. |

Pattern from these rows:
- Every DEPARTURE row names the constraint, the number, and the leftover. Rows with no number (look only, or not measured) are the ones that stay open the longest.
- Look departures need the owner's yes and cost almost no RAM or VRAM. Keep them separate from memory departures in the tracker.

---

## 4. Likely departures for your source game, ranked (PREDICTIONS)

Every item below is a prediction. Before any of them becomes a departure row, measure the source build for it. Story, lore and cinematic facts come from lore research (see LORE.md), checked against the source data before anything is wired.

Ranking is by how likely the cut is and how much it saves. "Must not change" is the same across all of them: the story, the choices, the dialogue, and the combat rules.

### 4.1 Movies (intro and story movies)

- Why: the Dreamcast has no decoder for <source movie format>, and full-rate video would eat VRAM and disc.
- Smallest adaptation: keep each movie's timing and audio; lower the resolution and frame rate; stream from disc with the audio at the lowest rate that still reads as speech. Pre-rendered frames can be VQ-compressed.
- Must NOT change: the content, the cut points, subtitles, and the ending. A movie the story depends on keeps its last frame and its timing.

### 4.2 Middleware UI to native menus

- Why: a middleware UI runtime is too heavy for 16 MB main memory and 8 MB VRAM. ESTIMATE; the runtime's cost is UNKNOWN until tried.
- Smallest adaptation: native menus with the same layout, text, option list and order.
- Must NOT change: menu options and their logic, what each dialogue choice does, journal and codex text, and the save and load flow.

### 4.3 Dynamic shadows and normal maps to baked vertex light

- Why: live shadow maps and per-pixel normal lighting cost fill and VRAM the Dreamcast does not have.
- Smallest adaptation: bake light and shadow into vertex colour or light maps (Halo's "baked lamp glow and room light grid" is the pattern). Keep live light only where gameplay needs it (a moving light the player must read).
- Must NOT change: the mood of key story moments, the light from story sources, and anything the player uses to aim or find cover.

### 4.4 Texture resolution

- Why: 16 MB main memory and 8 MB VRAM; VQ gives 8:1 but still needs a budget.
- Smallest adaptation: cap base maps (Halo capped at 128 px and found the arena cost) and bring detail maps back only where the player is close. Stop at the VRAM reserve.
- Must NOT change: readable text on signs and HUD, the faces of key characters (their identity must still read), and the colours the story uses.

### 4.5 Draw distance and LOD

- Why: 6,000 on-screen polygons for 30 fps (Halo's budget) is far below a modern scene.
- Smallest adaptation: far meshes beyond a distance (Halo: 6 wu floor for crates; the number is per model and must be measured for your game). Fog covers the far end.
- Must NOT change: the silhouettes of story landmarks, enemies and cover the player needs to read, and the playable space.

### 4.6 Facial animation detail

- Why: facial rigs with many bones and per-frame updates are a CPU and memory cost; many faces at once are common in dialogue scenes.
- Smallest adaptation: fewer facial bones or a lower update rate on background characters; full detail on the one or two speaking characters in close-up.
- Must NOT change: lip sync timing against the voice line, eye contact and the emotional beat of each line, and the choice points.

### 4.7 Open-terrain streaming

- Why: open terrain needs far geometry and streamed tiles. The source streams from its disc at a much higher rate than the GD-ROM (ESTIMATE; measure the real GD-ROM read rate on hardware).
- Smallest adaptation: tiles load on approach, with fog to hide the edge; far terrain is a cut mesh; the handling of vehicles or movers is unchanged.
- Must NOT change: the handling, the places enemies and collectables sit, the landmarks, and the places the player can reach.

### 4.8 Crowd density

- Why: many background characters each cost a model, animation and voice stream.
- Smallest adaptation: a lower background count with a spawn cap (Halo's spawn-wave cap is the pattern: spread placement over ticks; the cost is a late appearance).
- Must NOT change: quest characters, shop keepers, the people the story names, and who is in each area.

### 4.9 Audio rates and voice count

- Why: 2 MB sound RAM and 64 channels cannot hold the source's compressed voice and music at full rate. Most sources decode in hardware; on the Dreamcast it is software or ADPCM.
- Smallest adaptation: voice at a lower rate (Halo's CD-R profile used 33 kHz voice) and fewer simultaneous voice channels. Apply the cut only in the CD-R profile; GDemu and emulator keep full quality.
- Must NOT change: every line's text, timing and speaker; music cues on story beats; and any line that a choice depends on.

### 4.10 Disc split across several discs

- Why: the source disc is <your number> raw. A GD-ROM holds about 1 GB and a CD-R 700 MB. A full split may be needed. The real data size is UNKNOWN until measured.
- Smallest adaptation: split at chapter or area boundaries, never in the middle of a fight or a conversation. Disc profiles: full quality on GDemu and emulator; cuts only for CD-R.
- Must NOT change: story order, the save file's meaning across discs, and the loading of a mission's start point.

---

## 5. What to measure first (checklist)

- Source build: per-room triangle count, draw count, texture memory, number of audio streams, disc bytes per level.
- Dreamcast build: heap after load per map, VRAM free after load, sound RAM used, frame time per room, and the GD-ROM read rate on real hardware.
- Each measured number goes into the tracker with its status: CODED (need boot), DONE, OPEN, or DEPARTURE (reason).
