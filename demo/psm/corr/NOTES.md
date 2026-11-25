# corr: the texture filtering corridor (notes)

corr-a is a standalone test program: an endless corridor that shows the PVR's texture filters where they are weakest, a floor and walls seen edge on far off. Each mode can be shown alone or beside another on a split screen. It has a cost table for each mode, and a count of the polygons that pile up on one tile.

Nothing in this folder has run on a console yet. Every statement about the filters' look, the PVR's mip choice and the trilinear passes is OPEN until a console run is read off the screen. Only a real Dreamcast is evidence; an emulator is not.

## Files

| File | What it is |
| --- | --- |
| `corr_spike.c` | The program: the autorun (11 stages), the result pages and the live view. Dreamcast only. |
| `corr_scene.h` | The corridor: segment kinds, the ring of segments, the eye, the near clip, the projection and the bin counter. Plain C, host tested. |
| `corr_modes.h` | The filter modes, the bias selector (over `../fakeaf.h`), guard (a), the ramp hook and the effect labels. Plain C, host tested. |
| `tools/corr_bake.py` | The bake: the tiles scaled up, with mip chains, RGB565, twiddled. Writes a header into the build folder. `--self-test` checks the layout. |
| `tools/corr_preview.c` | A host picture of the layout. Nearest sampling, top level only: it cannot show any filter mode. |
| `test/corr_test.c` | The host test (see the comment at its top for the command line). |
| `build.sh` | Builds the ELF into a new folder: checks credits, bakes, then `kos-cc`. |
| `fetch_table.json`, `CREDITS.md`, `licenses/` | The art (Retro Textures Fantasy, Kenney, CC0). It is not in the repository. Fetch it with `python demo/pl4nar/tools/fetch.py --root demo/psm/corr`, and check it with `check_credits.py --root demo/psm/corr`. |

## The modes

| Mode | What is sent |
| --- | --- |
| nearest | `PVR_FILTER_NEAREST`, mip mapped. |
| bilinear | `PVR_FILTER_BILINEAR`, mip mapped. |
| trilinear | Two passes of the same vertices. Pass A: `PVR_FILTER_TRILINEAR1`, opaque list. Pass B: `PVR_FILTER_TRILINEAR2`, translucent list, blend ONE x ONE, depth GEQUAL, no depth write. Source: System Architecture 3.4.7.2.3. KallistiOS only names the two passes. |
| bias | Bilinear, with each polygon's mip D adjust taken from `faf_bias_code(ndv, strength, least)`, or one fixed code for all. Polygons are batched by code: one header for each code that has polygons. |
| ramp | A hook only. `corr_ramp_draw` returns 0 and the half is drawn as bilinear. The method waits for mip-a. |

## Hardware claims this program tests (ledger form)

| Claim | Status | Source | Test |
| --- | --- | --- | --- |
| Trilinear = pass A (opaque list) + pass B (translucent list, ONE x ONE) over the same vertices. | FACT (document) | System Architecture 3.4.7.2.3 | corr-a stage 3: no seam between levels, and the picture not darker or brighter than bilinear. |
| The header's mip bias (D adjust) acts per polygon. | OPEN | `fakeaf.h` | corr-a stages 4-6, 8. |
| CORE 26: the 8th filtered opaque polygon in a tile can hang the core. Reported for non-twiddled bump textures. | REPORTED (skmp; errata E3), PVR-071 | `third_party/skmp` model | corr-a2: "most in one bin" per stage, with the far cap, and the first design's count on one frame a stage. The host count of the same polygon list on the autorun path (`test/corr_test.c`): 58 boxes on one bin with the cap, 383 without, both at the vanishing point. These are box upper bounds. The first run is for information. |

## The far cap (corr-a2)

A segment that starts 16 m (`CORR_CAP`) or more ahead is drawn as its far kind: one quad for each surface, not one for each 1 m cell. The texture repeats stay the same, because uv is in metres. On the autorun path (host count) this gives:

| | First design | Far cap |
| --- | --- | --- |
| Boxes on one bin (most) | 383 | 58 |
| Triangles a frame (most) | 1,536 | 634 |

58 is still over 7: the vanishing point gets about 11 far segments times 5 quads. Getting under 7 there would take one long quad for each surface over the whole far range, and few or no far props.

What the cap costs: on the far quads the light is baked at the four corners only, and the near cells' edge vertices meet the first far segment as T-junctions at 16 m.

## The suite entry (for Lead; the suite disc is Lead's)

The entry for the next suite table, in the DEVELOPER TOOLS group after d1:

```json
{
  "name": "corr",
  "file": "CORR.BIN",
  "elf": "<the corr-a2 ELF>",
  "order": 2,
  "what": "texture filters down an endless corridor. ~95 s",
  "read": "photo every page + graph; START = live: X/Y modes",
  "group": "tools",
  "credits": "corr"
}
```

`"credits": "corr"` needs two more things:

- a `"corr"` key in `suite/build.py`'s `CREDITS`, whose switch is `PSM_CREDITS_CORR`;
- a page in `psm_credits.h` (Senior's file), for example: "ART IN THE CORRIDOR TEST / All of it is CC0 1.0 ... / Kenney (kenney.nl): Retro Textures Fantasy. / The corridor, its torches and pillars are this repository's own work."

The orders of hens-eyes and the tools after it move up by one.

## Open after corr-a

- corr-b: Senior's RANGES 3V mode (stretched copies, cut quads, a translucent zone pass). v already runs along the corridor on every surface, so it needs no layout change.
