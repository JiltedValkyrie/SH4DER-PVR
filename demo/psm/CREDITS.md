# demo/psm credits

Third-party art the software shadow demo uses. None of it is in this repository: `tools/fetch.py` downloads it, checks the sha256 in `fetch_table.json` and keeps it in `assets/_dl` (ignored by git).

| File | Author | Licence | Source |
|---|---|---|---|
| `spot.obj` (Spot the cow) | Keenan Crane | CC0 1.0 Universal ("Model and associated data is dedicated under CC0 1.0 Universal.") | https://www.cs.cmu.edu/~kmcrane/Projects/ModelRepository/ , fetched from the copy at https://github.com/alecjacobson/common-3d-test-models |
| `modular_village_collection.zip` (Modular Village: the town's walls, roofs, arch, well, ladder, lamps, sign, stall canopies, pen, barrels) | Keith at Fertile Soil Productions | CC0 1.0 (as stated on the page) | https://opengameart.org/content/modular-village |
| `kloofendal_48d_partly_cloudy_puresky_2k.hdr` (the town's sky band) | Greg Zaal (original), Jarod Guest (sky edits), Poly Haven | CC0 1.0 (as stated on the page) | https://polyhaven.com/a/kloofendal_48d_partly_cloudy_puresky |

The ground and the poles are made by the demo's own code, and so is the field's plain sky; so are the town's windows, doors, crates, sacks, people, hens, hills and treeline. The town's sky band is `demo/psm/tools/bake_sky.py`'s cut of the HDR above (tone-mapped, 512 x 256, RGB565), made in the build folder and never committed.

The settings panel (START; `psm_panel_*` in `psm_scene.h`, `psm_send_panel` in `psm_send.h`) takes its layout, navigation and colours from the owner's own Dreamcast menu code (Jilted Valkyrie), reused with her permission and written again here for this demo's row table and 8 x 16 text: the panel down the left, title, tabs switched with L / R, rows with "< VALUE >" at the right and a bar on the chosen one, hairlines, scrollbar, hints, the hold-repeat delay (400 ms; the rate after it stays this demo's own) and the colour values. None of that project's fonts, icons, banners or logos is used: the text is KallistiOS's 8 x 16 minifont (`minifont_draw_str`, KallistiOS licence), drawn into the readout's existing text texture.

The town's kit pieces are cut in the build folder by `third_party/dc-forerunner/tools/lod/qem_decimate.py` (the halo-64 port's decimator, CC0), imported where it is; the cut meshes are build output, never committed.

The technique is from "Perspective Shadow Maps: Care and Feeding", GPU Gems (NVIDIA, 2004), chapter 14. The equations in `include/sh4der_psm.h` were checked against the chapter's printed text (https://download.nvidia.com/developer/GPU_Gems_2/CD/Resources/PSMs_Care_and_Feeding.pdf) and written in this library's own code and row form, not copied: see its header for where it differs.
