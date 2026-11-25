# SH4DER-PVR wiki: structure, page template and rules

This is the plan for the wiki. The pages come after it is approved.

**Model.** The owner's wiki for an earlier port project: one landing page with a nav tree, one page per system, findings with a status, an open-questions page, and diagrams. Read 2026-11-25 at commit `ad6e8d2` (in a sibling repository; its name is left out here under rule 2).

**Location.** This folder holds the wiki. It lives in the repository, with the code, and can be split out later.

**Readers.** Two kinds:

- **people** starting a Dreamcast project with SH4DER-PVR;
- **agents** working in this repository or a client of it.

Both must be able to find a page fast, trust every number on it, and see what is not known.

**Goals.**

1. Explain what SH4DER-PVR is and how to call it, without repeating `README.md` and `AGENTS.md`: link them.
2. Help people set up their own project: budgets, call orders, what to switch off first.
3. Recommend **known or proven limits**. Every limit carries the number behind it and that number's status. Examples:
   - do not cast shadows to an unbounded distance; give the shadow a range;
   - limit SH4DOW-DC to the first N casters within R metres.
4. Keep a **ledger** of hardware facts: one entry a fact, with its status and its proof.

## Tree

```
wiki/
  README.md                  landing page: what this is, how to read a page, nav tree, status legend
  INDEX.md                   machine index: one line a page (path, title, tags, updated)
  llms.txt                   llms.txt-style summary for agents: what the library is, where to start, key links
  STRUCTURE.md               this file
  start-here/
    what-it-is.md            the library, its parts, its dependencies (SH4ZAM, SH4DOW-DC, KallistiOS)
    setup.md                 build, host tests first, SH4ZAM checkout, make targets (links the Makefile and AGENTS.md)
    call-orders.md           one short section an effect, each linking its AGENTS.md section (not copied)
    license-and-credit.md    what LICENSE and AGENTS.md require: credit, no strip, no resale; link, do not restate the terms
  effects/
    <effect>.md              one page an effect: blade, holo, bloom, water, look, frame, taa, glow, warp, trail,
                             lens, focus, visor, torch, guard, bump, sss, uvwarp, spark, bolt, motes, pl4nar,
                             pool, psm, ao, ca, shafts, sh4dow-dc, deform, mist, rise, film, cloudvolume, core
  limits/
    budgets.md               frame budget by unit: SH-4 ms, PVR ms, TA vertices, VRAM, RAM, sound RAM
    frame-budget.md          practices that keep the frame rate: which side bounds a frame, measured feature costs, what to do, the default look
    header-recommended.md    the RECOMMENDED block at the top of each public header: start settings, cost, status, do and do not
    recommended-limits.md    the "do this, not that" table: limit, why, number, status, scene it came from
    scenes.md                the scenes the numbers came from (town, ocean, street, torch walk), with their counts
  status/
    maturity.md              one row a part (effects, SH4ZAM-DCE kernels, demos, suite tools): PROTOTYPE, ALPHA, BETA, STABLE, the evidence, what would promote it
  hardware/
    README.md                how the ledger works, ID scheme, how to add an entry
    ledger-pvr.md            PVR CORE and TA: depth, blend, bump, fog, punch-through, pack, render to texture, fill
    ledger-sh4.md            SH-4: caches, store queues, FPU (FIPR, FTRV, FSRRA, FSCA), copies, cache aliasing
    ledger-bus-dma.md        PVR-DMA, ch2 TA DMA, G2, sound RAM DMA, rates and contention
    ledger-aica.md           AICA and ARM7DI: clock, memory map, interrupts, toolchain
    ledger-video.md          scan-out, front buffer reads, frame read timing
  errata/
    README.md                one entry a contradiction or bug: the claims, who says what, our result, the rule we follow
                             (bump twiddle hang, punch-through rule, PT depth mode, render to texture formats, ...)
  methods/
    benchmarking.md          console only, one line a number, MEASURED vs ESTIMATE, photos and logs, CDI folder rules
    host-test-first.md       host tests, SH4ZAM software back end, what the host cannot show
    sh4zam-rule.md           SH4ZAM maths everywhere, asm hooks (sh4zam-dce), C reference beside each, no slow code
    fault-catcher.md         the crash and hang catcher, breadcrumbs, how to read a fault page
  shadows/
    README.md                map of the shadow work: routes A, B, AB, the c-spikes, the thesis
    psm-routes.md            SH-4 depth map, PVR-drawn map, the AB hybrid; what each costs
    bump-compare-chain.md    c1 .. c4: the PVR-side compare, bands, bias, 1555 mask
    thesis.md                summary of THESIS_SH4_SHADOWS.md with its predictions and their status
  research/
    README.md                one line a research note, with its date and what it settled
    (links to demo/psm/*_NOTES.md; a page here summarises, the note stays where it is)
    outside-sources.md       third_party/skmp (polly2-rtl, dreamcast-tests, nulldc3 with refsw2): what each is,
                             its licence, cite only, never copy; an emulator result is REPORTED, "emulator, not evidence"
  glossary.md                PVR, TA, ISP, TSP, tile, list, 1/w, Q, K1..K3, twiddle, FIPR, FTRV, store queue, ...
  open-questions.md          every OPEN item from every page, in work order, with the test that would close it
  images/                    shared diagrams (SVG), our own
  archive/                   one-line "moved" pointers when a page is renamed
```

