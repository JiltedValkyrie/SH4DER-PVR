---
title: Host test first
tags: [method, test]
updated: 2026-11-26
sources: [README.md "Adding an effect", README.md build section, AGENTS.md, third_party/sh4zam-dce/KERNEL_GUIDE.md, third_party/sh4zam-dce/README.md]
---
# Host test first

**What you will learn:** how code is checked on a PC before it reaches a console, and what a host test can and cannot prove.
**For:** anyone changing the library's maths or writing an assembly hook.

## The three builds

| Command | What it runs | Needs |
|---|---|---|
| `make test` | every `test/*_test.c` and `example/blade.c` on the host, plain C | nothing extra |
| `make test-shz SH4ZAM=<checkout>` | the same tests on the SH4ZAM path, through SH4ZAM's software back end | SH4ZAM 0.10 or later in `third_party/sh4zam` |
| `make dc SH4ZAM=<checkout>` | the Dreamcast library, `build/libsh4der_core.a` | the KallistiOS environment |

(README:1270-1273.) `tools/check_sh4zam.py` runs first for `test-shz` and `dc`, and fails with the clone line when SH4ZAM is missing. See [setup](../start-here/setup.md).

On the host, SH4ZAM's software back end gives the same numbers as the plain C path (`AGENTS.md`, pl4nar "Checks").

## Adding an effect

From README:1262-1265:

1. A header and a source file.
2. `test/<name>_test.c`: the defaults, bad parameters, and both paths.
3. The Makefile's source, header and test lists.
4. A row in the README's effect table, and a tuning table. A test checks that every tuning macro is in a table with the same value.

## Assembly hooks (the sh4zam-dce layer)

`sh4zam-dce` is our layer beside SH4ZAM, not part of it; SH4ZAM itself (upstream, Falco Girgis, MIT) is never edited ([the SH4ZAM rule](sh4zam-rule.md)).

From `third_party/sh4zam-dce/KERNEL_GUIDE.md`:93-107:

- Each batch routine has three forms: `_c` (plain C on SH4ZAM's public calls, the reference), `_sh4` (the assembly) and the plain name (checks alignment, then dispatches).
- A host test compares against an exact reference in double or integers. A bit-identical routine is compared byte for byte over a large sweep. A routine that is not bit-identical prints how often it differs and shows every difference is inside the bound.
- The SH-4 assembly also runs instruction by instruction in an interpreter on the host. It stops on an unaligned load or store, a clobbered callee-saved register, or a cache line allocated and not filled.
- The report per routine says: what it does, its alignment, its error bound, bit-identical or not, cycles before and after, where the gain comes from, what was tried and did not pay, and every claim that could not be checked on the host.

## What a host test cannot prove

- **Speed.** Host cycle numbers are model estimates for comparing two routines, not console times (sh4zam-dce/README.md:27, 31). An assembly kernel is kept only with a measured gain on a console ([the SH4ZAM rule](sh4zam-rule.md)).
- **FIPR and FTRV rounding.** They do not round like IEEE; a host-against-console check needs a tolerance. On our console FIPR and FTRV gave the same bits as each other, and were nearer the exact sum than plain C (MEASURED, h2, one console, 2026-11-25; see the [SH-4 ledger](../hardware/ledger-sh4.md)).
- **The matrix load and the render target.** A console is the only test of these (`AGENTS.md`, pl4nar "Checks").
- **Hardware faults** an emulator forgives: zero-area triangles, uncompiled headers, unaligned access. See [Send checks](../effects/guard.md).

Files say "Not run on a console when this file was written" until a console has run them (sh4der_fault.h:62; bench_h1.c:21).

## Related

- [Benchmarking](benchmarking.md), [the SH4ZAM rule](sh4zam-rule.md), [setup](../start-here/setup.md)
