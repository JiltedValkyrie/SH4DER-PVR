---
title: Shadows on the Dreamcast
tags: [shadows]
updated: 2026-11-25
sources: [demo/psm/, sh4dow-dc/README.md, demo/psm/PRIOR_ART.md]
---
# Shadows on the Dreamcast

**What you will learn:** the shadow methods this project has, which are ready, which are research, and where each one is written up.
**For:** a programmer choosing a shadow method, and anyone following the shadow research.

The PVR has no way to write its depth buffer to memory (PVR-083). So a depth [shadow map](../glossary.md) must be made another way: drawn by the SH-4, or drawn by the PVR as a colour. That is what most of this section is about.

## What is ready

| Method | What it shades | Status | Page |
|---|---|---|---|
| sh4dow-dc | Soft shadows on characters, from limb "columns" cast along the light, per vertex. Foot cones. A heightfield sun module (experimental). | Shipped in a released game; console times REPORTED by that game's author | [sh4dow-dc](../effects/sh4dow-dc.md) |
| Blob shadow | A soft dark disc under a car or a character | Header only, host tested | [pool](../effects/pool.md) |
| Modifier volumes | The usual Dreamcast method (hardware shadow volumes) | Exact in our c1 test (PVR-044); not used as the method here | — |

## What is research

![The c4 chain: a map drawn from the sun (3.2-3.3 ms), a mask pass where the bump unit compares each receiver band against the map and the 1555 write keeps one bit (4.8-10.2 ms by band width), and one screen layer that darkens by the mask (6.5 ms, the difference of two measured screen passes); one console, 2026-11-25, ledger PVR-028, 045, 090, 091, 094-101.](../images/c4-chain.svg)

| Method | Idea | Status | Page |
|---|---|---|---|
| PSM, routes A, B and AB | A shadow map for a directional light, uniform or perspective-warped. A: the PVR draws a silhouette. B: the SH-4 draws depth. AB: the PVR draws depth as a colour code, PVR-DMA copies it back. | Headers host tested; town demo on a console (one view) | [PSM routes](psm-routes.md) |
| Bump compare chain | Use the PVR's bump unit as a per-pixel subtract, to compare depth on the PVR | c1, c3b, c4 run (one console) | [Bump compare chain](bump-compare-chain.md) |
| Cached sun visibility | One byte of sun visibility per static vertex, baked at load and re-sliced as the sun moves | Design document only | [The thesis](thesis.md) |

## Prior art, in safe words

We found no documented commercial Dreamcast game that used a depth shadow map. Render-to-texture shadows are reported for a few titles; most games used modifier volumes (`demo/psm/PRIOR_ART.md:85-89`). Never claim a first.

## Related

- [PVR ledger](../hardware/ledger-pvr.md)
- [Scenes](../limits/scenes.md)
- [Recommended limits](../limits/recommended-limits.md), "Shadows"
