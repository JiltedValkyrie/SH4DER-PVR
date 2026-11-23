/* SH4DER-PVR: soft ground test

   test/deform_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the depth grid: the default numbers and bad arguments; a fresh grid is flat at the surface; a stamp
   presses a bowl (the centre at the source's lowest point, never below the hard bottom, shallower toward the edge),
   raises a rim round it and leaves the ground beyond untouched; a second, higher stamp does not lift a dent; `amount`
   eases the dent in; a source above the surface or outside the window changes nothing; the height read blends between
   cells and is the surface outside the window; depth and slope agree with the height; the refill closes dents and
   rims at its rate, carries fractions, and does nothing at rate 0; the window follows a point, keeps the dents that
   stay inside, drops the ones that leave, and survives a jump past its own size; the print list keeps the newest
   prints, ages and fades them, and builds a quad of the asked size that every triangle of has area.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_deform.h"
#include "../include/sh4der_guard.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.00002f
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

#define W 32                      /* test window, cells */
#define H 24
#define CELL 0.25f                /* metres a cell */
#define FLOOR_Y (-0.32f)          /* hard bottom */
#define CEIL_Y 0.0f               /* undisturbed surface: one step is 0.32 / 128 = 0.0025 m */
#define STEP 0.0025f

static uint8_t cells[W * H];

static void fresh(sh4der_deform_t *d) {
    CHECK(sh4der_deform_init(d, cells, W, H, CELL, FLOOR_Y, CEIL_Y) == 1);
}

static void defaults_and_bad(void) {
    sh4der_deform_t d;
    float sx = 9.0f, sz = 9.0f;

    CHECK(SH4DER_DEFORM_LEVEL == 128 && SH4DER_DEFORM_GRID_W == 160 && SH4DER_DEFORM_GRID_H == 160 && SH4DER_DEFORM_PRINTS == 32);
    CHECK(NEAR_(SH4DER_DEFORM_CELL, 0.25f, 0.0f) && NEAR_(SH4DER_DEFORM_RIM, 0.25f, 0.0f) && NEAR_(SH4DER_DEFORM_RIM_REACH, 1.6f, 0.0f));
    CHECK(SH4DER_DEFORM_GRID_W * SH4DER_DEFORM_GRID_H == 25600);            /* the 25 KB the header promises */
    memset(&d, 0, sizeof(d));
    CHECK(sh4der_deform_init(NULL, cells, W, H, CELL, FLOOR_Y, CEIL_Y) == 0);
    CHECK(sh4der_deform_init(&d, NULL, W, H, CELL, FLOOR_Y, CEIL_Y) == 0);
    CHECK(sh4der_deform_init(&d, cells, 1, H, CELL, FLOOR_Y, CEIL_Y) == 0);
    CHECK(sh4der_deform_init(&d, cells, W, H, 0.0f, FLOOR_Y, CEIL_Y) == 0);
    CHECK(sh4der_deform_init(&d, cells, W, H, CELL, CEIL_Y, CEIL_Y) == 0);
    CHECK(d.cells == NULL);                                       /* a refused init writes nothing */
    CHECK(sh4der_deform_stamp(&d, 1.0f, 1.0f, -0.1f, 0.2f, 1.0f) == 0);   /* no grid: nothing */
    CHECK(sh4der_deform_refill(&d, 1.0f) == 0 && sh4der_deform_recentre(&d, 5.0f, 5.0f) == 0);
    CHECK(sh4der_deform_height(&d, 1.0f, 1.0f) == 0.0f && sh4der_deform_depth(&d, 1.0f, 1.0f) == 0.0f);
    sh4der_deform_slope(&d, 1.0f, 1.0f, &sx, &sz);
    CHECK(sx == 0.0f && sz == 0.0f);

    fresh(&d);
    CHECK(d.origin_x == 0 && d.origin_z == 0 && NEAR_(d.step, STEP, 1e-7f) && d.refill == 0.0f);
    CHECK(NEAR_(sh4der_deform_height(&d, 3.1f, 2.7f), CEIL_Y, SH4DER_TEST_TOL));   /* flat at the surface */
    CHECK(sh4der_deform_depth(&d, 3.1f, 2.7f) == 0.0f);
    CHECK(sh4der_deform_stamp(&d, 3.0f, 3.0f, -0.1f, 0.0f, 1.0f) == 0);    /* no radius */
    CHECK(sh4der_deform_stamp(&d, 3.0f, 3.0f, -0.1f, 0.3f, 0.0f) == 0);    /* no amount */
    CHECK(sh4der_deform_stamp(&d, 3.0f, 3.0f, 0.05f, 0.3f, 1.0f) == 0);    /* the source is above the surface */
    CHECK(sh4der_deform_stamp(&d, 300.0f, 3.0f, -0.1f, 0.3f, 1.0f) == 0);  /* outside the window */
    CHECK(sh4der_deform_stamp(&d, NAN, 3.0f, -0.1f, 0.3f, 1.0f) == 0);     /* not a number */
}

