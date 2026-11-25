---
title: The SH4ZAM rule
tags: [method, sh4]
updated: 2026-11-25
sources: [AGENTS.md "Maths and speed: standing rule for every agent", third_party/sh4zam-dce/README.md]
---
# The SH4ZAM rule

**What you will learn:** why all maths goes through SH4ZAM, what to do when SH4ZAM has no call for it, and what counts as "slow".
**For:** anyone writing or changing code in this library, person or agent.

The rule is in [`AGENTS.md`](../../AGENTS.md), "Maths and speed". In plain words:

## 1. SH4ZAM everywhere on the Dreamcast path

On the Dreamcast path, these maths steps go through SH4ZAM's `shz_*` calls, never libm or hand-written C:

- square roots, sines and cosines;
- reciprocals;
- dot and cross products;
- matrix and vector steps.

SH4ZAM uses the SH-4's own instructions for them:

- **FSRRA**: a fast reciprocal square root.
- **FSCA**: sine and cosine.
- **FIPR**: a four-term dot product.
- **FTRV**: a 4 × 4 matrix times a vector.

Plain C stays only as the `SH4DER_CORE_NO_SH4ZAM` path, for the host.

## 2. When SH4ZAM has no call for it, write a hook

- Write a thin SH-4 assembly (or inline assembly) kernel in our own file.
- Use SH4ZAM's style and naming (`shz_`-like) and put it in the `sh4zam-dce` layer (`third_party/sh4zam-dce`). Call it from C.
- Keep a C reference beside it, and a host test that checks the two agree.
- Do not copy SH4ZAM's files here. Keep Falco Girgis's MIT notice with SH4ZAM.
- Offer a hook that proves useful back to SH4ZAM.

The hooks that exist so far are listed in `third_party/sh4zam-dce/README.md`. One example is `shz_dce_pvr_project`, which transforms and projects vertices for the PVR.

## 3. No slow code

- No float divide in a loop where a reciprocal does it (FSRRA, FIPR, FTRV).
- No libm call per vertex.
- No uncached read in a hot loop.
- No copy that the store queue or DMA can do.

## 4. Proof is a console number

An assembly kernel is kept only with a measured gain on a real console. An emulator number is not evidence. The numbers we have are in the [SH-4 ledger](../hardware/ledger-sh4.md). For example, in our h2 run (one console):

- FTRV took 16.58 cycles a point;
- FIPR took 38.10;
- plain C took 72.43;
- FIPR and FTRV gave bit-identical results.

## Related

- [Host test first](host-test-first.md)
- [Benchmarking](benchmarking.md)
- [SH-4 ledger](../hardware/ledger-sh4.md)
