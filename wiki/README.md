---
title: SH4DER-PVR wiki
tags: [start]
updated: 2026-11-25
sources: [README.md, AGENTS.md, wiki/STRUCTURE.md]
---
# SH4DER-PVR wiki

**What you will learn:** what this wiki holds, how to read a page, and where to start.
**For:** everyone: people setting up a project with SH4DER-PVR, people porting to the Dreamcast, and agents working in this repository.

SH4DER-PVR is a library of visual effects for the Sega Dreamcast: glows, water, shadows, overlays and more, built on SH4ZAM (maths) and KallistiOS (drawing). This wiki explains it in plain words, recommends limits, and keeps a **ledger of hardware facts** measured on a real console.

The library's own `README.md` and `AGENTS.md` stay the reference for calls and tuning values. This wiki links them and does not copy them.

## Where to start

- **New here?** [What it is](start-here/what-it-is.md), then [Setup](start-here/setup.md).
- **Adding an effect to a game?** [Call orders](start-here/call-orders.md), the [effect pages](effects/README.md), and [Recommended limits](limits/recommended-limits.md).
- **Need a hardware number?** The [hardware ledger](hardware/README.md).
- **How far along is a part?** The [maturity table](status/maturity.md).
- **Something on the console disagrees with a document?** [Errata](errata/README.md).
- **Using the code in your project?** [Licence and credit](start-here/license-and-credit.md). Credit is required.

## How to read a page

Each page opens with **What you will learn** and **For** (the audience). Words such as TA, PVR-DMA or punch-through are in the [glossary](glossary.md).

Every number carries a status:

| Status | Meaning |
|---|---|
| MEASURED | Read from our own real console, with the date and the number of consoles. |
| FACT | Written in a primary document (Sega's manuals, the SH-4 manual) or in code, with the section. |
| REPORTED | Someone else's result. An emulator's result is always REPORTED, "emulator, not evidence". |
| ESTIMATE | Our arithmetic or model. Never mixed with MEASURED in one row. |
| DISPUTED | Two sources disagree. |
| OPEN | Not known yet; the page names the test that would settle it. |

Limits are **PROVEN** (a MEASURED number backs them) or **ADVISED** (not yet). So far, every MEASURED number comes from **one console and one run**.

## The tree

- **start-here/**: [what it is](start-here/what-it-is.md), [setup](start-here/setup.md), [call orders](start-here/call-orders.md), [licence and credit](start-here/license-and-credit.md)
- **effects/**: [one page for each of the 34 effects](effects/README.md)
- **limits/**: [budgets](limits/budgets.md), [recommended limits](limits/recommended-limits.md), [scenes](limits/scenes.md)
- **status/**: [maturity](status/maturity.md)
- **hardware/**: [the ledger](hardware/README.md): [PVR](hardware/ledger-pvr.md), [SH-4](hardware/ledger-sh4.md), [buses and DMA](hardware/ledger-bus-dma.md), [AICA](hardware/ledger-aica.md), [video](hardware/ledger-video.md)
- **errata/**: [conflicts and corrections](errata/README.md)
- **methods/**: [benchmarking](methods/benchmarking.md), [host test first](methods/host-test-first.md), [the SH4ZAM rule](methods/sh4zam-rule.md), [the fault catcher](methods/fault-catcher.md), [suite 13](methods/suite-13.md) ([suite 12](methods/suite-12.md), superseded)
- **shadows/**: [overview](shadows/README.md), [PSM routes](shadows/psm-routes.md), [bump compare chain](shadows/bump-compare-chain.md), [the thesis](shadows/thesis.md)
- **research/**: [research notes](research/README.md), [outside sources](research/outside-sources.md)
- [glossary](glossary.md), [open questions](open-questions.md), [INDEX](INDEX.md) (one line a page), [llms.txt](llms.txt) (for agents), [STRUCTURE](STRUCTURE.md) (how the wiki is built and kept)

## Keeping it right

- After every console run, the agent who owns the run writes each finding into the ledger (see [hardware/README.md](hardware/README.md)).
- No game or engine names in the wiki. Say "we know of no earlier use", never "first".
- Do not copy code or tables from `third_party/skmp` (no licence). Do not copy the library's source or tuning tables here; link them.