static void stamp(void) {
    sh4der_deform_t d;
    float sx, sz;
    unsigned n;

    fresh(&d);
    /* a 0.5 m source, lowest point 0.20 m under the surface, at a grid node (cell 16, 12 = 4.0 m, 3.0 m) */
    n = sh4der_deform_stamp(&d, 4.0f, 3.0f, -0.20f, 0.5f, 1.0f);
    CHECK(n > 8u);
    CHECK(NEAR_(sh4der_deform_height(&d, 4.0f, 3.0f), -0.20f, STEP));          /* the centre: at the source's lowest point */
    CHECK(sh4der_deform_height(&d, 4.25f, 3.0f) > sh4der_deform_height(&d, 4.0f, 3.0f));   /* a bowl: shallower one cell out */
    CHECK(NEAR_(sh4der_deform_height(&d, 4.25f, 3.0f), -0.20f + 0.25f * 0.25f / (2.0f * 0.5f), STEP));   /* dist^2 / (2 radius) */
    CHECK(sh4der_deform_height(&d, 4.75f, 3.0f) > CEIL_Y);                     /* the rim stands above the surface */
    CHECK(sh4der_deform_height(&d, 4.75f, 3.0f) <= CEIL_Y + 0.20f * SH4DER_DEFORM_RIM + STEP);   /* and no higher than its share */
    CHECK(NEAR_(sh4der_deform_height(&d, 5.5f, 3.0f), CEIL_Y, SH4DER_TEST_TOL));   /* beyond the rim: untouched */
    CHECK(NEAR_(sh4der_deform_depth(&d, 4.0f, 3.0f), 0.20f / 0.32f, 0.01f));
    CHECK(sh4der_deform_depth(&d, 4.75f, 3.0f) == 0.0f);                       /* a rim is not depth */
    sh4der_deform_slope(&d, 4.25f, 3.0f, &sx, &sz);
    CHECK(sx > 0.0f && NEAR_(sz, 0.0f, 0.01f));                                /* east of the centre the ground rises eastward */
    sh4der_deform_slope(&d, 4.0f, 2.75f, &sx, &sz);
    CHECK(sz < 0.0f && NEAR_(sx, 0.0f, 0.01f));                                /* north of it, it falls toward the centre */

    /* a second, shallower stamp on the same spot does not lift the dent, and its rim does not fill it */
    CHECK(sh4der_deform_stamp(&d, 4.0f, 3.0f, -0.05f, 0.5f, 1.0f) == 0);
    CHECK(NEAR_(sh4der_deform_height(&d, 4.0f, 3.0f), -0.20f, STEP));
    sh4der_deform_stamp(&d, 4.6f, 3.0f, -0.10f, 0.2f, 1.0f);                   /* a small source beside it: its rim reaches the old dent */
    CHECK(sh4der_deform_height(&d, 4.25f, 3.0f) < CEIL_Y);                     /* the old dent is still a dent */

    /* the hard bottom: a source far below presses to the floor and no further */
    fresh(&d);
    sh4der_deform_stamp(&d, 4.0f, 3.0f, -5.0f, 0.5f, 1.0f);
    CHECK(NEAR_(sh4der_deform_height(&d, 4.0f, 3.0f), FLOOR_Y, SH4DER_TEST_TOL));
    CHECK(NEAR_(sh4der_deform_depth(&d, 4.0f, 3.0f), 1.0f, SH4DER_TEST_TOL));

    /* no rim when the rim share is 0 */
    fresh(&d);
    d.rim = 0.0f;
    sh4der_deform_stamp(&d, 4.0f, 3.0f, -0.20f, 0.5f, 1.0f);
    CHECK(NEAR_(sh4der_deform_height(&d, 4.75f, 3.0f), CEIL_Y, SH4DER_TEST_TOL));

    /* amount eases the dent in: a quarter of the way, then further, never past the target */
    fresh(&d);
    sh4der_deform_stamp(&d, 4.0f, 3.0f, -0.20f, 0.5f, 0.25f);
    CHECK(NEAR_(sh4der_deform_height(&d, 4.0f, 3.0f), -0.05f, STEP));
    sh4der_deform_stamp(&d, 4.0f, 3.0f, -0.20f, 0.5f, 0.25f);
    CHECK(sh4der_deform_height(&d, 4.0f, 3.0f) < -0.05f - STEP && sh4der_deform_height(&d, 4.0f, 3.0f) > -0.20f);
    for (n = 0; n < 60u; n++) sh4der_deform_stamp(&d, 4.0f, 3.0f, -0.20f, 0.5f, 0.25f);
    CHECK(NEAR_(sh4der_deform_height(&d, 4.0f, 3.0f), -0.20f, STEP));   /* settled on the target: a small move still takes a whole step */

    /* the height read blends between cells */
    fresh(&d);
    cells[12 * W + 16] = 28;                                                   /* one cell 100 steps down: 0.25 m deep */
    CHECK(NEAR_(sh4der_deform_height(&d, 4.0f, 3.0f), -0.25f, SH4DER_TEST_TOL));
    CHECK(NEAR_(sh4der_deform_height(&d, 4.125f, 3.0f), -0.125f, SH4DER_TEST_TOL));   /* half way to the next cell */
    CHECK(NEAR_(sh4der_deform_height(&d, 4.125f, 3.125f), -0.0625f, SH4DER_TEST_TOL));   /* half way both ways */
    CHECK(NEAR_(sh4der_deform_height(&d, -50.0f, 3.0f), CEIL_Y, SH4DER_TEST_TOL));    /* outside the window */
}

