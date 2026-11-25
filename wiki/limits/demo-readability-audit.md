---
title: "Demo readability audit, 2026-11-25"
tags: [limits, practice, demos, suite, audit]
updated: 2026-11-25
sources: [wiki/limits/demo-readability.md, wiki/hardware/ledger-pvr.md]
---
# Demo readability audit, 2026-11-25

**What you will learn:** how each demo and test program scores against the readability standard, and the fixes ranked cheapest first.
**For:** whoever fixes a program's screens, and whoever films a console run.

The standard is [demo readability](demo-readability.md). The sources are the code (a survey of the current tree, with each string's file and line) and the console videos read so far: ao4-j, d1-c, vc-a, c8 and C9T. Paths are relative to demo/.

## Scoring

Each program gets 1 point for a rule that is met, ½ for a rule that is partly met and 0 for a rule that is missed.

The six rules:
1. **First screen:** it says what the program is and what to look at.
2. **Stage titles:** plain titles, on screen during the run, each saying what to compare.
3. **Keys:** printed from one key table, showing only the keys that work in the current mode.
4. **Verdict first:** each result page starts with a plain verdict.
5. **Plain names.**
6. **No game or engine names.**

## Scores

| Program | 1 First screen | 2 Stage titles | 3 Keys | 4 Verdict first | 5 Names | 6 No game names | Total | What a first-time viewer sees first |
|---|---|---|---|---|---|---|---|---|
| Suite menu | ½ | 1 | 1 | n/a | 0 | 1 | 3.5 / 5 | "PSM TEST SUITE", the two groups each with a line, every row with "what" and "read" text (suite_nav.h:239-273). Row names are code names: ao4, ao-lamp, hens-eyes, ao1c-live, hud1c-2 |
| pvr-dma | 1 | ½ | ½ | 1 | ½ | 1 | 4.5 | It opens straight on its page, and that page starts with "PROVES: ... YES, every copy below is right" (ao/pd_spike.c:217). This is the model the other programs should follow |
| mip-a | 0 | 1 | ½ | ½ | 1 | 1 | 4 | Stage titles on screen with what to read: "Brighter = a smaller mip level" (ao/mip_spike.c:454). The verdict comes after the T1 rows |
| d1 / d1-c | 0 | ½ | 0 | ½ | ½ | 1 | 2.5 | The T1 held picture with a PASS caption (ao/d1_spike.c:535). The T3 titles ("plain \| fade") never say what to compare. The page's LOOK lines are fixed text that reads like a result (:915-918); in d1-c they described a change of 2 % (PVR-191) |
| corr-a2 | 0 | ½ | 0 | 0 | 1 | 1 | 2.5 | "corr: building the corridor ...". Stage names are on screen but are jargon ("bias slope 1.0 l2"). There is no verdict ("RESULT OPEN until read off the screen", corr/corr_spike.c:658) |
| ao4 | 0 | ½ | 0 | 0 | ½ | 1 | 2 | "BAKING scenes A and B's AO (64 rays a vertex)". The 19 stages are numbers only. The live row names its methods well ("1 BAKED: a byte a vertex", ao/ao4_spike.c:380). The keys are a hand-written line. ao4-j was stopped by the pad in stage 17 (PVR-186): nothing on screen said "hands off" |
| lamp | 0 | ½ | 0 | 0 | ½ | 1 | 2 | No first screen. Stage names appear only on the results page (ao/lamp_spike.c:634-641, 745). Its one summary line comes after the numbers (:760) |
| hens-eyes | 0 | ½ | 0 | 0 | ½ | 1 | 2 | No first screen. Stages are numbers. The label hide is d-pad LEFT + B, where ao4, lamp and d1 use UP |
| town (live) | 0 | ½ | 0 | n/a | ½ | 1 | 2 / 5 | No on-screen title; the start-up text goes to serial only (psm_dc.c:656). The HUD header reads "ROUTE A PVR DRAWS THE MAP". The keys are a hand-written HUD line, with the pad mapped separately (psm_scene.h:1572; psm_dc.c:402) |
| town vc disc | 0 | 0 | 0 | ½ | ½ | 1 | 2 | The stage names are not on screen during the run: the vc-a video had to be read from the frame counter. The HUD and inset vanish in the ground stages (PVR-185). Nothing says "any button ends the run" |
| pl4nar c7 / c8 / c9 | 0 | 0 | 1 | 0 | 0 | 1 | 2 | The boot text goes to serial only (pl4nar/src/main.c:257). Stage names appear only on the results (main.c:185). Keys come from one table (controls.h). The HUD shows "PL4NAR ON/OFF", an old name. The C9 and C9T volume names led to a misfiled video (PVR-188). Junior's e148dd3 (DRIVE and TECH builds, a key line) has changed it since |
| hero1 | 0 | 0 | 0 | 0 | ½ | 1 | 1.5 | "hero1: loading the hero ...". The live row is "OPQ DET VIS SHD". The key line also shows during the autorun, when the keys do not apply (hero/hero_spike.c:624). There is no MEASURING line and no verdict |

Across the programs:
- Rule 6 passes everywhere.
- Rule 1 fails everywhere except pvr-dma.
- Only the suite menu and pl4nar print their keys from one table.

## Fixes, cheapest first (shared work first)

1. **Put the stage name on screen during the autorun.** Each table already exists: lamp `auto_stage`, hero1 `stages`, vc `vc_script`, pl4nar's three stage tables, and ao4's 19 stages, which need names written first. It is one `text_set` line a program. This would have saved the reading of vc-a and the C9T confusion. Owners: Senior for ao4, lamp and hero1; Junior for pl4nar and the town.
2. **Add an intro screen from the suite's own "what" and "read" text.** Show the suite_s8.json row for 4 s before the run, through one helper in `psm_suite.h`. Add "hands off for N s" when the run must not be touched. One helper serves every program. Owner: Senior.
3. **Put the verdict first on each result page.** Move the lines the programs already have to the top: lamp :760, d1's T1 RESULT and DERIVED lines, the vc status line (psm_viewcost.h:238). Relabel d1's fixed look lines "LOOK FOR (not measured):". Write one plain verdict for ao4, hero1 and corr. Owners: Senior, Junior.
4. **During a measured stage, the key line says only "MEASURING stage k of n: any button = live view".** hero1 lacks it, and the vc disc should say that any button ends the run. Owner: Senior.
5. **Use plain names on screen.** "PL4NAR ON" becomes "PLANAR REFLECTION ON". Page titles such as "AO ao4" become "Ambient occlusion (ao4)". The suite rows keep their code name but add a plain title. Owners: Junior for pl4nar, Senior for the suite.
6. **Give each disc a volume name that cannot be confused with another**, for example PL4NAR-C9-HILLS against PL4NAR-C9-TILES4. Owner: whoever builds the disc.
7. **Make the label hide the same key everywhere** (hens uses LEFT + B). Owner: Senior.
8. **Move each program to one key table** (the `nav_keys` or `ctl_rows` pattern, with a host test each): ao4, lamp, hens, d1, hero1, corr, the town. This is the most work. Owners: Senior for the suite programs, Junior for the town.
9. **Town: send the HUD first** (vc-b1 and vc-b2 test this), so a full vertex buffer cannot drop it (PVR-185). Owner: Junior.

## Status

ADVISED. The scores are Scholar's reading of the code and videos, not a measurement. Recheck each program's row after its fix runs on a console.

## Related

- [Demo readability](demo-readability.md): the standard
- [Names](../effects/names.md): the plain names
- [PVR ledger](../hardware/ledger-pvr.md): PVR-185, PVR-186, PVR-188, PVR-191
