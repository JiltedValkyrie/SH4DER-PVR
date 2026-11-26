/* heat, the hexagon form, the street demo's glue: the library's hexagons (include/sh4der_hexheat.h) fixed to the far
 * picture through heat.h's slip, and the demo's own choices. The maths is the library's; this file is what only this
 * demo has: where the band sits in its reflection target and how near the band's picture may come.
 *
 * RECOMMENDED (no console run: every number an ESTIMATE)
 * Cost:       the band: 256 x 128 texels in the reflection target, 64 KB (street), 128 KB (map), 0 (hills: a spare
 *             region); the far world sent a second time (COUNTED on the host: README "Heat haze, hexagons")
 * Start with: HEXH_NEAR_BACK = 6 m: the band holds what is farther than HEAT_DIST - 6 from the eye
 * Status:     ADVISED: host tested (host/heat_hex_test.c); nothing of it has run on a console
 * Maturity:   PROTOTYPE v0.1
 *
 * Include after heat.h. The band's pass and the draw are demo.c's (heat_band_pass, heat_hex_draw). */
#ifndef PL4NAR_HEAT_HEX_H
#define PL4NAR_HEAT_HEX_H
#include "heat.h"
#include "sh4der_hexheat.h"

#define HEXH_NEAR_BACK 6.0f        /* the band holds what is farther than HEAT_DIST less this, metres */

typedef struct hexh {
    sh4der_hexheat_t h;
    unsigned frame;                /* views since the gate opened */
    float pf[3];                   /* where the camera looked in the view before */
} hexh_t;

static inline void hexh_reset(hexh_t *x) { memset(x, 0, sizeof *x); }

/* A frame's start: the camera looks along f (unit). The far picture's sideways travel since the view before, summed. */
static inline void hexh_view(hexh_t *x, const float f[3]) {
    if (x->frame) {
        float slip[2];
        heat_slip(x->pf, f, HEAT_FOCAL, slip);
        sh4der_hexheat_slip(&x->h, slip[0]);
    }
    memcpy(x->pf, f, sizeof x->pf);
    x->frame++;
}

#endif
