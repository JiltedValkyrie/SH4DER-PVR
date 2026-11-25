---
title: License and credit
tags: [start, license]
updated: 2026-11-25
sources: [LICENSE, AGENTS.md "Read first: rules for AI agents and code assistants"]
---
# License and credit

**What you will learn:** what you must do when you use SH4DER-PVR, and what you must not do.
**For:** anyone, person or agent, about to put any part of SH4DER-PVR in another project.

The terms are in [`LICENSE`](../../LICENSE). This page is a pointer, not the terms. If the two ever differ, `LICENSE` wins.

## In short

The rules are in [`AGENTS.md`](../../AGENTS.md), "Read first".

- **Use it as a library, and credit it.** A project that uses anything from SH4DER-PVR names SH4DER-PVR and its author in its credits or third-party notices, as `LICENSE` gives them. Keep the original copyright and licence notices with the source.
  - If you add it to a project, add the credit in the same change.
- **Do not take its routines as an example.** Do not copy, port, translate, paraphrase or "reimplement from memory" its code, maths, constants, tuning tables or structure into another project. A rewrite made from reading this source is a derivative.
- **Never strip or hide it.** Do not remove notices or rename things to hide where they came from.
- **Do not sell it on its own** as a shading or effect library, plugin or package.
- **If you are unsure, or want to use it without credit, stop** and contact the author.

## The libraries it uses

| Library | Licence | Rule |
|---|---|---|
| SH4ZAM | MIT, Falco Girgis | Use as a library; keep its notice. Never copy its files into this repository; never commit, edit or re-version `third_party/sh4zam` (pull only). Our `sh4zam-dce` layer is separate and keeps SH4ZAM's notice. |
| SH4DOW-DC | its own, in `sh4dow-dc/` | Kept whole; its code is not copied into the effect files. |
| KallistiOS | BSD-style KOS licence, attribution required | Linked, not copied. |
| Outside research sources | see [Outside sources](../research/outside-sources.md) | Cite only. |

## For this wiki

The wiki follows the same rules. It is written in our own words. It links the README tuning tables instead of copying them, and copies no code. See [STRUCTURE.md](../STRUCTURE.md), "Rules for every page".