File names are lowercase, with hyphens. Links are relative. A renamed page leaves a pointer in `archive/`.

## Page template (effects, limits, shadows)

```
---
title: <name>
tags: [effect|limit|hardware|method|shadow|research, unit tags: pvr sh4 ta dma aica vram ram]
updated: YYYY-MM-DD
sources: [paths in this repo, doc sections]
---
# <Name>

**What you will learn:** one line.
**For:** who the page is written for (for example "a programmer adding the effect to a Dreamcast game").

One paragraph in plain sentences: what it does and when to use it. A term is defined on first use or linked to the glossary.

## Call order          link to the AGENTS.md section; three lines at most here
## Cost                SH-4, PVR, VRAM, RAM: each number with a ledger ID or a status tag
## Recommended limits  table: limit | value | why | status | scene
## Tuning              link to the README tuning table; which knob to turn first
## Pitfalls            what breaks and how it looks on screen
## Open                what is not known, each with the test that would settle it (also in open-questions.md)
## Related             links
```

## Ledger entry (hardware/ledger-*.md)

One table row a fact. The ID never changes, even when the fact moves.

| Field | Content |
|---|---|
| ID | `PVR-012`, `SH4-004`, `BUS-003`, `AICA-002`, `VID-001` |
| Claim | One sentence, our own words. |
| Status | One tag from the legend below. |
| Evidence | MEASURED: console count, date, and the photo or log name. FACT: the doc and section. REPORTED: who, and where. |
| Source | The file in this repo that holds the detail, with a line or section (for example `demo/psm/compare/NOTES.md` "c3b"). |
| Test | The program and page line that measures it, or the test that would. |
| Used by | The effects and limits that rest on it. |

## Status legend

