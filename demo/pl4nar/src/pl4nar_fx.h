/* SH4DER-PVR: the effect labels of the pl4nar street demo: the tables for ../../psm/psm_effects.h

   demo/pl4nar/src/pl4nar_fx.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   What the new map (PL4NAR_MAP) shows, by the field's names, with who does it and when (psm_effects.h has the rules).
   Read from demo.c; the place is named. Plain C, so ../../psm/test/psm_effects_test.c runs every table. Include after
   psm_effects.h.

     Planar Reflection  SH-4/PVR, Realtime: the PVR renders the mirrored world into the target each frame
                        (gfx_reflect_begin .. end), the SH-4 looks each paint and glass vertex up in it with a Fresnel
                        alpha (draw_player: sh4der_pl4nar_shade), the layer is drawn over the body.
     Projected Shadow   SH-4/PVR, Realtime: the cars' caster hulls, slid along the sun by the SH-4, drawn into the
                        target's right half (draw_tiles), laid on the road multiplied (draw_tile_quads). No depth
                        compare. By day with the reflection on.
     Blob Shadow        SH-4, Realtime: a dark disc fan under a car with no tile (blob: sh4der_pool_disc).
     Ambient Occlusion  SH-4, Baked: a byte a model vertex in the ambient (draw_model) and the road's multiplied patches
                        (draw_road_ao), both baked at build (tools/map_ao.c).
     Light Pool         SH-4, Realtime: the lamps' and headlights' additive fans on the road (draw_lamps 0,
                        draw_car_lights 0). At night.
   Not labelled: the halos round the lamps (a camera-facing glow quad: with them the night's entries do not fit two
   lines), the vertex light (ambient + sun a vertex, the base shading), the distance haze (an offset colour), the sky.
   The live view and the split both use fx_format_merged (names that share who and when in one group): the split's
   difference is the reflection alone, and its side labels ("PL4NAR ON" / "OFF") say which half has it. Tiles are by
   day only, so tiles and night never come together. */

#ifndef PL4NAR_FX_H
#define PL4NAR_FX_H

#define PL4NAR_FX_MAX 5u

static inline unsigned pl4nar_fx_add_(fx_entry_t *out, unsigned n, const char *name, unsigned who, unsigned when) {
    out[n].name = name; out[n].who = (uint8_t)who; out[n].when = (uint8_t)when; out[n].detail = NULL;
    return n + 1u;
}

/* The live view's entries into out (room for PL4NAR_FX_MAX). reflect: the layer drawn on this side; tiles: car tiles
   this frame (any; the hills build: 1 on the ground mesh, 2 the flat quad); blobs: a blob disc this frame (any); ao: the baked occlusion on; night: the lamps lit. */
static inline unsigned pl4nar_fx_live(int reflect, int tiles, int blobs, int ao, int night, fx_entry_t out[PL4NAR_FX_MAX]) {
    unsigned n = 0;

    if (reflect) n = pl4nar_fx_add_(out, n, "Planar Reflection", FX_BOTH, FX_REALTIME);
#ifdef PL4NAR_HILLS                                 /* the hills build: tiles 1 the projected shadow laid on the ground mesh, 2 the
                                                       typical flat quad at the car's ground height (B+Y) */
    if (tiles == 2) n = pl4nar_fx_add_(out, n, "Flat Shadow Quad", FX_BOTH, FX_REALTIME);
    else if (tiles) n = pl4nar_fx_add_(out, n, "Projected Shadow on Mesh", FX_BOTH, FX_REALTIME);   /* the page's long name does not fit two lines */
#else
    if (tiles) n = pl4nar_fx_add_(out, n, "Projected Shadow", FX_BOTH, FX_REALTIME);
#endif
    if (blobs) n = pl4nar_fx_add_(out, n, "Blob Shadow", FX_SH4, FX_REALTIME);
    if (ao) n = pl4nar_fx_add_(out, n, "Ambient Occlusion", FX_SH4, FX_BAKED);
    if (night) n = pl4nar_fx_add_(out, n, "Light Pool", FX_SH4, FX_REALTIME);
    return n;
}

/* The result pages: everything the run showed. Three lines: main.c defines PG_TOP 2. */
static const fx_entry_t pl4nar_fx_page[] = {
    { "Planar Reflection", FX_BOTH, FX_REALTIME, NULL },
#ifdef PL4NAR_HILLS
    { "Projected Shadow (on the ground mesh)", FX_BOTH, FX_REALTIME, NULL },
    { "Flat Shadow Quad", FX_BOTH, FX_REALTIME, NULL },
#else
    { "Projected Shadow", FX_BOTH, FX_REALTIME, NULL },
#endif
    { "Blob Shadow", FX_SH4, FX_REALTIME, NULL },
    { "Ambient Occlusion", FX_SH4, FX_BAKED, NULL },
    { "Light Pool", FX_SH4, FX_REALTIME, NULL },
};
#ifdef PL4NAR_HILLS
#define PL4NAR_FX_PAGE_N 6u
#else
#define PL4NAR_FX_PAGE_N 5u
#endif

#endif /* PL4NAR_FX_H */
