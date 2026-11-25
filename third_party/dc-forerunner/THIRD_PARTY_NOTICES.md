# Third-party notices

Nothing from the source game's disc or files is in this repository. The owner supplies the disc; tools convert it at build time.

| Dependency | How we use it | Pin | Licence | Where it lives |
|---|---|---|---|---|
| SH4ZAM (Falco Girgis and contributors) | SH-4 math: XMTRX / FIPR, fast sqrt, sincos | tag v0.10.0 | MIT | cloned by `tools/fresh_deps.py` into `third_party/sh4zam` (git-ignored) |
| SH4DER-PVR (Jilted Valkyrie) | post-processing and per-vertex light on the PVR | commit 76d12c9c5d082c042157f3dca1d8ef621f2464bc | its own LICENSE: free use in personal and commercial software; attribution required in credits / notices; source redistribution keeps the notice; not to be sold as standalone effect source or library | cloned into `third_party/sh4der-pvr` (git-ignored) |
| SH4DOW-DC (Emerald May) | soft per-vertex character self-shadow | commit bed4aa36e66cb4ec83d267fd0ccb0b24ec245e60 | its own LICENSE: same terms as SH4DER-PVR (attribution required, not to be sold as standalone shadow code or library) | cloned into `third_party/sh4dow-dc` (git-ignored) |
| KallistiOS (Simulant and contributors) | the Dreamcast SDK: PVR, AICA, maple, filesystem | v2.2.0-1050-g568c3404 | MIT (see its LICENSE.md) | external, `KOS_BASE` in `config/config.local` |
| mkdcdisc | builds the .cdi image | 2b98b0d | see its repository | external, `MKDCDISC` in `config/config.local` |
| FFmpeg | decodes the owner's source audio and video at build time (tools/build_assets.py, example) | any recent build | LGPL / GPL by build | installed tool, not shipped |
| <Reference port or reader, example line> | reading aid for how the source game loads, or a reader for the source's file format; no code copied into this repo unless named here | pin it: <commit or tag> | <its licence, e.g. GPL-3.0 or MIT> | external checkout (`SOURCE_READER` in `config/config.local`), never vendored |

Delete the example line, or replace it with your own, before you publish the port. Any code you copy from another project keeps that project's licence and credit, and gets a row here.

<Game> is a trademark of its owner. This project is not affiliated with the owner of <Game> or its publisher.