| Tag | Meaning | Required with it |
|---|---|---|
| MEASURED | Our own run on a real Dreamcast. An emulator is never evidence. | Date, console count (one console so far), photo or log, program and page line. |
| FACT | Read in a primary document (Sega, Hitachi, Yamaha), or in source we hold, where the source *is* the behaviour (for example what KallistiOS writes to a register). | Doc and section, or file:line. |
| REPORTED | A third party says it: KallistiOS comments, homebrew, skmp's console tests, an emulator's code (an emulator is its authors' model). | Who, and where. Cite only. |
| ESTIMATE | Our model, host run or arithmetic. | Which model, and what it assumed. |
| DISPUTED | Sources disagree. | Every side, with its tag. The rule we follow until it is measured. |
| OPEN | Not known. | The test that would settle it. Listed in `open-questions.md`. |

A limit in `recommended-limits.md` is **PROVEN** only when the number behind it is MEASURED. Otherwise it is **ADVISED**, and carries the tag of its number.

## Rules for every page

1. **Our own words.** Link `README.md`, `AGENTS.md`, `LICENSE` and the notes; do not copy them.
   - Do not copy SH4DER-PVR source or its tuning tables: link the README tables.
   - Do not copy anything from `third_party/skmp` (no licence): cite file and line only.
   - Do not copy SH4ZAM, SH4DOW-DC or KallistiOS code.
2. **No game or engine names** in this folder (the AGENTS.md rule). A scene is named by what it shows, for example "the town scene" or "the street scene".
3. **Every number carries its status, its date and its console count.** MEASURED and ESTIMATE are never mixed in one cell or one sum. A sum of the two is an ESTIMATE.
4. **Cite.** A claim names its source: file:line, doc section, or photo or log.
5. **Safe wording.**
   - Never "first ever" or "only". Write "we know of no earlier use".
   - Never "fast" without a number.
6. **Unknowns are listed, not guessed.** An OPEN line with its test is worth more than a smooth sentence.
7. **One fact, one place.** The ledger holds the fact; pages link its ID. A page never restates a ledger number without the ID.
8. **Readable by agents.**
   - Front matter on every page.
   - Fixed section names (the template).
   - One line a page in `INDEX.md`.
   - IDs that are stable.
   - No meaning carried by a picture alone: every diagram has its words beside it.
9. **Credit.** A page that tells a client to use a part of SH4DER-PVR repeats the credit rule by link to `wiki/start-here/license-and-credit.md`.
10. **Dates.** Absolute dates (YYYY-MM-DD), never "last week".
11. **People first.** Every page opens with "What you will learn" and "For". Plain sentences; terms defined on first use or linked to the glossary; tables after the words that explain them.
12. **Outside sources.** `third_party/skmp` is read-only and git-ignored. Cite it; never copy it. nulldc3's licences are mixed (Reicast's BSD 3-clause, with GPL and LGPL files), and polly2-rtl and dreamcast-tests have no licence file. An emulator's output is never MEASURED: it is REPORTED, "emulator, not evidence".
13. **Limits.** In `recommended-limits.md` a limit is PROVEN only when its number is MEASURED. Every ADVISED limit names the test that would prove it.

## INDEX.md and llms.txt

- `INDEX.md`: one line a page, sorted by folder:
  `path | title | tags | updated | one-line summary`.
  The ledger files also list their ID ranges.
- `llms.txt` (the llms.txt convention):
  - an H1 with the name;
  - a quoted one-line summary;
  - short sections ("Start", "Effects", "Limits", "Hardware ledger", "Methods", "Open questions"), each a list of `[title](path): what it answers`.

## Step 2: how the pages get written (after this file is approved)

Read-only harvest agents, one a source group. Each returns facts with source, line and status tag, never prose to paste. The source groups:

1. `AGENTS.md`, `README.md`, `LICENSE` (start-here, call orders, credit).
2. `include/*.h` macro tables and header comments (effect pages: what, cost notes, limits stated in comments).
3. `demo/psm/PVR_FEATURES.md`, `ROUTE_AB_NOTES.md`, `POLLY2_NOTES.md`, `ARM7_TOOLCHAIN_NOTES.md` (ledger-pvr, ledger-bus-dma, ledger-aica, errata).
4. `demo/psm/bench/NOTES.md`, `demo/psm/compare/NOTES.md` (MEASURED entries, with photo names).
5. `demo/psm/THESIS_SH4_SHADOWS.md`, `OFFLOAD_PLAN.md`, `GPU_GEMS_NOTES.md`, `PRIOR_ART.md`, `TOWN_*.md`, `DC_MOCKUP.md`, `LATEGEN_NOTES.md`, `SHZ_TRIG_PROBE.md` (shadows, research, scenes).
6. `sh4dow-dc` README and `third_party/dc-forerunner/DREAMCAST_MAGIC.md` (methods, sh4dow-dc page, ledger-sh4 lessons).

I assemble the pages and the ledger, then seed `limits/` with what we have, each number tagged, gaps OPEN. Examples:

- the town's vertices a frame and its vertex-buffer use;
- SH-4 and PVR ms by route;
- store queue against `pvr_prim`;
- copy and DMA rates, sound RAM included;
- the cache aliasing trap;
- SH4DOW-DC's caster count × range, OPEN until benched.
