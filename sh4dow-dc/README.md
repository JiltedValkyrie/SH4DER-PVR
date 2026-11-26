# SH4DOW-DC

SH4DOW-DC (soft character shadows: limb columns, foot cones, the heightfield sun) is folded into SH4DER-PVR, by the same author, on 2026-11-26. This folder no longer builds anything.

- The code: `../include/sh4dow_dc.h`, `../include/sh4dow_heightfield.h`, `../src/sh4dow_dc.c`, `../src/sh4dow_foot.c`.
- The tests and examples: `../test/shade_test.c`, `../test/foot_test.c`, `../example/`, `../bench/`. Build and run them with the root `Makefile` (`make test`).
- How to call it: `../AGENTS.md`, "Call order: sh4dow". The guide for people: `../wiki/effects/sh4dow.md`.
- The licence text SH4DOW-DC came with stays here, unchanged: [LICENSE](LICENSE). The terms are in `../LICENSE`.

`include/sh4dow_dc.h` and `include/sh4dow_heightfield.h` in this folder are forwards to the library headers, kept for builds that still pass `-Ish4dow-dc/include`.

The old README of this folder is in the git history (before commit b71ef08 and in it).
