/* Host test of PSM_WORLD_BOUND (demo/psm/psm_scene.h, the turn plan's world step) on the closed square (bake_square.py
   --closed). Two frames go through the same poses, one with the ground window as it always was (rings to PSM_ROW_END, the
   fit over the view to PSM_FAR) and one cut to the walls. For every pose:
     every cell the full window sends that can touch the ground inside the walls is sent by the cut one too, with the same
     screen positions at its corners, and the cut one sends no cell the full one does not (the picture inside the walls is
     the same ground);
     route A's map covers the cut window: a point inside the walls has coordinates in the target (0 .. 1);
   and over all poses it prints what the cut saves (rings, points placed, cells sent) and the shadow map's texel against
   the full fit at points on the ground in front of the camera. The fog: the farthest thing in the square (the keep's top
   from the far corner of the walk) is inside DRAW DISTANCE PSM_BOUND_DIST_I. Plain C. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../psm_scene.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static psm_scene_t scene;
static psm_frame_t ref, cut;
static uint16_t depth[1024u * 1024u];

static void run(psm_frame_t *f, const psm_state_t *st) {
    psm_frame_setup(f, &scene, st);
    psm_place_geo(f, &scene);
    if (st->route == PSM_ROUTE_A) { psm_frame_unshadowed(f, &scene); psm_frame_a_casters(f, &scene); psm_frame_a_ground(f); }
    else if (psm_frame_b_begin(f, &scene, depth)) { psm_frame_b_casters(f, &scene, 0); psm_frame_b_receivers(f, &scene, st); }
    else psm_frame_unshadowed(f, &scene);
    psm_colour(f, &scene);
}

static int inside(const float *p) { return p[0] >= scene.bnd[0] && p[0] <= scene.bnd[2] && p[2] >= scene.bnd[1] && p[2] <= scene.bnd[3]; }

/* the full window's cell (r, c) can touch bnd: its corners' box meets it, and its inner edge (a chord of ring r, no nearer
   the centre than row_r x cos(half a spoke)) is no farther than bnd's farthest corner */
static int meets(const psm_frame_t *f, unsigned r, unsigned c) {
    const unsigned a = r * f->gcols + c, q[4] = { a, a + 1u, a + f->gcols, a + f->gcols + 1u };
    float x0 = 1e30f, x1 = -1e30f, z0 = 1e30f, z1 = -1e30f, far = 0.0f;
    unsigned k;

    for (k = 0; k < 4u; k++) {
        const float dx = scene.bnd[(k & 1u) ? 2 : 0] - f->gc[0], dz = scene.bnd[(k & 2u) ? 3 : 1] - f->gc[1];

        if (dx * dx + dz * dz > far) far = dx * dx + dz * dz;
    }
    if (scene.row_r[r] * cosf(0.5f * PSM_COL_STEP) > sqrtf(far)) return 0;
    for (k = 0; k < 4u; k++) {
        const float *p = f->gpos[q[k]];

        if (p[0] < x0) x0 = p[0];
        if (p[0] > x1) x1 = p[0];
        if (p[2] < z0) z0 = p[2];
        if (p[2] > z1) z1 = p[2];
    }
    return !(x1 < scene.bnd[0] || x0 > scene.bnd[2] || z1 < scene.bnd[1] || z0 > scene.bnd[3]);
}

