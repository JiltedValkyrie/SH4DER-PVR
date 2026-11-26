/* SH4DER-PVR: the effect labels of hero1: the tables for ../psm_effects.h

   demo/psm/hero/hero_fx.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   What hero1 shows, by the field's names, with who does it and when (../psm_effects.h has the rules). Read from
   hero_spike.c; the place is named. Plain C, so ../test/psm_effects_test.c runs every table. Include after
   ../psm_effects.h.

     Detail Map         PVR, Texture: DETAIL, the detail texture over the armour, 2 x multiply (h_detail); the SH-4 only
                        sends the strips with fixed texture coordinates.
     Sphere Map         SH-4/PVR, Realtime: VISOR, the SH-4 makes the visor's coordinates each frame from the camera in
                        bone space (hero_sphere_uv), the PVR samples the sphere map texture, added (h_visor).
     Projected Shadow   SH-4/PVR, Realtime: SHADOW, the hero slid along the sun into the 256 x 256 target, whose
                        window follows him; the ground reads it as a texture. No depth compare.
     Ambient Occlusion  SH-4, Baked: a set with a baked byte a vertex (the room had one). The space street has none:
                        the program passes room = 0 and the label is not on its pages.
   Not labelled: the vertex light (ambient + sun a vertex, the base shading) and the skinning (the program's subject, in
   its title; with it the four effects do not fit two lines). The order below is the order that fits two lines. */

#ifndef HERO_FX_H
#define HERO_FX_H

#define HERO_FX_MAX 4u

/* The live view's entries into out (room for HERO_FX_MAX). The count; 0 when nothing is shown. */
static inline unsigned hero_fx_live(int hero_on, int det, int vis, int shd, int room, fx_entry_t out[HERO_FX_MAX]) {
    unsigned n = 0;

    if (hero_on && det) { out[n].name = "Detail Map"; out[n].who = FX_PVR; out[n].when = FX_TEXTURE; out[n].detail = NULL; n++; }
    if (shd) { out[n].name = "Projected Shadow"; out[n].who = FX_BOTH; out[n].when = FX_REALTIME; out[n].detail = NULL; n++; }
    if (hero_on && vis) { out[n].name = "Sphere Map"; out[n].who = FX_BOTH; out[n].when = FX_REALTIME; out[n].detail = NULL; n++; }
    if (room) { out[n].name = "Ambient Occlusion"; out[n].who = FX_SH4; out[n].when = FX_BAKED; out[n].detail = NULL; n++; }
    return n;
}

/* The result pages: everything the run showed. Two lines: hero_spike.c defines PG_TOP 1. */
static const fx_entry_t hero_fx_page[] = {
    { "Detail Map", FX_PVR, FX_TEXTURE, NULL },
    { "Projected Shadow", FX_BOTH, FX_REALTIME, NULL },
    { "Sphere Map", FX_BOTH, FX_REALTIME, NULL },
};
#define HERO_FX_PAGE_N 3u

#endif /* HERO_FX_H */
