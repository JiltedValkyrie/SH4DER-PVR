/* The demo's controls, one table: the pad read (main.c read_pad through ctl_map), the HUD's key lines (demo.c through
 * ctl_hud_line) and the README's Controls table (tools/check_controls.py reads the CTL_ROW lines below and the README's
 * table and fails when they differ) all come from it. A control that is not a row here does not exist.
 * One CTL_ROW a line: pad keys (CTL_*, B included when it is held), the action bit (DEMO_BTN_*, 0: read as an axis in
 * read_pad), the builds it is in (CTL_B_*), the keys and the action as the README says them, the HUD's short form
 * (NULL: not on the HUD). The driving rows (steer, gas, brake) are the autorun's own: there its driver holds them. */
#ifndef PL4NAR_CONTROLS_H
#define PL4NAR_CONTROLS_H
#include <stdint.h>
#include <string.h>
#include "demo.h"

enum {                                     /* the pad, as read_pad gives it (KallistiOS CONT_* there) */
    CTL_A = 1u << 0, CTL_B = 1u << 1, CTL_X = 1u << 2, CTL_Y = 1u << 3, CTL_START = 1u << 4,
    CTL_UP = 1u << 5, CTL_DOWN = 1u << 6, CTL_LEFT = 1u << 7, CTL_RIGHT = 1u << 8,
    CTL_STICK = 1u << 9, CTL_LT = 1u << 10, CTL_RT = 1u << 11       /* axes: only named, never matched */
};
#define CTL_B_STREET 1u                    /* the first street (no PL4NAR_MAP) */
#define CTL_B_MAP    2u                    /* the map, flat */
#define CTL_B_HILLS  4u                    /* the map with PL4NAR_HILLS */
#define CTL_B_ALL    7u
#if defined(PL4NAR_HILLS)
#define CTL_BUILD CTL_B_HILLS
#elif defined(PL4NAR_MAP)
#define CTL_BUILD CTL_B_MAP
#else
#define CTL_BUILD CTL_B_STREET
#endif

typedef struct ctl_row { uint32_t pad, btn, builds; const char *keys, *action, *hud; } ctl_row_t;
#define CTL_ROW(pad, btn, builds, keys, action, hud) { pad, btn, builds, keys, action, hud },
#define CTL_AXIS 0u
static const ctl_row_t ctl_rows[] = {
CTL_ROW(CTL_STICK | CTL_LEFT | CTL_RIGHT, CTL_AXIS, CTL_B_ALL, "stick or D-pad left / right", "steer", "stick steer")
CTL_ROW(CTL_RT, CTL_AXIS, CTL_B_ALL, "RT", "engine", "RT gas")
CTL_ROW(CTL_LT, CTL_AXIS, CTL_B_ALL, "LT", "brake, then reverse", "LT brake")
CTL_ROW(CTL_X, DEMO_BTN_REFLECT, CTL_B_ALL, "X", "reflection on / off", "X reflect")
CTL_ROW(CTL_Y, DEMO_BTN_TOD, CTL_B_ALL, "Y", "time of day: AUTO (a 45 second day), DAY, NIGHT, PAUSE", "Y time")
CTL_ROW(CTL_A, DEMO_BTN_LOOK, CTL_B_ALL, "A", "textures: low (64 x 64, 16 colours, the start) or full", "A look")
CTL_ROW(CTL_UP, DEMO_BTN_MORE, CTL_B_ALL, "D-pad up", "F0 (the reflection's strength) up", "UP/DOWN F0")
CTL_ROW(CTL_DOWN, DEMO_BTN_LESS, CTL_B_ALL, "D-pad down", "F0 down", NULL)
CTL_ROW(CTL_START, DEMO_BTN_INFO, CTL_B_ALL, "START", "the overlay (times, triangles, memory, the target as a thumbnail) on / off; off at the start on the hills", "START info")
CTL_ROW(CTL_B | CTL_STICK, CTL_AXIS, CTL_B_ALL, "B + stick", "the camera round the car", "B+stick camera")
CTL_ROW(CTL_B | CTL_LT | CTL_RT, CTL_AXIS, CTL_B_ALL, "B + LT / RT", "move the split's divider (with the split on)", NULL)
CTL_ROW(CTL_B | CTL_X, DEMO_BTN_SPLIT, CTL_B_ALL, "B + X", "the split: the reflection on the left of a divider only", "B+X split")
CTL_ROW(CTL_B | CTL_A, DEMO_BTN_SIZE, CTL_B_ALL, "B + A", "the target's size: 256 x 128, 128 x 64, 512 x 256 in turn (the start: 256 x 128; the hills: 128 x 64)", "B+A size")
CTL_ROW(CTL_B | CTL_UP, DEMO_BTN_LAMP_UP, CTL_B_ALL, "B + D-pad up", "the reflection's night gain up", "B+UP/DOWN night gain")
CTL_ROW(CTL_B | CTL_DOWN, DEMO_BTN_LAMP_DN, CTL_B_ALL, "B + D-pad down", "the night gain down", NULL)
CTL_ROW(CTL_B | CTL_RIGHT, DEMO_BTN_SUN, CTL_B_ALL, "B + D-pad right", "the sun: FIXED, SWEEP (to DAY unless NIGHT), HOLD", "B+RIGHT sun")
CTL_ROW(CTL_B | CTL_LEFT, DEMO_BTN_AO, CTL_B_MAP | CTL_B_HILLS, "B + D-pad left", "baked occlusion on / off", "B+LEFT AO")
CTL_ROW(CTL_B | CTL_Y, DEMO_BTN_TILES, CTL_B_MAP, "B + Y", "car shadow tiles on / off (off: blob discs)", "B+Y shadow")
CTL_ROW(CTL_B | CTL_Y, DEMO_BTN_TILES, CTL_B_HILLS, "B + Y", "car shadow: on the ground mesh, one flat quad, off (blob discs) in turn", "B+Y shadow")
CTL_ROW(CTL_B | CTL_START, DEMO_BTN_FX, CTL_B_MAP | CTL_B_HILLS, "B + START", "the effect labels shown / hidden", "B+START labels")
};
#define CTL_ROWS (sizeof ctl_rows / sizeof ctl_rows[0])

/* The pad's keys held (CTL_*) to the action bits held (DEMO_BTN_*) in this build: a row's keys all held, and B held
   exactly when the row names it. */
static inline uint32_t ctl_map(uint32_t pad) {
    uint32_t now = 0u;
    unsigned i;
    for (i = 0; i < CTL_ROWS; i++) {
        const ctl_row_t *r = &ctl_rows[i];
        if (r->btn && (r->builds & CTL_BUILD) && (pad & r->pad) == r->pad && ((pad & CTL_B) != 0u) == ((r->pad & CTL_B) != 0u)) now |= r->btn;
    }
    return now;
}

/* The HUD's key lines: this build's short forms, two spaces apart, as many as fit in max - 1 characters, from row
   *from on; *from moves past them. 0: none left. */
static inline int ctl_hud_line(char *out, size_t max, unsigned *from) {
    size_t o = 0;
    out[0] = 0;
    for (; *from < CTL_ROWS; (*from)++) {
        const ctl_row_t *r = &ctl_rows[*from];
        size_t n;
        if (!r->hud || !(r->builds & CTL_BUILD)) continue;
        n = strlen(r->hud);
        if (o + (o ? 2u : 0u) + n + 1u > max) break;
        if (o) { out[o++] = ' '; out[o++] = ' '; }
        memcpy(out + o, r->hud, n + 1u);
        o += n;
    }
    return o != 0u;
}
#endif
