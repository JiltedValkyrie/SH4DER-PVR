---
title: Setup
tags: [start, build, test]
updated: 2026-11-25
sources: [AGENTS.md "Files", Makefile, tools/check_sh4zam.py]
---
# Setup

**What you will learn:** how to get SH4ZAM, run the host tests and build for the Dreamcast.
**For:** a programmer building the library for the first time.

Work in this order:

1. Host tests.
2. The same tests on SH4ZAM's path.
3. The Dreamcast build.
4. A real console.

The reasons are in [Host test first](../methods/host-test-first.md).

## 1. Host tests (no SH4ZAM needed)

```
make test
```

This builds every `test/*_test.c` and `example/blade.c` with plain C (`SH4DER_CORE_NO_SH4ZAM`) and runs them.

## 2. SH4ZAM

SH4ZAM is not shipped. Fetch version 0.10 into `third_party/sh4zam`:

```
git clone --branch v0.10.0 https://github.com/gyrovorbis/sh4zam third_party/sh4zam
```

- Git ignores that folder: never commit it.
- `make check-sh4zam` (which runs `tools/check_sh4zam.py`) fails, and prints that line, when the checkout is missing, incomplete or older than 0.10.
- Then run the tests on SH4ZAM's path:

  ```
  make test-shz SH4ZAM=third_party/sh4zam
  ```

## 3. The Dreamcast library

```
make dc SH4ZAM=third_party/sh4zam
```

Every target also takes `SH4DOW=` for the SH4DOW-DC location.

The demos have their own targets, all listed in [`AGENTS.md`](../../AGENTS.md), "Files":

- `make demo`
- `make ocean`
- `make torchwalk-ab`
- `make pl4nar-demo`

## 4. A real console

- An emulator result is not evidence (see [Benchmarking](../methods/benchmarking.md)).
- Disc images for the shadow research go in `demo/psm/out/cdi`. Never overwrite one: each build gets a new tag.
- How to build for a Dreamcast at all: the toolchain, KOS flags and lessons are in `third_party/dc-forerunner/DREAMCAST_MAGIC.md` (summarised in [Benchmarking](../methods/benchmarking.md)).

## Related

- [What it is](what-it-is.md)
- [The SH4ZAM rule](../methods/sh4zam-rule.md)
