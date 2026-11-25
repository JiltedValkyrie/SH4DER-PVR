/* SH4DER-PVR: the effect labels of the AO programs (ao4, hens-eyes, pvr-dma): the tables for ../psm_effects.h

   demo/psm/ao/ao_fx.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   What each program shows, by the field's names, with who does it and when (../psm_effects.h has the rules). Every
   line here was read from the program's code; the place is named. Plain C, so ../test/psm_effects_test.c runs every
   table. Include after ../psm_effects.h.

   ao4 (ao4_spike.c, ao4_scene.h, ao4_draw.h). A half of the screen, by its method:
     OFF     nothing
     1       Ambient Occlusion, SH-4, Baked: a byte a vertex (at load in scenes A, B; at build in C), multiplied into
             the ambient light by the SH-4 (ao4_light)
     2       Ambient Occlusion, SH-4/PVR, Realtime: the SH-4 slides the casters along K sky directions, the PVR draws
             them into target A, the ground reads it as a texture (ao4_draw_sky)
     3       Ambient Occlusion (Lightmap), PVR, Texture: the room's flat surfaces as quads with their baked AO as a
             texture; the SH-4 does nothing for it a frame
     3+M     3, and with the hens shown Ambient Occlusion (movers), SH-4/PVR, Realtime: a tile a hen in target M
             (m_pass), a multiply quad a hen (m_quads). Occlusion from sky directions: not a sun shadow.
   Both halves, in scenes A and B unless the look is AO ONLY (ao4_draw_sun gives nothing then, and in the room):
             Projected Shadow, SH-4/PVR, Realtime: the casters laid along the sun into target S. No depth compare.

   hens-eyes (hen_spike.c): the room lit once at load with its baked vertex AO (ao4_light, AO4_BAKED), and the
   program's subject, which is not a picture effect: how a hen keeps off a wall. EYES: a fan of level rays and two
   rays down, cast on a 0.1 s tick to steer (Cortana's hen_eyes.h; the entry passes no viewer, so its line-of-sight
   ray is never cast: "steering", not "line of sight"). The SWEEP modes: the hen's circle swept along its path
   against edges and the other hens (hen_sweep.h). In every mode the per-step push and the separation also run;
   they are not the rays' or the sweep's, and the result page says so. Names confirmed by Cortana, 2026-11-25.

   pvr-dma (pd_spike.c): no effect; it draws result pages only. */

#ifndef AO_FX_H
#define AO_FX_H

#define AO_FX_AO "Ambient Occlusion"
#define AO_FX_SHADOW "Projected Shadow"

/* ao4: one half's entries into out (room for 3). method 0 .. 4 = AO4_OFF, _BAKED, _MAP, _LMAP, _LMAP_M; sun_shadow:
   the scene casts one and the look shows it; movers: the hens are shown and their quads are on. The count. */
static inline unsigned ao4_fx_half(unsigned method, int sun_shadow, int movers, fx_entry_t out[3]) {
    unsigned n = 0;

    if (method == 1u) { out[n].name = AO_FX_AO; out[n].who = FX_SH4; out[n].when = FX_BAKED; out[n].detail = NULL; n++; }
    if (method == 2u) { out[n].name = AO_FX_AO; out[n].who = FX_BOTH; out[n].when = FX_REALTIME; out[n].detail = NULL; n++; }
    if (method == 3u || method == 4u) { out[n].name = AO_FX_AO; out[n].who = FX_PVR; out[n].when = FX_TEXTURE; out[n].detail = "Lightmap"; n++; }
    if (method == 4u && movers) { out[n].name = AO_FX_AO; out[n].who = FX_BOTH; out[n].when = FX_REALTIME; out[n].detail = "movers"; n++; }
    if (sun_shadow) { out[n].name = AO_FX_SHADOW; out[n].who = FX_BOTH; out[n].when = FX_REALTIME; out[n].detail = NULL; n++; }
    return n;
}

