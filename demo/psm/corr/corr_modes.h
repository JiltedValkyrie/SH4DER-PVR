/* SH4DER-PVR: corr-a's filter modes, the bias selector, the header guard, the ramp hook and the effect labels

   demo/psm/corr/corr_modes.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   THE MODES (each half of a split has its own):
     0 NEAREST    PVR_FILTER_NEAREST, mip mapped
     1 BILINEAR   PVR_FILTER_BILINEAR, mip mapped
     2 TRILINEAR  two passes, as the hardware manual (System Architecture 3.4.7.2.3) and KallistiOS give it:
                  pass A: PVR_FILTER_TRILINEAR1 in the opaque list, the opaque list's blend ONE x ZERO;
                  pass B: PVR_FILTER_TRILINEAR2 in the translucent list, ONE x ONE, depth GEQUAL, no depth write,
                  the same vertices. KallistiOS's pvr_header.h names the two passes and says nothing more.
     3 BIAS       bilinear, mip mapped, each polygon's mip bias (the header's D adjust, 0.25 .. 3.75) from the
                  selector: by its slope to the eye (../fakeaf.h's faf_bias_code) or one fixed code for all
     4 RAMP       a hook for the ramp blend (a sharp pass, a destination-alpha ramp, the blurred copy by DESTALPHA):
                  not written (corr_ramp_draw returns 0), skipped in the cycle. Senior writes it after mip-a.
   Every statement about what the PVR does in these modes is OPEN until a console run.

   THE GUARD (a): a header with a filter other than point sampling is refused on a texture that is not twiddled or
   is a bump map (CORE 26, PVR-071, REPORTED skmp; errata E3). All of corr-a's textures are twiddled and mip mapped,
   so nothing should be refused: the count is on the page.

   Plain C, no KallistiOS. */

#ifndef CORR_MODES_H
#define CORR_MODES_H

#include <stdint.h>
#include <stddef.h>

#include "../fakeaf.h"

enum { CORR_M_NEAREST, CORR_M_BILINEAR, CORR_M_TRILINEAR, CORR_M_BIAS, CORR_M_RAMP, CORR_MODES };

static inline const char *corr_mode_name(unsigned m) {
    static const char *const n[CORR_MODES] = { "nearest", "bilinear", "trilinear", "bias", "ramp (hook)" };

    return m < CORR_MODES ? n[m] : NULL;
}

/* The next mode in the live cycle: the ramp hook is not in it. */
static inline unsigned corr_mode_next(unsigned m) { return (m + 1u) % CORR_M_RAMP; }

/* The ramp blend's hook. It draws nothing yet and returns 0; the program then draws that half as BILINEAR and the
   page says so. */
struct corr_ramp_ctx;
static inline int corr_ramp_draw(struct corr_ramp_ctx *c) { (void)c; return 0; }

/* ---- the bias selector --------------------------------------------------------------------------------------------- */

typedef struct corr_sel {
    const char *name;
    float strength;                          /* faf_bias_code's strength (faf clamps it to 0 .. 1) */
    uint8_t least;                           /* faf_bias_code's least code, 1 .. 4 */
    uint8_t fixed;                           /* 1 .. 15: one code for every polygon; 0: by slope */
} corr_sel_t;

enum { CORR_SEL_SLOPE05_L2, CORR_SEL_SLOPE1_L2, CORR_SEL_SLOPE1_L1, CORR_SEL_FIXED025, CORR_SEL_FIXED050 = CORR_SEL_FIXED025 + 1, CORR_SEL_N = CORR_SEL_FIXED025 + 15 };

static inline const corr_sel_t *corr_sel(unsigned k) {
    static corr_sel_t t[CORR_SEL_N];
    static const char *const fixed_name[15] = { "fixed 0.25", "fixed 0.50", "fixed 0.75", "fixed 1.00", "fixed 1.25", "fixed 1.50", "fixed 1.75",
        "fixed 2.00", "fixed 2.25", "fixed 2.50", "fixed 2.75", "fixed 3.00", "fixed 3.25", "fixed 3.50", "fixed 3.75" };
    static int made;
    unsigned i;

    if (!made) {
        t[0].name = "slope 0.5 l2"; t[0].strength = 0.5f; t[0].least = 2u; t[0].fixed = 0u;
        t[1].name = "slope 1.0 l2"; t[1].strength = 1.0f; t[1].least = 2u; t[1].fixed = 0u;
        t[2].name = "slope 1.0 l1"; t[2].strength = 1.0f; t[2].least = 1u; t[2].fixed = 0u;
        for (i = 0; i < 15u; i++) { t[3u + i].name = fixed_name[i]; t[3u + i].strength = 0.0f; t[3u + i].least = 1u; t[3u + i].fixed = (uint8_t)(i + 1u); }
        made = 1;
    }
    return &t[k < CORR_SEL_N ? k : 0u];
}

