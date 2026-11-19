# sh4_fpu_approx.h (third party, tests only)

Bit-exact models of the SH7091 / SH7750 (SH-4) vector and approximation FPU instructions: FIPR, FTRV, FSCA, FSRRA. One C99 header, no dependencies.

- Author: Stefanos Kornilios Mitsis Poiitidis (skmp). Copyright © 2026. MIT License: see `LICENSE` in this folder; the header carries the same notice.
- Source: https://gist.github.com/skmp/4bd7b38149fc455978b5104201573236 (file `sh4_fpu_approx.h`, revision a61dd6de0304c631b77915352e9d4dba48f2262d), unmodified. SHA-256 `cc95d6a3ede9ab57b6f332fcec5dacc97cda16515699dcdcd4bcf4978c869d2f`.
- Its author checked every model against a real Dreamcast (the header's comments say on what, with 0 mismatches). That check is his; nothing here repeats it on hardware.

What it is used for: the host tests of SH4ZAM-DCE. The SH-4 interpreter (`tests/sh4run`) calls these models for the four instructions, so a test run sees the console's own bits instead of plain sums. `tests/run_checks.py` finds this folder by itself; `--hw-model off` runs without it.

What it is NOT used for: nothing of this file is in the SH4ZAM-DCE headers (`include/sh4zam_dce`) or in anything built for the Dreamcast. It is test equipment.
