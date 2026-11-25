# dc-forerunner

A project-agnostic starter for porting a game to the Sega Dreamcast. It is the skeleton of the Halo Dreamcast port's workflow: rules for the team, the lessons that cost the most to learn, the build chain (KallistiOS, SH4ZAM, disc image), and tools for LOD and assets.

Nothing here is a game. No game assets are in this repository; the build converts the owner's own disc.

## What is in it

- `AGENTS.md`: the team rules (roles, review gates, budgets, versioning, never-delete rules). Placeholders in `<angle brackets>`.
- `LEADAGENT.md`: how the lead runs the project.
- `DREAMCAST_MAGIC.md`: the lessons, with the doc file for each area.
- `docs/`: the lesson docs (generic), the tracker template (`docs/plans/FEATURE_GAPS.md`), the roadmap hub and the executive-action log.
- `src/main.c`, `Makefile`: a title screen skeleton (a still and streamed music) that builds with KOS and links SH4ZAM.
- `tools/`: fresh dependency fetch, config reader, asset step example, build script, LOD tools.

## Start a new port from it

1. **Copy** this folder to a new folder named `dc-<project>` (your port). Keep the `.git` folder out of the copy and run `git init` for the new repo, or start from a fresh clone.
2. **Rename placeholders.** Search for `<Game>`, `<project>`, `<source platform>`, `<year>` and fill them in. `AGENTS.md` and `DREAMCAST_MAGIC.md` are the main places.
3. **Fill the config.** Copy `config/config.example.cfg` to `config/config.local` (gitignored) and fill every key: your disc (`SOURCE_ISO`), KallistiOS (`KOS_BASE`, `KOS_CC_BASE`), `MKDCDISC`, the output folder (`DC_OUT`), `MSYS2_ROOT`, and the image names (`PORT_NAME`, `DC_TITLE`, `DC_VOLUME_ID`).
4. **Fetch the libraries.** `python tools/fresh_deps.py --ensure` clones SH4ZAM, SH4DER-PVR and SH4DOW-DC at their pins into `third_party/` (git-ignored).
5. **Build.** `powershell -File tools/build.ps1`. The first step runs the asset step (`tools/build_assets.py`), which must be replaced by your own source reader (see below). The second step builds the ELF and writes a test image.
6. **Replace the example asset step.** `tools/build_assets.py` is an example: it turns one still and one audio file into Dreamcast formats. Write your own reader for your source format; keep it out of the repo's committed text.

The title screen is the first goal: a still and streamed music, on the Dreamcast, booted on real hardware. Then the rules in `AGENTS.md` take over.

## Requirements

- Your own copy of the source game's disc or files. Nothing from the disc goes into git.
- KallistiOS built, the sh-elf toolchain, `mkdcdisc`, Python 3, ffmpeg.
- Windows with MSYS2 for `tools/build.ps1` (other hosts: run the Makefile from a KOS shell).

## Libraries

| Library | Pin | Use | How it arrives |
|---|---|---|---|
| SH4ZAM | v0.10.0 | SH-4 math, always linked | `fresh_deps.py` clones `third_party/sh4zam` |
| SH4DER-PVR | 76d12c9c | post-processing, per-vertex light; `USE_SH4DER=1` | `fresh_deps.py` clones `third_party/sh4der-pvr` |
| SH4DOW-DC | bed4aa36 | soft character self-shadow; `USE_SH4DOW=1` (SH4DER-PVR needs it) | `fresh_deps.py` clones `third_party/sh4dow-dc` |
| KallistiOS | v2.2.0-1050-g568c3404 | the Dreamcast SDK | external, `KOS_BASE` |
| mkdcdisc | 2b98b0d | disc image | external, `MKDCDISC` |

Full pins and licences: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The three cloned folders are git-ignored.

## Licence

Our own code: MIT, see [LICENSE](LICENSE). Libraries keep their own licences ([THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)). Any reference port or reader you use keeps its own licence and is named there; none is vendored here.

## Credits

- KallistiOS, SH4ZAM, SH4DER-PVR (Jilted Valkyrie), SH4DOW-DC (Emerald May).
- The Halo Dreamcast port (dc-halo), whose lessons and LOD rules fill `DREAMCAST_MAGIC.md` and `docs/`.