static void refill(void) {
    sh4der_deform_t d;

    fresh(&d);
    cells[5] = 100;                                                            /* a dent, 28 steps deep */
    cells[6] = 140;                                                            /* a rim, 12 steps high */
    CHECK(sh4der_deform_refill(&d, 10.0f) == 0 && cells[5] == 100);            /* snow: rate 0, nothing moves */
    d.refill = 4.0f;                                                           /* four steps a second */
    CHECK(sh4der_deform_refill(&d, 0.1f) == 0 && cells[5] == 100);             /* 0.4 of a step: carried */
    CHECK(sh4der_deform_refill(&d, 0.1f) == 0);
    CHECK(sh4der_deform_refill(&d, 0.1f) == 1u && cells[5] == 101 && cells[6] == 139);   /* 1.2 steps: one applied, 0.2 carried */
    CHECK(sh4der_deform_refill(&d, 2.0f) == 8u && cells[5] == 109 && cells[6] == 131);
    CHECK(sh4der_deform_refill(&d, 100.0f) == 400u && cells[5] == 128 && cells[6] == 128);   /* closed, and not past the surface */
    CHECK(cells[7] == 128);                                                    /* flat ground stays flat */
}

static void window(void) {
    sh4der_deform_t d;
    unsigned n;

    fresh(&d);
    CHECK(sh4der_deform_recentre(&d, 4.0f, 3.0f) == 0);                        /* (16, 12) is the middle already */
    sh4der_deform_stamp(&d, 4.0f, 3.0f, -0.20f, 0.3f, 1.0f);
    sh4der_deform_stamp(&d, 0.5f, 3.0f, -0.20f, 0.3f, 1.0f);                   /* near the west edge */
    n = sh4der_deform_recentre(&d, 5.0f, 3.0f);                                /* one metre east: four columns enter */
    CHECK(n == 4u * H && d.origin_x == 4 && d.origin_z == 0);
    CHECK(NEAR_(sh4der_deform_height(&d, 4.0f, 3.0f), -0.20f, STEP));          /* the dent that stayed inside is kept, at its world place */
    CHECK(NEAR_(sh4der_deform_height(&d, 0.5f, 3.0f), CEIL_Y, SH4DER_TEST_TOL));   /* the one that left reads as surface */
    CHECK(NEAR_(sh4der_deform_height(&d, 8.5f, 3.0f), CEIL_Y, SH4DER_TEST_TOL));   /* the columns that came in are fresh */
    sh4der_deform_recentre(&d, 4.0f, 3.0f);                                    /* back: the west dent does not come back */
    CHECK(NEAR_(sh4der_deform_height(&d, 0.5f, 3.0f), CEIL_Y, SH4DER_TEST_TOL));
    CHECK(NEAR_(sh4der_deform_height(&d, 4.0f, 3.0f), -0.20f, STEP));
    n = sh4der_deform_recentre(&d, 4.0f, 2.5f);                                /* two rows north */
    CHECK(n == 2u * W && d.origin_z == -2);
    CHECK(NEAR_(sh4der_deform_height(&d, 4.0f, 3.0f), -0.20f, STEP));
    sh4der_deform_stamp(&d, 4.0f, -0.25f, -0.20f, 0.3f, 1.0f);                 /* a dent at a negative world coordinate */
    CHECK(NEAR_(sh4der_deform_height(&d, 4.0f, -0.25f), -0.20f, STEP));
    n = sh4der_deform_recentre(&d, 400.0f, -300.0f);                           /* a jump past the window: all fresh */
    CHECK(n == (unsigned)(W * H));
    CHECK(NEAR_(sh4der_deform_height(&d, 400.0f, -300.0f), CEIL_Y, SH4DER_TEST_TOL));
    for (n = 0; n < (unsigned)(W * H); n++) CHECK(cells[n] == 128);
}

