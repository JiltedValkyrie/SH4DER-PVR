---
title: Outside sources (skmp's repositories)
tags: [research, license, pvr]
updated: 2026-11-25
sources: [third_party/skmp, demo/psm/POLLY2_NOTES.md]
---
# Outside sources: skmp's repositories

**What you will learn:** what the three outside repositories in `third_party/skmp` are, what we may do with them, and how their results are tagged here.
**For:** anyone citing PowerVR2 behaviour from these sources, and any agent working near that folder.

`third_party/skmp` holds read-only local copies of three repositories by skmp, a Dreamcast emulator developer. Git ignores the folder (`.gitignore:10`). Nothing in it is part of SH4DER-PVR.

| Repository | Commit read | What it is | Licence |
|---|---|---|---|
| `polly2-rtl` | `53a62f5` | A model of the PowerVR2 CORE in SystemVerilog, said to match a console bit for bit. Its spec (`docs/spec/isp.md`, `tsp.md`, `numerics.md`) writes each rule in words and names the console test that pins it. | **No licence file.** Treat as all rights reserved. |
| `dreamcast-tests` | `a932980` | Console test programs, and what they found (`README.md`, `TODO.md`, `pvr-tests/`). Their results come from their console(s). | **No licence file.** Treat as all rights reserved. |
| `nulldc3` | `4e773df` | A Dreamcast emulator made to aid development, with reference software renderers (`libswirl/rend/refsw2`, `refsw-offline`). | **Mixed:** `LICENSE` is Reicast's BSD 3-clause; `LICENSE-GPL` (GPL v2) and `LICENSE-LGPL` (LGPL 2.1) cover parts of it. |

## The rules

- **Cite, never copy.** Name the file and line. No code, no tables, no constants from these repositories go into this repository or this wiki. That includes the bump unit's measured tables.
- **Tagging.**
  - A rule polly2-rtl or dreamcast-tests states, with their console test behind it, is **REPORTED** (their console, not ours).
  - An emulator's or reference renderer's output is **REPORTED, "emulator, not evidence"**.
  - Neither is ever MEASURED here. Only our own console runs are.
- **The refsw3 source was not read.** polly2-rtl transcribes it from nulldc3's `skmp/refsw3` branch. The local nulldc3 copy has `refsw2` only. Every refsw3 rule we cite comes through polly2-rtl's text.

## What they gave us

What they settled for our work, with each rule cited, is in [`demo/psm/POLLY2_NOTES.md`](../../demo/psm/POLLY2_NOTES.md):

- the bump unit's exact form;
- the punch-through test;
- the fog index;
- depth storage and compare;
- the write-out pack;
- the bilinear-only hang of non-twiddled bump maps.

The ledger entries that rest on them say so: [PVR ledger](../hardware/ledger-pvr.md), [Errata](../errata/README.md).

## What we gave them, or could

Their docs list render to texture in 1555 and 4444 as untested (`dreamcast-tests/TODO.md:93`). Our c3b run has console data for it (see the [PVR ledger](../hardware/ledger-pvr.md)).
