---
title: The SH4ZAM rule
tags: [method, sh4]
updated: 2026-11-26
sources: [AGENTS.md "Maths and speed: standing rule for every agent", third_party/sh4zam-dce/README.md]
---
# The SH4ZAM rule

**What you will learn:** why all maths goes through SH4ZAM, what to do when SH4ZAM has no call for it, and what counts as "slow".
**For:** anyone writing or changing code in this library, person or agent.

The rule is in [`AGENTS.md`](../../AGENTS.md), "Maths and speed". In plain words:

**Whose code is whose.** SH4ZAM is upstream: Falco Girgis's library, MIT licence, at [github.com/gyrovorbis/sh4zam](https://github.com/gyrovorbis/sh4zam). `third_party/sh4zam` is a plain checkout of its master branch: we never edit it, never change or pin its version, never commit it; we only `git pull`. `sh4zam-dce` (`third_party/sh4zam-dce`, names `shz_dce_*`) is **our own layer** beside it, not part of SH4ZAM: it follows SH4ZAM's style and calls SH4ZAM, and its optional override header puts SH4ZAM's names on our routines at compile time without touching SH4ZAM's files. What we measure or find about SH4ZAM (SH4-019, SH4-022) is our notes; a fix we think upstream could use is offered to its author, never applied to the checkout.

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
- Use SH4ZAM's style and naming (`shz_dce_`) and put it in our `sh4zam-dce` layer (`third_party/sh4zam-dce`), never in SH4ZAM's checkout. Call it from C.
- Keep a C reference beside it, and a host test that checks the two agree.
- Do not copy SH4ZAM's files here. Keep Falco Girgis's MIT notice with SH4ZAM.
- Offer a hook that proves useful to SH4ZAM's author; whether it goes upstream is their call.

Our hooks that exist so far are listed in `third_party/sh4zam-dce/README.md`. One example is `shz_dce_pvr_project`, which transforms and projects vertices for the PVR.

## 3. No slow code

- No float divide in a loop where a reciprocal does it (FSRRA, FIPR, FTRV).
- No libm call per vertex.
- No uncached read in a hot loop.
- No copy that the store queue or DMA can do.


Uncached and badly placed buffers are the commonest slow code here:

![The placing trap: the SH-4 operand cache is 16 KB, direct-mapped, indexed by address bits 13..5, so two arrays a multiple of 16 KB apart put line i of each in the same entry and every access misses; measured on one console, mat_transform 13.16 against 38.67 cycles a vertex (1 MB apart), pvr_project 21.15 against 132.5 (256 KB apart in Full compare chain [c4]), memcpy 398.0 against 46.6 MB/s; the fix is one pool with the second array 8 KB off modulo 16 KB (SH4-002, 003, 020-023, 030-032, 046).](../images/cache-placing-trap.svg)

## 4. Proof is a console number

An assembly kernel is kept only with a measured gain on a real console. An emulator number is not evidence. The numbers we have are in the [SH-4 ledger](../hardware/ledger-sh4.md). For example, in our h2 run (one console):

- FTRV took 16.58 cycles a point;
- FIPR took 38.10;
- plain C took 72.43;
- FIPR and FTRV gave bit-identical results.

The rule also cuts the other way. In Ray bench, run one [ray1] (one console, one run) our batch kernel `shz_dce_xmtrx_project_point3_batch` gave the right bytes and ran 2 times faster than a C loop of SH4ZAM calls, but 5.8 % slower than KallistiOS's `mat_transform` in the cache and 7.3 % out of it (SH4-050). It has no measured gain over the routine it would replace; whether it stays is the owner's call.

## Related

- [Host test first](host-test-first.md)
- [Benchmarking](benchmarking.md)
- [SH-4 ledger](../hardware/ledger-sh4.md)