static void prints(void) {
    static sh4der_deform_prints_t p;
    sh4der_deform_corner_t q[4];
    unsigned i, live = 0;

    sh4der_deform_prints_reset(&p);
    CHECK(sh4der_deform_prints_quad(&p, 0, 0.01f, 0.05f, 0.12f, 20.0f, 4.0f, q) == 0);   /* empty */
    CHECK(sh4der_deform_prints_quad(&p, SH4DER_DEFORM_PRINTS, 0.01f, 0.05f, 0.12f, 20.0f, 4.0f, q) == 0);
    sh4der_deform_prints_add(&p, 2.0f, 3.0f, 0.0f, 5.0f);                      /* toe toward +z, any length */
    CHECK(sh4der_deform_prints_quad(&p, 0, 0.01f, 0.05f, 0.12f, 20.0f, 4.0f, q) == 4u);
    CHECK(NEAR_(q[0].x, 1.95f, SH4DER_TEST_TOL) && NEAR_(q[0].z, 2.88f, SH4DER_TEST_TOL));   /* heel, one side */
    CHECK(NEAR_(q[3].x, 2.05f, SH4DER_TEST_TOL) && NEAR_(q[3].z, 3.12f, SH4DER_TEST_TOL));   /* toe, the other */
    CHECK(NEAR_(q[1].y, 0.01f, 0.0f) && q[0].alpha == 1.0f);
    CHECK(q[0].v == 1.0f && q[2].v == 0.0f && q[0].u == 0.0f && q[1].u == 1.0f);            /* the toe at the texture's top */
    for (i = 0; i < 2u; i++) {                                                 /* both triangles of the strip have area, seen from above */
        const float a[3] = { q[i].x - 2.0f, q[i].z - 3.0f, 5.0f }, b[3] = { q[i + 1u].x - 2.0f, q[i + 1u].z - 3.0f, 5.0f };
        const float c[3] = { q[i + 2u].x - 2.0f, q[i + 2u].z - 3.0f, 5.0f };

        CHECK(sh4der_tri_area_ok(a, b, c, 0.1f, SH4DER_GUARD_MIN_AREA2));
    }
    sh4der_deform_prints_add(&p, 0.0f, 0.0f, 0.0f, 0.0f);                      /* no direction: stored as +z */
    CHECK(p.print[1].dir_x == 0.0f && p.print[1].dir_z == 1.0f);
    sh4der_deform_prints_tick(&p, 17.0f, 20.0f);                               /* 3 s of life left, fade 4 s: three quarters */
    CHECK(sh4der_deform_prints_quad(&p, 0, 0.01f, 0.05f, 0.12f, 20.0f, 4.0f, q) == 4u && NEAR_(q[0].alpha, 0.75f, SH4DER_TEST_TOL));
    CHECK(sh4der_deform_prints_quad(&p, 0, 0.01f, 0.05f, 0.12f, 20.0f, 0.0f, q) == 4u && q[0].alpha == 1.0f);   /* no fade asked */
    sh4der_deform_prints_tick(&p, 4.0f, 20.0f);                                /* past its life: gone */
    CHECK(sh4der_deform_prints_quad(&p, 0, 0.01f, 0.05f, 0.12f, 20.0f, 4.0f, q) == 0);
    for (i = 0; i < SH4DER_DEFORM_PRINTS + 5u; i++) sh4der_deform_prints_add(&p, (float)i, 0.0f, 1.0f, 0.0f);   /* more than it holds */
    for (i = 0; i < SH4DER_DEFORM_PRINTS; i++) live += p.print[i].age >= 0.0f;
    CHECK(live == SH4DER_DEFORM_PRINTS);
    for (i = 0; i < SH4DER_DEFORM_PRINTS; i++) CHECK(p.print[i].x >= 5.0f);    /* the five oldest were replaced */
}

int main(void) {
    defaults_and_bad();
    stamp();
    refill();
    window();
    prints();
    if (fails) { fprintf(stderr, "deform_test: %d FAILED\n", fails); return 1; }
    puts("deform: PASS defaults, bad arguments, stamp (bowl, floor, rim, ease-in, no lift), height blend, depth, slope, refill, window, prints");
    return 0;
}
