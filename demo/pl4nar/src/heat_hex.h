/* heat, the hexagon form, the street demo's glue: the library's hexagon lattice (include/sh4der_hexheat.h) given this
 * demo's camera and road each frame, and the view before (for a turn and the camera's travel). The maths is the
 * library's; this file is what only this demo has: where the haze stands and how near the band's picture may come.
 *
 * RECOMMENDED (every number an ESTIMATE)
 * Cost:       the band: 256 x 128 texels in the reflection target, 64 KB (street), 128 KB (map), 0 (hills: a spare
 *             region); the far world sent a second time (COUNTED on the host: README "Heat haze, hexagons"); the
 *             hexagons: 131 a frame on the start's straight, 4 triangles each (COUNTED on the host, the three layouts;
 *             113 to 132 over the hills by the road's shape)
 * Start with: the haze sheet HEAT_DIST = 70 m ahead; HEXH_NEAR_BACK = 6 m: the band holds what is farther than
 *             HEAT_DIST - 6 = 64 m along the view, and the hexagons stand at that depth
 * Status:     ADVISED: host tested (host/heat_hex_test.c).
 * Maturity:   PROTOTYPE v0.1
 *
 * Include after heat.h. The band's pass and the draw are demo.c's (hex_band_pass, hex_draw). */
#ifndef PL4NAR_HEAT_HEX_H
#define PL4NAR_HEAT_HEX_H
#include "heat.h"
#include "sh4der_hexheat.h"

#define HEXH_NEAR_BACK 6.0f        /* the band holds what is farther than HEAT_DIST less this, metres */

typedef struct hexh {
    sh4der_hexheat_t h;
    unsigned frame;                /* views since the gate opened */
    float pg[3], pe[3];            /* the view before: where the camera looked along the road (unit), where it stood */
} hexh_t;

static inline void hexh_reset(hexh_t *x) { memset(x, 0, sizeof *x); }

/* A frame's camera for the screen-to-world step: the demo's view (eye, at, the world's up +y, the vertical field of
   view, the screen), the road flat at height road_y, the haze sheet HEAT_DIST ahead (heat_anchor's point is its
   foot), the near cut HEXH_NEAR_BACK short of it. 0: no view; nothing is drawn. */
static inline int hexh_camera(sh4der_hexheat_view_t *v, const float eye[3], const float at[3], float fov, float sw, float sh, float road_y) {
    static const float up[3] = { 0.0f, 1.0f, 0.0f };
    return sh4der_hexheat_view_look(v, eye, at, up, fov, sw, sh) && sh4der_hexheat_view_ground(v, up, road_y, HEAT_DIST, HEAT_DIST - HEXH_NEAR_BACK);
}

/* A frame's start, its view made: the camera's turn and its travel since the view before go into the field's scroll
   (sh4der_hexheat_follow), so the pattern stays on the far picture in a turn and streams toward the camera as the car
   drives. The hexagons stay on the screen, so in a turn the pattern runs through them: an offset then changes about
   1 px a frame at 0.5 rad/s (host); nothing fades or holds it (the owner's call). The first view after a reset adds
   nothing. */
static inline void hexh_view(hexh_t *x, const sh4der_hexheat_view_t *v) {
    if (x->frame) sh4der_hexheat_follow(&x->h, v, x->pg, x->pe);
    memcpy(x->pg, v->g, sizeof x->pg); memcpy(x->pe, v->eye, sizeof x->pe);
    x->frame++;
}

#endif
