---
title: What SH4DER-PVR is
tags: [start, overview]
updated: 2026-11-25
sources: [README.md, AGENTS.md, LICENSE]
---
# What SH4DER-PVR is

**What you will learn:** what the library does, what it needs, and where each part lives.
**For:** anyone opening this repository for the first time, person or agent.

SH4DER-PVR is a library of visual effects for the Sega Dreamcast. The Dreamcast has no programmable shaders, so each effect is built from what the hardware does have:

- the **SH-4** main processor, with its fast vector maths;
- the **PowerVR2** graphics chip (the "PVR"): a tile renderer with fixed blending, fog and bump units.

An effect here is a small amount of maths, run per frame or per vertex, plus a way of drawing that the PVR can do cheaply.

The library is licensed work, not a free sample. Before using any of it, read [License and credit](license-and-credit.md).

## The parts

| Part | Where | What it is |
|---|---|---|
| The effects | `include/sh4der_*.h`, `src/sh4der_*.c` | One header (and often one `.c`) per effect. Headers are portable and host-tested; the `.c` files are the drawing code for the Dreamcast. See [the effect pages](../effects/). |
| Shared core | `include/sh4der_core.h`, `src/sh4der_core.c` | Maths helpers, the soft edge ("feather"), shapes, the breath tracker and the glint. |
| SH4DOW-DC | `sh4dow-dc/` | The soft shadow library, included whole. Its own page: [sh4dow-dc](../effects/sh4dow-dc.md). |
| Tests | `test/*.c`, `make test` | Host tests for every header. |
| Demos | `demo/` | The demo disc, the ocean, the street reflection demo, the torch walk, the shadow research programs in `demo/psm/`. |

## What it depends on

- **SH4ZAM** (MIT, Falco Girgis): the maths library for the SH-4, version 0.10 or later. Every square root, sine, reciprocal and matrix step goes through it on the Dreamcast. It is not shipped here: see [Setup](setup.md).
- **KallistiOS** (KOS): the Dreamcast operating system and drawing library the `.c` files call.
- **SH4DOW-DC**: included in `sh4dow-dc/`, kept whole.

Without SH4ZAM, `SH4DER_CORE_NO_SH4ZAM` compiles the same steps as plain C. Host tools and tests use that.

## Where to go next

- [Setup](setup.md): build, test, fetch SH4ZAM.
- [Call orders](call-orders.md): how each effect is called, linked to `AGENTS.md`.
- [Budgets](../limits/budgets.md) and [Recommended limits](../limits/recommended-limits.md): what fits in a frame.
- [Hardware ledger](../hardware/README.md): what we know about the console, and how we know it.
- [Glossary](../glossary.md): every term used on these pages.
