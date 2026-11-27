/* SH4DER-PVR: host test of the baked AO against exact shapes (include/sh4der_aobake.h)

   test/aobake_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Checks: an open ground point gives 255; a point under a sphere is darker than a point far from it; a start inside
   a sphere or a box is 0; a ray past the reach hits nothing (and the ray returns the reach); the ray set is unit length
   with y >= 0; the tick count (every 64 vertices, from 0); nrays 0, reach 0 and count 0 are safe; the ray set agrees
   with tools/ao_bake_core.h's (the host tool's set, built with libc) to AGREE_TOL.

   Runs on the plain C path (-DSH4DER_CORE_NO_SH4ZAM) and on the SH4ZAM software path (make test-shz).
   Build: gcc -O2 -std=c99 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude
          test/aobake_test.c -lm */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_aobake.h"
#include "../tools/ao_bake_core.h"

#define AGREE_TOL 0.000001f     /* the two ray sets agree to a millionth on both paths (the SH4ZAM path measured 1.2e-7) */

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

static unsigned ticks;
static void count_tick(void) { ticks++; }

#define NV 200u
static float P[NV][3], N[NV][3];
static uint8_t OUT[NV];
static float DIRS[64][3];

static const float UP[3] = { 0.0f, 1.0f, 0.0f };

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    sh4der_aobake_occl_t ground, sph_occl, box_occl, far_occl;
    static const sh4der_aobake_sph_t sph_under[1] = { { { 0.0f, 2.0f, 0.0f }, 1.0f } };
    static const sh4der_aobake_sph_t sph_far[1] = { { { 0.0f, 5.0f, 0.0f }, 1.0f } };
    static const sh4der_aobake_sph_t sph_centre[1] = { { { 0.0f, 0.0f, 0.0f }, 1.0f } };
    static const sh4der_aobake_box_t box_round[1] = { { { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f } } };
    float d[3], o[3], a, b;
    unsigned k, i;
    aob_t core;

    memset(&ground, 0, sizeof ground);
    ground.ground = 1; ground.ground_y = 0.0f;

    /* 1. an open ground point: a plane, nothing above it within reach */
    {
        const float p0[3] = { 1.0f, 0.0f, 2.0f };
        uint8_t byte = 0;

        sh4der_aobake_rays(DIRS, 64);
        sh4der_aobake_bytes(&ground, &p0, &UP, 1, DIRS, 64, 3.0f, 0.02f, &byte, NULL);
        CHECK(byte == 255u);
    }

    /* 2. under a sphere is darker than far from it */
    {
        const float under[3] = { 0.0f, 0.0f, 0.0f }, far[3] = { 20.0f, 0.0f, 0.0f };
        uint8_t bu = 0, bf = 0;

        sph_occl = ground;
        sph_occl.sph = sph_under; sph_occl.nsph = 1;
        sh4der_aobake_bytes(&sph_occl, &under, &UP, 1, DIRS, 64, 3.0f, 0.02f, &bu, NULL);
        sh4der_aobake_bytes(&sph_occl, &far, &UP, 1, DIRS, 64, 3.0f, 0.02f, &bf, NULL);
        CHECK(bu < 255u);
        CHECK(bu + 20u < bf);             /* clearly darker, not a rounding difference (a 1 m sphere 2 m up: about 0.875) */
        printf("  under a sphere %u, far %u\n", (unsigned)bu, (unsigned)bf);
        CHECK(bf == 255u);
    }

    /* 3. a start inside a sphere or a box is closed */
    {
        const float c0[3] = { 0.0f, 0.0f, 0.0f }, dn[3] = { 0.0f, -1.0f, 0.0f };
        uint8_t bs = 99, bb = 99;

        sph_occl = ground; sph_occl.ground = 0;
        sph_occl.sph = sph_centre; sph_occl.nsph = 1;
        CHECK(sh4der_aobake_ray(&sph_occl, c0, dn, 3.0f) == 0.0f);
        sh4der_aobake_bytes(&sph_occl, &c0, &UP, 1, DIRS, 64, 3.0f, 0.02f, &bs, NULL);
        CHECK(bs == 0u);

        box_occl = ground; box_occl.ground = 0;
        box_occl.box = box_round; box_occl.nbox = 1;
        CHECK(sh4der_aobake_ray(&box_occl, c0, dn, 3.0f) == 0.0f);
        sh4der_aobake_bytes(&box_occl, &c0, &UP, 1, DIRS, 64, 3.0f, 0.02f, &bb, NULL);
        CHECK(bb == 0u);
    }

    /* 4. a ray longer than the reach hits nothing; one within it hits at its distance */
    {
        float reach_far, reach_near;

        far_occl = ground; far_occl.ground = 0;
        far_occl.sph = sph_far; far_occl.nsph = 1;            /* bottom at y 4 */
        o[0] = 0.0f; o[1] = 0.0f; o[2] = 0.0f;
        d[0] = 0.0f; d[1] = 1.0f; d[2] = 0.0f;
        reach_far = sh4der_aobake_ray(&far_occl, o, d, 3.0f);
        CHECK(reach_far == 3.0f);                            /* past the reach: the reach comes back, no hit */
        reach_near = sh4der_aobake_ray(&far_occl, o, d, 10.0f);
        CHECK(NEAR_(reach_near, 4.0f, 1e-4f));               /* within it: the distance to the sphere's bottom */
        {
            uint8_t byte = 0;
            const float p0[3] = { 0.0f, 0.0f, 0.0f };

            sh4der_aobake_bytes(&far_occl, &p0, &UP, 1, DIRS, 64, 3.0f, 0.02f, &byte, NULL);
            CHECK(byte == 255u);                              /* every ray is past 3 m: open */
        }
    }

    /* 5. the ray set: unit length, y >= 0; a set of 1 or 7 rays is unit too */
    sh4der_aobake_rays(DIRS, 64);
    for (k = 0; k < 64u; k++) {
        const float len = sqrtf(DIRS[k][0] * DIRS[k][0] + DIRS[k][1] * DIRS[k][1] + DIRS[k][2] * DIRS[k][2]);

        CHECK(NEAR_(len, 1.0f, 1e-5f));
        CHECK(DIRS[k][1] >= 0.0f);
    }
    {
        float d1[1][3], d7[7][3];

        sh4der_aobake_rays(d1, 1);
        sh4der_aobake_rays(d7, 7);
        CHECK(NEAR_(d1[0][1], sqrtf(0.5f), 1e-5f));        /* k 0 of 1: u1 = 0.5, so y = sqrt(1 - u1) */
        CHECK(NEAR_(d7[0][0] * d7[0][0] + d7[0][1] * d7[0][1] + d7[0][2] * d7[0][2], 1.0f, 1e-5f));
    }

    /* 6. the tick count: one call at 0, 64, 128 ...; none for an empty list */
    ticks = 0;
    for (i = 0; i < NV; i++) { P[i][0] = (float)i; P[i][1] = 0.0f; P[i][2] = 0.0f; N[i][0] = 0.0f; N[i][1] = 1.0f; N[i][2] = 0.0f; }
    sh4der_aobake_bytes(&ground, P, N, 130u, DIRS, 64, 3.0f, 0.02f, OUT, count_tick);
    CHECK(ticks == 3u);
    ticks = 0;
    sh4der_aobake_bytes(&ground, P, N, 64u, DIRS, 64, 3.0f, 0.02f, OUT, count_tick);
    CHECK(ticks == 1u);
    ticks = 0;
    sh4der_aobake_bytes(&ground, P, N, 0u, DIRS, 64, 3.0f, 0.02f, OUT, count_tick);
    CHECK(ticks == 0u);

    /* 7. nrays 0, reach 0 and count 0 are safe: open (255), nothing written, no divide by zero */
    {
        uint8_t byte = 7;
        const float p0[3] = { 0.0f, 0.0f, 0.0f };

        sh4der_aobake_bytes(&ground, &p0, &UP, 1, DIRS, 0, 3.0f, 0.02f, &byte, NULL);
        CHECK(byte == 255u);
        byte = 7;
        sh4der_aobake_bytes(&ground, &p0, &UP, 1, DIRS, 64, 0.0f, 0.02f, &byte, NULL);
        CHECK(byte == 255u);
        byte = 7;
        sh4der_aobake_bytes(&ground, NULL, NULL, 0u, NULL, 0, 3.0f, 0.02f, &byte, NULL);
        CHECK(byte == 7u);                                   /* count 0: nothing written */
        sh4der_aobake_rays(NULL, 64);                        /* no array: nothing done */
        CHECK(sh4der_aobake_at(&ground, P[0], UP, DIRS, 0, 3.0f, 0.02f) == 1.0f);
    }

    /* 8. the library's ray set agrees with tools/ao_bake_core.h's, to AGREE_TOL */
    CHECK(aob_init(&core, NULL, NULL, 0) == 1);
    sh4der_aobake_rays(DIRS, 64);
    {
        float worst = 0.0f;

        for (k = 0; k < 64u; k++)
            for (i = 0; i < 3u; i++) {
                const float e = fabsf(DIRS[k][i] - core.dirs[k][i]);

                if (e > worst) worst = e;
            }
        CHECK(worst <= AGREE_TOL);
        if (worst > AGREE_TOL) fprintf(stderr, "  ray set: largest difference %g\n", (double)worst);
        else printf("  ray set: largest difference from ao_bake_core %g\n", (double)worst);
    }
    aob_free(&core);

    /* 9. the stored byte as the light uses it */
    a = sh4der_aobake_open(255u, 0); b = sh4der_aobake_open(0u, 1);
    CHECK(a == 1.0f && b == 0.0f);
    CHECK(NEAR_(sh4der_aobake_open(191u, 1), 0.498f, 0.01f));
    CHECK(sh4der_aobake_open(100u, 1) == 0.0f);

    if (fails) { printf("aobake_test: %d failed\n", fails); return 1; }
    printf("aobake_test: ok\n");
    return 0;
}
