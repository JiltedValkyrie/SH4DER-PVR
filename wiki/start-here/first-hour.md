---
title: "Your first hour: build, run one effect, see it"
tags: [start, learning]
updated: 2026-11-26
sources: [Makefile, README.md, AGENTS.md, wiki/start-here/setup.md, wiki/start-here/what-do-you-want.md, wiki/effects/names.md, wiki/glossary.md, wiki/methods/host-test-first.md, wiki/methods/benchmarking.md, wiki/guides/racing-game.md, wiki/effects/pool.md, include/sh4der_pool.h, demo/cloudvolume.c, demo/ocean.c, test/core_test.c, test/pool_test.c, tools/check_tuning.py]
---
# Your first hour: build, run one effect, see it

**What you will learn:** how to run the library's host tests, make one picture on your own computer, know what each result proves, and find the console build for when you are ready for it.
**For:** a developer who has never built SH4DER-PVR.

Do the steps in order. Each one ends with something you can check. Run the commands in a shell in the repository folder. On Windows, run them in Git Bash (our advice; this page's commands were not run here). The order matches [Setup](setup.md), which gives the reasons.

## a. What you need

- **A C compiler.** The Makefile calls `cc` by default (`Makefile:11`, `CC ?= cc`). If your compiler has another name, add `CC=<name>` to a make command.
- **GNU make.** The Makefile is written for it. [Setup](setup.md) does not say how to install it, so this wiki does not cover that step yet.
- **Python 3.** The Makefile runs `python3` (`Makefile:404`, `PYTHON ?= python3`). If your Python has another name, add `PYTHON=<name>` to a make command. The checks use nothing beyond its standard library (`Makefile:41`).
- **For the console build:** a current KallistiOS with its environment sourced, and `mkdcdisc` for the disc image (`Makefile:402` and `:405`). KallistiOS carries SH4ZAM, so you do not download a second copy ([Setup](setup.md), section 2).

Nothing runs in this step. Step b is where the checks start.

## b. Run the host tests

Preview every command before you run it. This prints the commands and runs none:

```
make -n test
```

**You should see:** one line for each command the run would execute, starting with the two Python tuning checks of `check-tuning` (`Makefile:42-44`). The line count was not checked here (NOT RUN).

Then run the tests:

```
make test
```

This builds and runs every program in `test/`, the demos' own host tests, and three tool self-tests. Two tests print `NOT RUN` instead of running when their input files are missing, and the target still passes (`Makefile:236-237`). A comment in the Makefile gives `psm_place_cmp` a time of "about 20 s" (`Makefile:204`; NOT RUN here, not timed).

**You should see:** the tuning check print its counts (the `check_tuning:` line, `tools/check_tuning.py:432`). Each test prints its own lines. `core_test` ends with `core_test: ok` (`test/core_test.c:84`), and `pool_test` prints lines starting `pool: PASS` (`test/pool_test.c:225-226`). The prompt comes back with no error line. A failed check prints `FAIL` and make stops (`test/pool_test.c:224` prints `pool_test: N FAILED` and returns 1). NOT RUN: the author of this page did not run `make test`.

## c. See one effect on the host

Build the cloud preview. It is a host program that marches a cloud deck on the CPU and writes a picture file:

```
make cloudvolume-preview
build/cloudvolume-preview build/first-cloud.ppm
```

The Makefile builds into `build/` (`Makefile:14`). It names no picture file. The name after the program is yours, and `build/first-cloud.ppm` is an example. The picture is 640 × 480 in PPM (`demo/cloudvolume.c:790-791`), an image format that many picture programs open. `make clean` removes `build/` (`Makefile:440`).

**You should see:** a line starting `cloudvolume: t` with march counters and an ESTIMATE cycle count (`demo/cloudvolume.c:1095`). The file's own comment says that count is an estimate, not a measurement (`demo/cloudvolume.c:24`). The PPM file appears at the path you gave (`build/first-cloud.ppm` in the example). NOT RUN: the page author did not run this program.

For a sea instead:

```
make ocean-preview
build/ocean-preview build/first-ocean.ppm
```

**You should see:** one line starting `preview:` that ends with `host raster, no PVR bloom or hardware timing` (`demo/ocean.c:399`). That last phrase matters. The picture is the maths run on the host, not a console.

## d. Know what you saw

- **The cloud picture:** read the top comment of `include/sh4der_cloudvolume.h` (the march), then [Cloud deck [cloudvolume]](../effects/cloudvolume.md).
- **The sea picture:** read [Water waves [water]](../effects/water.md), then `demo/OCEAN.md` for the sunset demo's rows (AGENTS.md, "Files").
- **The next effect to learn:** [Light pools [pool]](../effects/pool.md). The header's RECOMMENDED block is at the top of [sh4der_pool.h](../../include/sh4der_pool.h), lines 7 to 18, with start values, cost and status. Its call order is in AGENTS.md, "Call order: pool".
- **A whole recipe:** the [racing game guide](../guides/racing-game.md) builds one scene from several effects, with each number tagged.
- **Three effects in one scene:** [Your first game scene](first-game-scene.md).

## e. The console build, last

Read sections 3 and 4 of [Setup](setup.md) before you start. An emulator result is not evidence (`setup.md`, section 4). Both builds need KallistiOS (`kos-cc` and `kos-ar`, `Makefile:20-21`) with its environment sourced, and `make dc` first runs `tools/check_sh4zam.py` (`Makefile:350`).

Build the library:

```
make dc SH4ZAM=<the SH4ZAM folder that your KallistiOS carries>
```

Build the demo disc, one program that shows six effects as live rows (AGENTS.md, "Files", `demo/main.c`):

```
make demo SH4ZAM=<the SH4ZAM folder that your KallistiOS carries>
```

The disc image is `build/demo/sh4der-demo.cdi` (`Makefile:420-421`). The demo target needs `kos-cc`, `mkdcdisc` and python: the Makefile names each one (`DC_CC ?= kos-cc`, `Makefile:20`; `MKDCDISC ?= mkdcdisc`, `Makefile:405`; `PYTHON ?= python3`, `Makefile:404`). To build the library and the disc together: `make dc DEMO=1 SH4ZAM=...` (`Makefile:423-425`).

**You should see:** a `.cdi` file that a Dreamcast, a GDEMU or an emulator can boot (`Makefile:401-402`). The emulator shows the picture, not the timing. NOT RUN: this page's author did not build either target, and the dry run named above was not re-checked here.

## What a host result does not prove

A host picture or a host test shows the maths and the layout. It shows nothing about the console's speed, the PVR's behaviour, or how the picture looks on a television. See [Host test first](../methods/host-test-first.md), section "What a host test cannot prove", and [Benchmarking on a real console](../methods/benchmarking.md) for what counts as evidence.

## Related

- [Setup](setup.md): the same order, with the reasons.
- [What do you want to make?](what-do-you-want.md): find an effect by the picture you have in mind.
- [Effect names](../effects/names.md): the reader names and the module names.
- [Glossary](../glossary.md): PVR, TA, SH-4 and the other words used here.
