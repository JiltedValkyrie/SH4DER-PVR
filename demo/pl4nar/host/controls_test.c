/* controls.h against the hand-written pad read it replaced (main.c before the table), every pad state, one build a
 * compile (-DPL4NAR_MAP, -DPL4NAR_MAP -DPL4NAR_HILLS, or neither; each with or without -DPL4NAR_DRIVE): the tech set is
 * the old read exactly; DRIVE's drive set is the restart (A) and the overlay (START) alone. The HUD's key lines fit.
 * Bits an older build set but never acted on (tiles, AO and the labels on the street) are left out of the comparison. */
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

int main(void) {
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
        const uint32_t want = ((p & CTL_A) && !(p & CTL_B) ? DEMO_BTN_RESTART : 0u) | ((p & CTL_START) && !(p & CTL_B) ? DEMO_BTN_INFO : 0u);
        if ((tech & acted) != (old_map(p) & acted)) { if (bad++ < 8u) printf("pad %03x: tech %05x, old %05x\n", (unsigned)p, (unsigned)tech, (unsigned)old_map(p)); }
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