int main(void) {
    static const float pitches[3] = { PSM_PITCH, PSM_PITCH_DOWN, -0.25f };
    const float gx[3] = { 1.0f, 0.0f, 0.0f }, gz[3] = { 0.0f, 0.0f, 1.0f };
    psm_state_t st;
    unsigned poses = 0, miss = 0, extra = 0, moved = 0, uv_bad = 0, rows_ref = 0, rows_cut = 0, rows_cut_max = 0;
    unsigned long pts_ref = 0, pts_cut = 0, cells_ref = 0, cells_cut = 0;
    unsigned pts_cut_max = 0, cells_cut_max = 0, tex_n = 0, tex_worse = 0;
    float tex_sum = 0.0f, tex_max = 0.0f, x, z;
    int pi, yi;

    psm_scene_init_town(&scene);
    CHECK(scene.bound_ok == 1);
    CHECK(scene.bnd[0] < scene.x0 && scene.bnd[2] > scene.x1 && scene.bnd[1] < scene.z0 && scene.bnd[3] > scene.z1);   /* the walk is inside */
    printf("psm_bound_test: closed square: %u model vertices, %u model triangles, %u caster vertices, %u caster triangles in %u objects;"
           " bound x %.1f .. %.1f, z %.1f .. %.1f\n", scene.mvn, scene.mtn, scene.cvn, scene.ctn, scene.con,
           scene.bnd[0], scene.bnd[2], scene.bnd[1], scene.bnd[3]);
    psm_state_town(&st);
    ref.place_opt = cut.place_opt = 7u;
    ref.colour_opt = cut.colour_opt = PSM_COLOUR_TABLE | PSM_COLOUR_SEEN;
    ref.world_opt = 0u; cut.world_opt = PSM_WORLD_BOUND;
    for (x = scene.x0; x <= scene.x1 + 0.01f; x += 3.1f)
        for (z = scene.z0; z <= scene.z1 + 0.01f; z += 3.7f)
            for (pi = 0; pi < 3; pi++)
                for (yi = 0; yi < 12; yi++) {
                    unsigned r, c, i, need = 0, sent_r = 0, sent_c = 0;

                    st.x = x; st.z = z; st.pitch = pitches[pi]; st.yaw = 0.5236f * (float)yi + 0.11f * (float)pi;
                    st.route = (poses % 5u) == 4u ? PSM_ROUTE_B : PSM_ROUTE_A;   /* route B's raster on one pose in five */
                    run(&ref, &st);
                    run(&cut, &st);
                    poses++;
                    CHECK(cut.gbound == 1 && cut.gcols == ref.gcols && cut.grows <= ref.grows);
                    for (r = 0; r + 1u < ref.grows; r++)
                        for (c = 0; c + 1u < ref.gcols; c++) {
                            const int on = ref.cell[r * PSM_COLS + c] != 0u;
                            const int con = r + 1u < cut.grows && cut.cell[r * PSM_COLS + c] != 0u;

                            sent_r += on; sent_c += con;
                            if (con && !on) extra++;
                            if (!on || !meets(&ref, r, c)) continue;
                            if (!con) {
                                if (++miss < 6u) printf("miss: pose (%.1f, %.1f) yaw %.2f pitch %.2f: cell %u, %u of %u rings (cut %u), ring %.2f m, cell %u\n",
                                                        x, z, st.yaw, st.pitch, r, c, ref.grows, cut.grows, scene.row_r[r],
                                                        r + 1u < cut.grows ? cut.cell[r * PSM_COLS + c] : 9u);
                                continue;
                            }
                            {
                                const unsigned a = r * ref.gcols + c, q[4] = { a, a + 1u, a + ref.gcols, a + ref.gcols + 1u };
                                unsigned k;

                                for (k = 0; k < 4u; k++)
                                    if (memcmp(ref.gscr[q[k]], cut.gscr[q[k]], sizeof ref.gscr[0])) { moved++; break; }
                            }
                        }
                    for (i = 0; i < cut.gvn; i++) {
                        need += cut.gneed[i];
                        if (st.route == PSM_ROUTE_A && cut.gneed[i] && inside(cut.gpos[i]) && cut.built
                            && !(cut.gok[i] && cut.guv[i][0] >= -1e-3f && cut.guv[i][0] <= 1.001f && cut.guv[i][1] >= -1e-3f && cut.guv[i][1] <= 1.001f))
                            uv_bad++;
                    }
                    CHECK(need == cut.gneed_n);
                    rows_ref += ref.grows; rows_cut += cut.grows;
                    if (cut.grows > rows_cut_max) rows_cut_max = cut.grows;
                    pts_ref += ref.gvn; pts_cut += need;
                    if (need > pts_cut_max) pts_cut_max = need;
                    cells_ref += sent_r; cells_cut += sent_c;
                    if (sent_c > cells_cut_max) cells_cut_max = sent_c;
                    if (ref.built && cut.built)                    /* the texel on the ground 4 and 10 m ahead, inside the walls */
                        for (i = 0; i < 2u; i++) {
                            const float d = i ? 10.0f : 4.0f, s = sinf(st.yaw), co = cosf(st.yaw);
                            const float p[3] = { st.x + s * d, 0.0f, st.z - co * d };
                            float tr, tc;

                            if (!inside(p)) continue;
                            tr = sh4der_psm_texel_m(&ref.map, ref.size, p, gx, gz, 0.05f);
                            tc = sh4der_psm_texel_m(&cut.map, cut.size, p, gx, gz, 0.05f);
                            if (!(tr < 1e29f && tc < 1e29f && tr > 0.0f)) continue;
                            tex_n++; tex_sum += tc / tr;
                            if (tc / tr > tex_max) tex_max = tc / tr;
                            if (tc > tr * 1.001f) tex_worse++;
                        }
                }
    CHECK(miss == 0u && extra == 0u && moved == 0u && uv_bad == 0u);
    printf("psm_bound_test: %u poses: cells missed %u, extra %u, moved %u; route A points off the map %u\n", poses, miss, extra, moved, uv_bad);
    printf("psm_bound_test: rings a frame %.1f -> %.1f (most %u); ground points placed %.0f -> %.0f (most %u); cells sent %.0f -> %.0f"
           " (most %u, 2 triangles a cell)\n", (double)rows_ref / poses, (double)rows_cut / poses, rows_cut_max,
           (double)pts_ref / poses, (double)pts_cut / poses, pts_cut_max, (double)cells_ref / poses, (double)cells_cut / poses, cells_cut_max);
    CHECK(tex_n > 50u);
    printf("psm_bound_test: shadow texel, square fit over view fit, %u ground points: mean %.2f, largest %.2f, %u larger\n",
           tex_n, tex_n ? tex_sum / (float)tex_n : 0.0f, tex_max, tex_worse);
    CHECK(tex_n && tex_sum / (float)tex_n < 1.0f);
    {   /* PSM_KEEP_CAST: route B's static casters kept between sun steps, against every caster drawn every frame with the same
           key range; 240 steps, the camera still and turning, the movers walking, the sun stepped a clock minute */
        static psm_frame_t fr, fk;
        static uint16_t dk[PSM_MAP_STATIC * PSM_MAP_STATIC], keep[PSM_MAP_STATIC * PSM_MAP_STATIC];
        unsigned step, bad_d = 0, bad_l = 0, plain_l = 0, rect_sum = 0, rect_n = 0;
        float yaw0;

        psm_state_town(&st);
        st.route = PSM_ROUTE_B; st.sun_step = PSM_SUN_STEP_H; st.tspeed = 3u;
        yaw0 = st.yaw;
        fr.place_opt = fk.place_opt = 7u;
        fr.colour_opt = fk.colour_opt = PSM_COLOUR_TABLE | PSM_COLOUR_SEEN;
        fr.world_opt = fk.world_opt = PSM_WORLD_BOUND;
        fr.keep_opt = fk.keep_opt = PSM_KEEP_SUN | PSM_KEEP_CAST;
        CHECK(psm_map_size(&st) <= PSM_MAP_STATIC);
        for (step = 0; step < 240u; step++) {
            unsigned i;

            st.yaw = yaw0 + ((step % 60u) >= 30u ? 0.02f * (float)(step % 30u) : 0.0f);
            psm_town_update(&scene, &st, 1.0f / 30.0f);
            psm_frame_setup(&fr, &scene, &st); psm_place_geo(&fr, &scene);
            psm_frame_setup(&fk, &scene, &st); psm_place_geo(&fk, &scene);
            CHECK(fr.gbound && fk.gbound);
            if (psm_frame_b_keep(&fr, &scene, depth, NULL)) psm_frame_b_receivers(&fr, &scene, &st);
            if (psm_frame_b_keep(&fk, &scene, dk, keep)) psm_frame_b_receivers(&fk, &scene, &st);
            if (memcmp(depth, dk, (size_t)fr.size * fr.size * sizeof depth[0])) bad_d++;
            for (i = 0; i < scene.mvn; i++) if (fr.mlit[i] != fk.mlit[i]) { bad_l++; break; }
            for (i = 0; i < fr.gvn; i++) if (fr.glit[i] != fk.glit[i]) { bad_l++; break; }
            CHECK(fr.tris == fk.tris && fr.drawn == fk.drawn);
            for (i = 0; i < fk.ck_rn; i++) rect_sum += (unsigned)((fk.ck_rect[i][2] - fk.ck_rect[i][0]) * (fk.ck_rect[i][3] - fk.ck_rect[i][1]));
            rect_n++;
        }
        {                                                          /* and against the plain route B (its own key range): the shadow */
            ref.world_opt = PSM_WORLD_BOUND;
            psm_frame_setup(&ref, &scene, &st); psm_place_geo(&ref, &scene);
            if (psm_frame_b_begin(&ref, &scene, depth)) { psm_frame_b_casters(&ref, &scene, 0); psm_frame_b_receivers(&ref, &scene, &st); }
            for (step = 0; step < ref.gvn; step++) plain_l += ref.glit[step] != fk.glit[step];
        }
        CHECK(bad_d == 0u && bad_l == 0u);
        CHECK(fk.ck_hits > 150u && fk.ck_hits < 240u);             /* kept on most steps; drawn again when the sun stepped */
        CHECK(fr.ck_hits == 0u);
        printf("psm_bound_test: PSM_KEEP_CAST: 240 route B steps, %u maps differ, %u light shares differ; the static map kept on %u steps;"
               " movers' texels copied back %u a step (of %u)\n", bad_d, bad_l, fk.ck_hits, rect_n ? rect_sum / rect_n : 0u, fk.size * fk.size);
        printf("psm_bound_test: PSM_KEEP_CAST: its key range against route B's own, last step: %u of %u ground shares differ\n", plain_l, ref.gvn);
    }
    {                                                              /* the fog: the keep's top from the far corner of the walk */
        const float ex = scene.x1, ez = scene.z0, kx = -27.0f - 3.0f, kz = 31.0f + 3.0f, ky = 28.0f - PSM_EYE_H;
        const float d = sqrtf((ex - kx) * (ex - kx) + (ez - kz) * (ez - kz) + ky * ky);

        CHECK(psm_dist_m[PSM_BOUND_DIST_I] == 75u);
        CHECK(d < (float)psm_dist_m[PSM_BOUND_DIST_I]);
        printf("psm_bound_test: fog: DRAW DISTANCE %u m, fog from %.0f m; the keep's top is %.1f m from the far corner\n",
               (unsigned)psm_dist_m[PSM_BOUND_DIST_I], psm_dist_m[PSM_BOUND_DIST_I] * PSM_FOG_START, d);
    }
    printf(fails ? "psm_bound_test: %d FAILED\n" : "psm_bound_test: ok\n", fails);
    return fails != 0;
}
