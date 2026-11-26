/* The demo's controls, one table: the pad read (main.c read_pad through ctl_map), the HUD's key lines (demo.c through
 * ctl_hud_line) and the README's Controls table (tools/check_controls.py reads the CTL_ROW lines below and the README's
 * table and fails when they differ) all come from it. A control that is not a row here does not exist.
 * One CTL_ROW a line: pad keys (CTL_*, B included when it is held), the action bit (DEMO_BTN_*, 0: read as an axis in
 * read_pad), the builds it is in (CTL_B_*), when it is live (CTL_W_*), the keys and the action as the README says them,
 * the HUD's short form (NULL: not on the HUD). The driving rows (steer, gas, brake) are the autorun's own: there its
 * driver holds them.
 * When: DRIVE (PL4NAR_DRIVE, the game program) has two key sets: the drive keys while the overlay is closed, the tech
 * keys while START has it open. Every other program (the live view, the autorun) has the tech keys always.
 * No key turns the reflection or the car's shadow off, and no key shows the split: live, both are always on; the
 * comparisons (reflection off, the split, blob or flat shadows) are the autorun's stages (demo_auto_*). */
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
#define CTL_B_WET    8u                    /* the street with PL4NAR_WET (the wet road one-off) */
#define CTL_B_ALL    15u
#define CTL_W_DRIVE  1u                    /* DRIVE with the overlay closed */
#define CTL_W_TECH   2u                    /* DRIVE with the overlay open; every other program always */
#define CTL_W_ALL    3u
#if defined(PL4NAR_DRIVE) && defined(PL4NAR_AUTORUN)
#error "PL4NAR_DRIVE is the game program, PL4NAR_AUTORUN the measuring one: one of them"
#endif
#if defined(PL4NAR_WET)
#define CTL_BUILD CTL_B_WET
#elif defined(PL4NAR_HILLS)
#define CTL_BUILD CTL_B_HILLS
#elif defined(PL4NAR_MAP)
#define CTL_BUILD CTL_B_MAP
#else
#define CTL_BUILD CTL_B_STREET
#endif