/* A polygon's bias code under selector s; ndv is |normal . direction to the eye|. */
static inline unsigned corr_sel_code(const corr_sel_t *s, float ndv) { return s->fixed ? s->fixed : faf_bias_code(ndv, s->strength, s->least); }

/* ---- the guard (a) ------------------------------------------------------------------------------------------------- */

#define CORR_FMT_TWIDDLED    1u
#define CORR_FMT_NONTWIDDLED 2u
#define CORR_FMT_MIPMAP      4u
#define CORR_FMT_BUMP        8u

/* 1: the header may be made; 0: refused (a filtered sample of a non-twiddled or bump texture). */
static inline int corr_filter_ok(unsigned fmt, int filtered) {
    if (!filtered) return 1;
    return (fmt & CORR_FMT_TWIDDLED) && !(fmt & (CORR_FMT_NONTWIDDLED | CORR_FMT_BUMP));
}

/* ---- the effect labels (../psm_effects.h; include it first) -------------------------------------------------------- */

#ifdef PSM_EFFECTS_H
#define CORR_FX_MAX 5u

/* The live view's labels of one mode (bias: by slope or fixed) into out. The count. */
static inline unsigned corr_fx_mode(unsigned mode, int fixed, fx_entry_t out[CORR_FX_MAX]) {
    unsigned n = 0;

    if (mode == CORR_M_RAMP) mode = CORR_M_BILINEAR;          /* the hook draws bilinear */
    out[n].name = mode == CORR_M_NEAREST ? "Point Sampling" : mode == CORR_M_TRILINEAR ? "Trilinear Filtering" : "Bilinear Filtering";
    out[n].who = FX_PVR; out[n].when = FX_REALTIME; out[n].detail = NULL; n++;
    if (mode == CORR_M_BIAS) {
        out[n].name = "Mip Bias"; out[n].detail = fixed ? "fixed" : "by slope";
        out[n].who = fixed ? FX_PVR : FX_SH4; out[n].when = FX_REALTIME; n++;
    }
    out[n].name = "Mipmapping"; out[n].who = FX_PVR; out[n].when = FX_TEXTURE; out[n].detail = NULL; n++;
    return n;                                                  /* the light (baked) and the fog: on the result pages only */
}

/* The live view's two lines: the split form (L, R), or the merged form when that does not fit two lines (three groups
   do not: psm_effects.h's fallback, with no L / R). 1 when it fits. */
static inline int corr_fx_live(fx_text_t *t, unsigned lm, int lfixed, int split, unsigned rm, int rfixed) {
    fx_entry_t l[CORR_FX_MAX], r[CORR_FX_MAX], both[2u * CORR_FX_MAX];
    const unsigned nl = corr_fx_mode(lm, lfixed, l);
    unsigned nr, i;

    if (!split) return fx_format(t, l, nl);
    nr = corr_fx_mode(rm, rfixed, r);
    if (fx_format_split(t, l, nl, r, nr)) return 1;
    for (i = 0; i < nl; i++) both[i] = l[i];
    for (i = 0; i < nr; i++) both[nl + i] = r[i];
    return fx_format_merged(t, both, nl + nr);
}

/* The result pages: everything the run showed. Three lines: corr_spike.c defines PG_TOP 2. */
static const fx_entry_t corr_fx_page[] = {         /* the three filters under one name: a page has no room for each */
    { "Texture Filtering", FX_PVR, FX_REALTIME, NULL },
    { "Mip Bias", FX_SH4, FX_REALTIME, "by slope" },
    { "Mip Bias", FX_PVR, FX_REALTIME, "fixed" },
    { "Mipmapping", FX_PVR, FX_TEXTURE, NULL },
    { "Vertex Lighting", FX_SH4, FX_BAKED, NULL },
    { "Distance Fog", FX_PVR, FX_REALTIME, NULL },
};
#define CORR_FX_PAGE_N 6u
#endif

#endif /* CORR_MODES_H */