/* ao4's result pages: everything the run showed. Two lines: ao4 defines PG_TOP 1. */
static const fx_entry_t ao4_fx_page[] = {
    { AO_FX_AO, FX_SH4, FX_BAKED, NULL },
    { AO_FX_AO, FX_PVR, FX_TEXTURE, "Lightmap" },
    { AO_FX_AO, FX_BOTH, FX_REALTIME, NULL },
    { AO_FX_AO, FX_BOTH, FX_REALTIME, "movers" },
    { AO_FX_SHADOW, FX_BOTH, FX_REALTIME, NULL },
};
#define AO4_FX_PAGE_N 5u

/* hens-eyes: mode 0 is EYES, the others sweep. Entry 0 alone is the page's picture effect; the page shows all three. */
static const fx_entry_t hen_fx_eyes[] = {
    { AO_FX_AO, FX_SH4, FX_BAKED, NULL },
    { "Ray Casting", FX_SH4, FX_REALTIME, "steering" },
};
static const fx_entry_t hen_fx_sweep[] = {
    { AO_FX_AO, FX_SH4, FX_BAKED, NULL },
    { "Collision Detection", FX_SH4, FX_REALTIME, "swept" },
};
static const fx_entry_t hen_fx_page[] = {
    { AO_FX_AO, FX_SH4, FX_BAKED, NULL },
    { "Ray Casting", FX_SH4, FX_REALTIME, NULL },
    { "Collision Detection", FX_SH4, FX_REALTIME, NULL },
};
#define HEN_FX_N 2u
#define HEN_FX_PAGE_N 3u

static const fx_entry_t pd_fx_page[] = { { "No effect: a hardware transfer test (PVR-DMA)", FX_NONE, 0, NULL } };
#define PD_FX_PAGE_N 1u

/* ao-lamp (lamp_spike.c, ao4_lamp.h): the room with its baked vertex AO (ao4_light, AO4_BAKED), and with the lamp on:
     Emission                SH-4 Realtime   the shade's own level, written a vertex each frame (ao4_lamp_shade)
     Subsurface Scattering   SH-4 Realtime   sh4der_sss_rgb a vertex into the offset colour (the PVR only adds it)
     Glow                    SH-4 Realtime   the halo's rings, levels and plane fade (ao4_lamp_halo); an additive blend
     Point Light             SH-4 Realtime   the bulb on the room's vertices (lamp_spike.c's room_light), when it is on
     Punch-Through Cutout    PVR Texture     the slots: a 1-bit texture, the alpha test of the punch-through list
   The live view shows what is on in either half (no L / R: five names with their halves do not fit two lines). */
static inline unsigned lamp_fx_live(int lamp, int slots, int light, fx_entry_t out[6]) {
    static const fx_entry_t all[6] = {
        { "Emission", FX_SH4, FX_REALTIME, NULL }, { "Subsurface Scattering", FX_SH4, FX_REALTIME, NULL }, { "Glow", FX_SH4, FX_REALTIME, NULL },
        { "Point Light", FX_SH4, FX_REALTIME, NULL }, { AO_FX_AO, FX_SH4, FX_BAKED, NULL }, { "Punch-Through Cutout", FX_PVR, FX_TEXTURE, NULL },
    };
    unsigned n = 0;

    if (lamp) { out[n++] = all[0]; out[n++] = all[1]; out[n++] = all[2]; }
    if (lamp && light) out[n++] = all[3];
    out[n++] = all[4];
    if (lamp && slots) out[n++] = all[5];
    return n;
}

/* ao-lamp's result pages: three lines, lamp_spike.c defines PG_TOP 2. */
static const fx_entry_t lamp_fx_page[] = {
    { "Emission", FX_SH4, FX_REALTIME, NULL }, { "Subsurface Scattering", FX_SH4, FX_REALTIME, NULL }, { "Glow", FX_SH4, FX_REALTIME, NULL },
    { "Point Light", FX_SH4, FX_REALTIME, NULL }, { AO_FX_AO, FX_SH4, FX_BAKED, NULL }, { "Punch-Through Cutout", FX_PVR, FX_TEXTURE, NULL },
};
#define LAMP_FX_PAGE_N 6u

#endif /* AO_FX_H */