typedef struct ctl_row { uint32_t pad, btn, builds, when; const char *keys, *action, *hud; } ctl_row_t;
#define CTL_ROW(pad, btn, builds, when, keys, action, hud) { pad, btn, builds, when, keys, action, hud },
#define CTL_AXIS 0u
static const ctl_row_t ctl_rows[] = {
CTL_ROW(CTL_STICK | CTL_LEFT | CTL_RIGHT, CTL_AXIS, CTL_B_ALL, CTL_W_ALL, "stick or D-pad left / right", "steer", NULL)
CTL_ROW(CTL_RT, CTL_AXIS, CTL_B_ALL, CTL_W_ALL, "RT", "engine", "RT gas")
CTL_ROW(CTL_LT, CTL_AXIS, CTL_B_ALL, CTL_W_ALL, "LT", "brake, then reverse", "LT brake")
CTL_ROW(CTL_X, DEMO_BTN_HEAT, CTL_B_STREET | CTL_B_MAP | CTL_B_HILLS, CTL_W_ALL, "X", "heat haze over the road's far end: OFF (the start), LIGHT, STRONG; by day and dry only, and drawn only while the camera is still (else the HUD says 'heat: waiting for a still camera')", "X heat")
CTL_ROW(CTL_Y, DEMO_BTN_TOD, CTL_B_STREET | CTL_B_MAP | CTL_B_HILLS, CTL_W_TECH, "Y", "time of day: AUTO (a 45 second day), DAY, NIGHT, PAUSE", "Y time")
CTL_ROW(CTL_Y, DEMO_BTN_RAIN, CTL_B_WET, CTL_W_TECH, "Y", "the rain: DRY, DAMP, WET in turn (night always)", "Y rain")
CTL_ROW(CTL_A, DEMO_BTN_LOOK, CTL_B_ALL, CTL_W_TECH, "A", "textures: low (64 x 64, 16 colours, the start) or full", "A look")
CTL_ROW(CTL_A, DEMO_BTN_RESTART, CTL_B_ALL, CTL_W_DRIVE, "A", "restart: the car at the start, stopped; the clock and the laps from 0", "A reset")
CTL_ROW(CTL_UP, DEMO_BTN_MORE, CTL_B_ALL, CTL_W_TECH, "D-pad up", "F0 (the reflection's strength) up", "UP/DOWN F0")
CTL_ROW(CTL_DOWN, DEMO_BTN_LESS, CTL_B_ALL, CTL_W_TECH, "D-pad down", "F0 down", NULL)
CTL_ROW(CTL_START, DEMO_BTN_INFO, CTL_B_ALL, CTL_W_ALL, "START", "the overlay (times, triangles, memory, the target as a thumbnail) on / off; off at the start on the hills and in DRIVE", "START info")
CTL_ROW(CTL_B | CTL_STICK, CTL_AXIS, CTL_B_ALL, CTL_W_ALL, "B + stick", "the camera round the car", "B+stk cam")
CTL_ROW(CTL_B | CTL_X, DEMO_BTN_CINE, CTL_B_STREET | CTL_B_MAP | CTL_B_HILLS, CTL_W_ALL, "B + X", "the fly-by: a fixed camera low at the kerb ahead, the car sweeps past through the heat (on in the shot), then the follow camera again", "B+X fly")
CTL_ROW(CTL_B | CTL_Y, DEMO_BTN_HIDE, CTL_B_ALL, CTL_W_ALL, "B + Y", "the whole HUD hidden (the picture alone) / shown; START also shows it again", "B+Y hide")
CTL_ROW(CTL_B | CTL_A, DEMO_BTN_SIZE, CTL_B_WET, CTL_W_TECH, "B + A", "the target's size: 128 x 64 (the start), 64 x 32, 256 x 128 in turn", "B+A size")
CTL_ROW(CTL_B | CTL_A, DEMO_BTN_SIZE, CTL_B_STREET | CTL_B_MAP | CTL_B_HILLS, CTL_W_TECH, "B + A", "the target's size: 256 x 128, 128 x 64, 512 x 256 in turn (the start: 256 x 128; the hills: 128 x 64)", "B+A size")
CTL_ROW(CTL_B | CTL_UP, DEMO_BTN_LAMP_UP, CTL_B_ALL, CTL_W_TECH, "B + D-pad up", "the reflection's night gain up", "B+UP/DOWN night gain")
CTL_ROW(CTL_B | CTL_DOWN, DEMO_BTN_LAMP_DN, CTL_B_ALL, CTL_W_TECH, "B + D-pad down", "the night gain down", NULL)
CTL_ROW(CTL_B | CTL_RIGHT, DEMO_BTN_SUN, CTL_B_ALL, CTL_W_TECH, "B + D-pad right", "the sun: FIXED, SWEEP (to DAY unless NIGHT), HOLD", "B+RIGHT sun")
CTL_ROW(CTL_B | CTL_LEFT, DEMO_BTN_AO, CTL_B_MAP | CTL_B_HILLS, CTL_W_TECH, "B + D-pad left", "baked occlusion on / off", "B+LEFT AO")
CTL_ROW(CTL_B | CTL_START, DEMO_BTN_FX, CTL_B_MAP | CTL_B_HILLS, CTL_W_TECH, "B + START", "the effect labels shown / hidden", "B+START labels")
};
#define CTL_ROWS (sizeof ctl_rows / sizeof ctl_rows[0])

/* The key set live now: CTL_W_DRIVE or CTL_W_TECH (info: the overlay is open). */
static inline uint32_t ctl_when(int info) {
#ifdef PL4NAR_DRIVE
    return info ? CTL_W_TECH : CTL_W_DRIVE;
#else
    (void)info;
    return CTL_W_TECH;
#endif
}

/* The pad's keys held (CTL_*) to the action bits held (DEMO_BTN_*) in this build and key set: a row's keys all held,
   and B held exactly when the row names it. */
static inline uint32_t ctl_map(uint32_t pad, uint32_t when) {
    uint32_t now = 0u;
    unsigned i;
    for (i = 0; i < CTL_ROWS; i++) {
        const ctl_row_t *r = &ctl_rows[i];
        if (r->btn && (r->builds & CTL_BUILD) && (r->when & when) && (pad & r->pad) == r->pad && ((pad & CTL_B) != 0u) == ((r->pad & CTL_B) != 0u)) now |= r->btn;
    }
    return now;
}

/* The HUD's key lines: this build's and key set's short forms, two spaces apart, as many as fit in max - 1 characters,
   from row *from on; *from moves past them. 0: none left. */
static inline int ctl_hud_line(char *out, size_t max, unsigned *from, uint32_t when) {
    size_t o = 0;
    out[0] = 0;
    for (; *from < CTL_ROWS; (*from)++) {
        const ctl_row_t *r = &ctl_rows[*from];
        size_t n;
        if (!r->hud || !(r->builds & CTL_BUILD) || !(r->when & when)) continue;
        n = strlen(r->hud);
        if (o + (o ? 2u : 0u) + n + 1u > max) break;
        if (o) { out[o++] = ' '; out[o++] = ' '; }
        memcpy(out + o, r->hud, n + 1u);
        o += n;
    }
    return o != 0u;
}
#endif
