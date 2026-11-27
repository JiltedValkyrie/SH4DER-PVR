/* SH4DER-PVR: tools/ao_bake_core.h, host test.  Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   cc -O2 -std=gnu11 -Wall -Wextra -Itools -Idemo/psm/ao -I<room dir> \
      test/ao_bake_test.c -lm -o ao_bake_test && ./ao_bake_test

   <room dir> holds room_mesh.h and room_ao.h (demo/psm/ao/build.py --room-only --out <dir>). The core, run over the
   room's occluders with room_bake.c's order (every vertex, an object's own vertices inside a solid moved into their
   triangles, each surface's grid filled), must give room_ao_v to 2 bytes. Then the wrap: a floor point just under
   the period's end sees a box past the seam as a point just before z 0 sees it with no wrap, and the two ends of the
   seam bake the same byte. Then the fill on a small grid. */

#include <stdio.h>
#include "ao_bake_core.h"
#include "room_data.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static aob_t b;
static uint8_t av[ROOM_V], abad[ROOM_V];

static void t_room(void) {
    unsigned i, k, s, diff = 0, worst = 0, worst_i = 0;
    int inside;

    CHECK(aob_init(&b, room_ot, room_ot_closed, ROOM_OT), "no memory");
    for (i = 0; i < ROOM_V; i++) { av[i] = aob_byte(aob_at(&b, room_v[i], room_n[i], &inside)); abad[i] = (uint8_t)inside; }
    for (k = 0; k < ROOM_OBJS; k++) {
        if (room_obj[k].surf >= 0) continue;
        for (i = room_obj[k].v0; i < (unsigned)room_obj[k].v0 + room_obj[k].vn; i++) {
            float a;

            if (abad[i] && aob_at_in(&b, i, room_v, room_n[i], &room_t[room_obj[k].t0], room_obj[k].tn, &a)) { av[i] = aob_byte(a); abad[i] = 0; }
        }
    }
    for (s = 0; s < ROOM_SURFS; s++) aob_fill(&av[room_surf[s].v0], &abad[room_surf[s].v0], room_surf[s].nu + 1u, room_surf[s].nv + 1u, 1u);
    for (i = 0; i < ROOM_V; i++) {
        const unsigned d = av[i] > room_ao_v[i] ? av[i] - room_ao_v[i] : room_ao_v[i] - av[i];

        if (d > 2u) diff++;
        if (d > worst) { worst = d; worst_i = i; }
    }
    printf("room: %u vertices against room_ao_v, %u differ by more than 2, worst %u (vertex %u)\n", (unsigned)ROOM_V, diff, worst, worst_i);
    CHECK(diff == 0u, "%u vertices differ by more than 2 bytes", diff);
    aob_free(&b);
}

/* a closed box, wound outward, from lo to hi: 12 triangles */
static unsigned box(float (*o)[3][3], const float lo[3], const float hi[3]) {
    static const uint8_t f[12][3] = { {0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{2,3,7},{2,7,6},{1,2,6},{1,6,5},{0,4,7},{0,7,3} };
    float c[8][3];
    unsigned i, k;

    for (i = 0; i < 8u; i++) {                 /* bit 0: x after y in the ring 0..3 on z lo, 4..7 on z hi */
        const unsigned r = i & 3u;

        c[i][0] = (r == 1u || r == 2u) ? hi[0] : lo[0];
        c[i][1] = (r >= 2u) ? hi[1] : lo[1];
        c[i][2] = i >= 4u ? hi[2] : lo[2];
    }
    for (i = 0; i < 12u; i++) for (k = 0; k < 3u; k++) memcpy(o[i][k], c[f[i][k]], sizeof c[0]);
    return 12u;
}

static void t_wrap(void) {
    static float ot[24][3][3], wr[64][3][3];
    static uint8_t cl[24], wcl[64];
    const float period = 20.0f, up[3] = { 0, 1, 0 };
    const float lo[3] = { -0.5f, 0.0f, 0.3f }, hi[3] = { 0.5f, 1.0f, 1.3f }, far_lo[3] = { -0.5f, 0.0f, 9.0f }, far_hi[3] = { 0.5f, 1.0f, 10.0f };
    const float p_wrap[3] = { 0.0f, 0.0f, period - 0.4f }, p_ref[3] = { 0.0f, 0.0f, -0.4f };
    const float s0[3] = { 0.0f, 0.0f, 0.0f }, s1[3] = { 0.0f, 0.0f, period };
    unsigned n, m, i;
    int in;
    float a_wrap, a_ref, a_bare;
    aob_t w, r;

    n = box(ot, lo, hi);
    n += box(ot + n, far_lo, far_hi);         /* the middle: never copied */
    for (i = 0; i < n; i++) cl[i] = 1u;
    m = aob_wrap(ot, cl, n, period, wr, wcl, 64u);
    CHECK(m == n + 12u, "wrap made %u triangles, want %u (the near box once, at +period)", m, n + 12u);
    CHECK(aob_init(&w, (const float (*)[3][3])wr, wcl, m) && aob_init(&r, (const float (*)[3][3])ot, cl, n), "no memory");
    a_wrap = aob_at(&w, p_wrap, up, &in);
    a_ref = aob_at(&r, p_ref, up, &in);
    a_bare = aob_at(&r, p_wrap, up, &in);
    printf("wrap: at the end %.4f, the same place before 0 with no wrap %.4f, the end with no wrap %.4f\n", a_wrap, a_ref, a_bare);
    CHECK(aob_byte(a_wrap) == aob_byte(a_ref), "the end does not see the box past the seam");
    CHECK(aob_byte(a_bare) > aob_byte(a_wrap), "the box made no difference");
    a_wrap = aob_at(&w, s0, up, &in);
    a_ref = aob_at(&w, s1, up, &in);
    printf("seam: z 0 %u, z period %u (bake once, give both)\n", aob_byte(a_wrap), aob_byte(a_ref));
    CHECK(aob_byte(a_wrap) == aob_byte(a_ref), "the seam's ends differ");
    aob_free(&w); aob_free(&r);
}

static void t_fill(void) {
    uint8_t v[5 * 5], bad[5 * 5];
    unsigned i, n;

    for (i = 0; i < 25u; i++) { v[i] = 200; bad[i] = 0; }
    for (i = 6; i < 9u; i++) { v[i] = 0; v[i + 5] = 0; v[i + 10] = 0; bad[i] = bad[i + 5] = bad[i + 10] = 1; }
    n = aob_fill(v, bad, 5, 5, 1);
    CHECK(n == 9u, "flagged %u", n);
    for (i = 0; i < 25u; i++) CHECK(v[i] == 200u && !bad[i], "point %u = %u after the fill", i, v[i]);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    t_room();
    t_wrap();
    t_fill();
    printf("ao_bake_core_test: %s (%d failed)\n", fails ? "FAILED" : "all passed", fails);
    return fails ? 1 : 0;
}
