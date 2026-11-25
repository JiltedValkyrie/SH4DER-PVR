# Dreamcast Magic

What porting Halo to the Dreamcast taught us, written down so your port does not pay for the same mistakes twice. Read this page first, then the file for the area you touch.

Every lesson in the files below cites the Halo issue or commit it came from, gives the fix, and states the rule for your port. Numbers are labelled MEASURED (a boot log), ESTIMATE (arithmetic) or GUESS.

## The files

| File | Read it before you touch |
|---|---|
| [docs/DEPARTURES.md](docs/DEPARTURES.md) | anything that changes what the player sees, hears or does. The source platform vs Dreamcast gap in numbers, and the rules for departing from the original |
| [docs/GRAPHICS.md](docs/GRAPHICS.md) | rendering, VRAM, textures, LOD, alpha, lighting, effects |
| [docs/MEMORYALLOC.md](docs/MEMORYALLOC.md) | heap, loading, streaming, leaks, saves |
| [docs/AUDIO.md](docs/AUDIO.md) | sound RAM, music and voice streaming, sound banks |
| [docs/DIALOGUE.md](docs/DIALOGUE.md) | conversations, subtitles, translations, any line of text the player reads |
| [docs/ANIM.md](docs/ANIM.md) | skeletal animation, cutscenes, character LOD |
| [docs/SCRIPTING.md](docs/SCRIPTING.md) | the source's script or trigger system, difficulty, checkpoints, save format |
| [docs/SCRIPT_CONVERSION.md](docs/SCRIPT_CONVERSION.md) | how Halo turned mission scripts into a compiled package and VM, and how to map your source's script system onto it |
| [tools/lod/README.md](tools/lod/README.md) | making far / xfar / impostor models: decimation, cards, LOD census |
| [docs/PERFORMANCE.md](docs/PERFORMANCE.md) | frame time, CPU budget, profiling, logging |
| [docs/TOOLS.md](docs/TOOLS.md) | the bake pipeline, image builds, tests, collision bakes |
| [docs/LAUNCHER_AND_EDITOR.md](docs/LAUNCHER_AND_EDITOR.md) | the launcher (six steps, one BUILD button, play modes) and the map editor (per-object choices, saved profile, departures): read before you touch the build entry point or an editor tool |
| [docs/UI.md](docs/UI.md) | menus, HUD, text on screen, menu notes, settings and presets, look rules |
| [docs/LORE.md](docs/LORE.md) | canon, lore sourcing, pronouns, campaign percent and what counts as proven |
| [docs/LOADING.md](docs/LOADING.md) | load screens: the redraw rate during a load, the stage line, the text sheet's VRAM hold, what a load must never do |
| [docs/ACHIEVEMENTS.md](docs/ACHIEVEMENTS.md) | achievement toast, fanfare, achievements page and save file: the look and layout that stay the same across projects |
| [docs/MENUS.md](docs/MENUS.md) | settings and debug menu layout as a template: pages, row types, presets, menu note rules, translation keys, the debug gate |
| [docs/LOD.md](docs/LOD.md) | level of detail: tiers, the 6,000-triangle screen budget, switch distances, fades, impostors and the census rule |
| [docs/VERSIONING.md](docs/VERSIONING.md) | the version number (campaign percent), the build number, image names and the title-screen string, who sets them and when |

## The thirteen rules that cost us the most to learn

1. **Measure before you fix.** In Halo, the first guess about a slow frame was wrong more often than right. Add a timer or a log row, find the bucket that holds the time, then fix that bucket.
2. **Every change states its cost in numbers first:** RAM, VRAM and sound RAM in KB, frame time in ms. No number, no merge.
3. **Free memory after load is a gate, not a hope.** Halo's gate was 2 MB of heap free at the end of every gameplay load, and VRAM above its reserve. Measure it on every map, every build.
4. **Nothing fails silently.** A refused texture, sound block, subtitle table or script node prints one named line with the reason and the size. Silent refusals cost us weeks (subtitles vanishing for a whole level; a look feature refused by a stale check for three weeks).
5. **Contiguous free memory matters, not total free.** Allocate the big streamed block first at a clean heap; small tables after.
6. **Never run one step of a bake chain alone.** Run the whole chain. A partial rebake made a whole set of images unusable.
7. **Trace the original before changing behaviour.** Find the function and its caller in the original game, and write the address or path in the change. Here the original is the source disc and executable; any reference port is a reading aid.
8. **Departures are for hardware, correctness or performance only, never convenience.** Each one gets a tracker row with the reason, the smallest adaptation and what is left different. Anything visible needs the owner's yes.
9. **Clamp colours and check floats.** A negative vertex colour wrapped into rainbow colours on the SH-4. Under `-ffast-math`, `isfinite` is a constant 1.
10. **No disc read inside the audio callback or the frame,** and no per-frame serial print (1 KB of serial costs about 94 ms).
11. **The emulator is not the hardware.** It reads the disc instantly and does not model fill rate. Only a real-hardware boot proves a number.
12. **Test pins follow code in test-only commits.** A stale pin is not a regression; decide which it is with `git diff`, not by guessing.
13. **Bake only the animation the player's actors play.** One skeleton family shares one clip set, each area bakes only the clips its actors can play, and identical clips are stored once. Halo's player graph went from 760 KB to 215 to 390 KB per map this way. Measure the shipped bake, not a scratch bake.

## What your source game may bring

Check these on day one. Each one has cost a port time. The list is generic; your source game may bring none, or others.

- **Big engine packages.** A source package can unpack to tens of MB. Never load it raw: convert each area to a small baked pack at build time.
- **Dialogue-heavy games:** thousands of voice lines, conversation trees, facial animation. Voice streams from disc; only the current conversation's text sits in RAM.
- **Menus built for a runtime the Dreamcast does not have** (Flash, a retained-mode UI engine, a web view). Plan native menus early.
- **Movies in a format with no Dreamcast decoder.** Plan a movie strategy early: a still frame, a pre-decoded stream, or a cut.
- **Save data bigger than the VMU.** Plot flags and RPG state can outgrow a VMU. Plan the save budget before the content.

## How this file stays useful

Add a lesson to the right file in the same change that learns it, with the issue or commit, the fix and the rule. Keep each file short enough to read before starting work.
