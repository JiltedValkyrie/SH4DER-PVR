/* controls.h against the hand-written pad read it replaced (main.c before the table), every pad state, one build a
 * compile (-DPL4NAR_MAP, -DPL4NAR_MAP -DPL4NAR_HILLS, or neither; each with or without -DPL4NAR_DRIVE): the tech set is
 * the old read exactly; DRIVE's drive set is the restart (A) and the overlay (START) alone. The HUD's key lines fit.
 * Bits an older build set but never acted on (tiles, AO and the labels on the street) are left out of the comparison,
 * and so are the keys taken out since (X reflection, B + X split, B + Y shadow): they must map to nothing now. B + Y is
 * the HUD hide now, X the heat and B + X the fly-by: each checked on its own. */
#include <stdio.h>
#include "controls.h"

static uint32_t old_map(uint32_t p) {                /* main.c's read_pad, as it was */
    uint32_t now = 0u;
    const int b = (p & CTL_B) != 0u;
    if (p & CTL_X)     now |= b ? DEMO_BTN_SPLIT : DEMO_BTN_REFLECT;
    if (p & CTL_Y)     now |= b ? DEMO_BTN_TILES : DEMO_BTN_TOD;
    if (p & CTL_A)     now |= b ? DEMO_BTN_SIZE : DEMO_BTN_LOOK;
    if (p & CTL_UP)    now |= b ? DEMO_BTN_LAMP_UP : DEMO_BTN_MORE;
    if (p & CTL_DOWN)  now |= b ? DEMO_BTN_LAMP_DN : DEMO_BTN_LESS;
    if ((p & CTL_LEFT) && b) now |= DEMO_BTN_AO;
    if ((p & CTL_RIGHT) && b) now |= DEMO_BTN_SUN;
    if (p & CTL_START) now |= b ? DEMO_BTN_FX : DEMO_BTN_INFO;
    return now;
}

#define GONE (DEMO_BTN_REFLECT | DEMO_BTN_SPLIT | DEMO_BTN_TILES)   /* reflection and shadow always on live: no key */
#define NEW  (DEMO_BTN_HEAT | DEMO_BTN_HIDE | DEMO_BTN_CINE)        /* keys the old read did not have: checked on their own */
#ifdef PL4NAR_WET
#define HEAT_X 0u                                  /* no heat in the rain build */
#else
#define HEAT_X DEMO_BTN_HEAT                       /* X alone: the heat, in both key sets */
#endif
#ifdef PL4NAR_WET
#define CINE_BX 0u                                 /* no fly-by in the rain build (no heat) */
#else
#define CINE_BX DEMO_BTN_CINE                      /* B + X: the fly-by, in both key sets */
#endif

int main(int argc, char **argv) {
    (void)argc; (void)argv;
#if defined(PL4NAR_WET)                            /* Y is the rain there: checked below */
    const uint32_t acted = ~(uint32_t)(DEMO_BTN_TILES | DEMO_BTN_AO | DEMO_BTN_FX | DEMO_BTN_TOD | DEMO_BTN_RAIN);
#elif defined(PL4NAR_MAP)
    const uint32_t acted = ~0u;
#else
    const uint32_t acted = ~(uint32_t)(DEMO_BTN_TILES | DEMO_BTN_AO | DEMO_BTN_FX);
#endif
    uint32_t p, bad = 0u;
    unsigned from = 0u, lines = 0u;
    char t[96];
    for (p = 0; p < 512u; p++) {
        const uint32_t tech = ctl_map(p, ctl_when(1)), drive = ctl_map(p, CTL_W_DRIVE);
        const uint32_t want = ((p & CTL_A) && !(p & CTL_B) ? DEMO_BTN_RESTART : 0u) | ((p & CTL_START) && !(p & CTL_B) ? DEMO_BTN_INFO : 0u)
                            | ((p & CTL_X) && !(p & CTL_B) ? HEAT_X : 0u) | ((p & CTL_Y) && (p & CTL_B) ? DEMO_BTN_HIDE : 0u)
                            | ((p & CTL_X) && (p & CTL_B) ? CINE_BX : 0u);
        const uint32_t heat = (p & CTL_X) && !(p & CTL_B) ? HEAT_X : 0u;
        if (tech & GONE) { if (bad++ < 8u) printf("pad %03x: a removed key still maps (%05x)\n", (unsigned)p, (unsigned)(tech & GONE)); }
        if ((tech & DEMO_BTN_HIDE) != ((p & CTL_Y) && (p & CTL_B) ? DEMO_BTN_HIDE : 0u)) { if (bad++ < 8u) printf("pad %03x: hide wrong\n", (unsigned)p); }
        if ((tech & DEMO_BTN_CINE) != ((p & CTL_X) && (p & CTL_B) ? CINE_BX : 0u)) { if (bad++ < 8u) printf("pad %03x: fly-by wrong\n", (unsigned)p); }
        if ((tech & DEMO_BTN_HEAT) != heat) { if (bad++ < 8u) printf("pad %03x: heat %05x, want %05x\n", (unsigned)p, (unsigned)(tech & DEMO_BTN_HEAT), (unsigned)heat); }
        if ((tech & acted & ~(uint32_t)(GONE | NEW)) != (old_map(p) & acted & ~(uint32_t)(GONE | NEW))) { if (bad++ < 8u) printf("pad %03x: tech %05x, old %05x\n", (unsigned)p, (unsigned)tech, (unsigned)old_map(p)); }
        if (drive != want) { if (bad++ < 8u) printf("pad %03x: drive %05x, want %05x\n", (unsigned)p, (unsigned)drive, (unsigned)want); }
    }
#ifdef PL4NAR_WET
    if (ctl_map(CTL_Y, CTL_W_TECH) != DEMO_BTN_RAIN) { printf("the wet road's Y is not the rain\n"); bad++; }
#endif
#ifndef PL4NAR_DRIVE
    if (ctl_when(0) != CTL_W_TECH) { printf("the tech keys must be live always outside DRIVE\n"); bad++; }
#else
    if (ctl_when(0) != CTL_W_DRIVE || ctl_when(1) != CTL_W_TECH) { printf("DRIVE's key sets swapped\n"); bad++; }
#endif
    while (ctl_hud_line(t, 76u, &from, CTL_W_TECH)) { printf("hud: %s\n", t); lines++; if (strlen(t) > 75u) bad++; }
    if (lines > 3u) { printf("more than three HUD key lines\n"); bad++; }
    from = 0u;
    if (!ctl_hud_line(t, 76u, &from, CTL_W_DRIVE)) bad++;
    printf("drive hud: %s\n", t);
    if (ctl_hud_line(t, 76u, &from, CTL_W_DRIVE)) { printf("DRIVE's keys take more than one line\n"); bad++; }
    printf(bad ? "CONTROLS_TEST FAIL (%u)\n" : "CONTROLS_TEST_OK build %u\n", bad ? (unsigned)bad : (unsigned)CTL_BUILD);
    return bad != 0u;
}
