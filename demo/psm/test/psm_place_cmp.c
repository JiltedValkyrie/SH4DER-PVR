/* SH4DER-PVR: host check that the placement's options (psm_scene.h, PSM_PLACE_*) do not change the picture

   demo/psm/test/psm_place_cmp.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The town's opaque list, as psm_dc.c sends it (psm_send.h's own walk, its vertices caught here), drawn by a small
   software rasteriser: 640 x 480, the PVR's depth rule (1 / w, greater or equal wins), Gouraud vertex colours, no
   textures. The same view is drawn with place_opt 0 and with each option set (PSM_PLACE_GROUND among them), and the pictures are compared pixel by
   pixel, colour and depth. A triangle the options leave out is off the screen or has no area, so every pixel must be
   the same. Views: the boot place and two others, twelve headings, three pitches (sky, boot, down), routes A and B,
   DRAW DISTANCE infinite and 30 m. Also counted: the vertices sent and the blocks left out.

   With a folder as the first argument it writes two pictures of the boot view (plain and every option) there as PPM.
   Plain C path only (on a host the stored 1 / w and the guard's reciprocal are the same number; on the console they are
   not: see PSM_PLACE_AREA). Nothing here is a console measurement. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../psm_scene.h"
#include "../psm_sky.h"

#define W 640
#define H 480
#define NOPT 7u                         /* the option sets compared against the plain placement */

static uint32_t img[W * H];
static float zbuf[W * H];
static int cur_kind = -1;
static unsigned run;
static float vx[3], vy[3], vz[3];
static uint32_t vc[3];
static unsigned long tris_drawn;

static void tri(void) {
    const float ax = vx[0], ay = vy[0], bx = vx[1], by = vy[1], cx = vx[2], cy = vy[2];
    float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax), inv;
    int x0, x1, y0, y1, x, y, hit = 0;

    if (area == 0.0f || !isfinite(area)) return;
    x0 = (int)floorf(fminf(ax, fminf(bx, cx))); x1 = (int)ceilf(fmaxf(ax, fmaxf(bx, cx)));
    y0 = (int)floorf(fminf(ay, fminf(by, cy))); y1 = (int)ceilf(fmaxf(ay, fmaxf(by, cy)));
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W - 1) x1 = W - 1;
    if (y1 > H - 1) y1 = H - 1;
    if (x0 > x1 || y0 > y1) return;
    inv = 1.0f / area;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float w0 = ((bx - px) * (cy - py) - (by - py) * (cx - px)) * inv;
            float w1 = ((cx - px) * (ay - py) - (cy - py) * (ax - px)) * inv;
            float w2 = 1.0f - w0 - w1, z;
            unsigned k, rgb = 0;

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            hit = 1;
            z = w0 * vz[0] + w1 * vz[1] + w2 * vz[2];
            if (!(z >= zbuf[y * W + x])) continue;
            zbuf[y * W + x] = z;
            for (k = 0; k < 3u; k++) {
                const unsigned sh = 16u - 8u * k;
                const float ch = w0 * (float)((vc[0] >> sh) & 255u) + w1 * (float)((vc[1] >> sh) & 255u) + w2 * (float)((vc[2] >> sh) & 255u);

                rgb |= (unsigned)(ch + 0.5f) << sh;
            }
            img[y * W + x] = rgb;
        }
    tris_drawn += (unsigned long)hit;           /* it covers a pixel centre (hidden or not) */
}

static void sink_vtx(int eol, float x, float y, float z, uint32_t argb) {
    if (run >= 3u) { vx[0] = vx[1]; vy[0] = vy[1]; vz[0] = vz[1]; vc[0] = vc[1]; vx[1] = vx[2]; vy[1] = vy[2]; vz[1] = vz[2]; vc[1] = vc[2]; }
    {
        const unsigned k = run < 3u ? run : 2u;

        vx[k] = x; vy[k] = y; vz[k] = z; vc[k] = argb;
    }
    if (run < 3u) run++;
    if (run >= 3u) tri();
    if (eol) run = 0;
}

#define PSM_SINK_HDR(kind) (cur_kind = (kind), run = 0u)
#define PSM_SINK_VTX(eol, X, Y, Z, U, V, ARGB, OARGB) sink_vtx((eol), (X), (Y), (Z), (ARGB))
#include "../psm_send.h"

static psm_scene_t scene;
static psm_frame_t frame;
static psm_sky_out_t sky;
static uint16_t depth[PSM_MAP_STATIC * PSM_MAP_STATIC];
static uint32_t ref_img[W * H];
static float ref_z[W * H];
static int fails;

/* one view drawn with the options `opt`; returns the opaque list's vertices */
static unsigned draw(psm_state_t *st, unsigned opt) {
    psm_sink_t k;
    unsigned i;

    frame.place_opt = opt;
    psm_frame_setup(&frame, &scene, st);
    psm_place_geo(&frame, &scene);
    psm_sky_build(&sky, &frame, st);
    if (st->route == PSM_ROUTE_B) {
        if (psm_frame_b_begin(&frame, &scene, depth)) { psm_frame_b_casters(&frame, &scene, 0); psm_frame_b_receivers(&frame, &scene, st); }
        else psm_frame_unshadowed(&frame, &scene);
    } else {
        psm_frame_unshadowed(&frame, &scene);
        psm_frame_a_casters(&frame, &scene);
        psm_frame_a_ground(&frame);
    }
    psm_colour(&frame, &scene);
    for (i = 0; i < W * H; i++) { img[i] = 0x00102030u; zbuf[i] = 0.0f; }
    psm_sink_reset(&k, 1, 1, 0u, NULL);
    run = 0; cur_kind = -1;
    psm_send_op(&k, &frame, &scene, st->atmos ? &sky : NULL);
    if (k.bad_short || k.bad_open || k.bad_coord) { fails++; printf("FAIL: the send's own checks (opt %u)\n", opt); }
    return k.verts;
}

static void ppm(const char *dir, const char *name) {
    char path[512];
    FILE *fp;
    unsigned i;

    snprintf(path, sizeof path, "%s/%s", dir, name);
    fp = fopen(path, "wb");
    if (!fp) { printf("cannot write %s\n", path); return; }
    fprintf(fp, "P6\n%d %d\n255\n", W, H);
    for (i = 0; i < W * H; i++) { fputc((int)((img[i] >> 16) & 255u), fp); fputc((int)((img[i] >> 8) & 255u), fp); fputc((int)(img[i] & 255u), fp); }
    fclose(fp);
}

int main(int argc, char **argv) {
    static const float place[3][2] = { { 0.0f, 0.0f }, { -6.0f, 4.0f }, { 5.0f, -9.0f } };     /* added to the boot place, inside the walk */
    static const float pitch[3] = { -1.25f, PSM_PITCH, PSM_PITCH_DOWN };
    static const unsigned opts[NOPT] = { PSM_PLACE_AREA, PSM_PLACE_BLOCKS, PSM_PLACE_SIDE, PSM_PLACE_AREA | PSM_PLACE_BLOCKS, PSM_PLACE_AREA | PSM_PLACE_BLOCKS | PSM_PLACE_SIDE,
                                         PSM_PLACE_GROUND, PSM_PLACE_AREA | PSM_PLACE_BLOCKS | PSM_PLACE_SIDE | PSM_PLACE_GROUND };
    static const char *const pname[3] = { "sky", "boot", "down" };
    unsigned long views = 0, bad_px = 0, v0_sum[3] = { 0, 0, 0 }, v_sum[3][NOPT], cull_sum[3] = { 0, 0, 0 }, n_p[3] = { 0, 0, 0 }, drawn0 = 0, drawn7 = 0, drawn15 = 0;
    unsigned long gb_sum[3] = { 0, 0, 0 }, gbc_sum[3] = { 0, 0, 0 }, gv_sum[3] = { 0, 0, 0 }, gvp_sum[3] = { 0, 0, 0 };
    psm_state_t st0, st;
    psm_walk_limits_t lim;
    unsigned p, h, pi, route, dist, o, i;

    memset(v_sum, 0, sizeof v_sum);
    psm_scene_init_town(&scene);
    psm_scene_limits(&scene, &lim);
    psm_state_town(&st0);
    printf("town: %u model vertices (%u static), %u triangles (%u static)\n", scene.mvn, scene.mdyn0, scene.mtn, scene.tdyn0);
    for (p = 0; p < 3u; p++)
        for (h = 0; h < 12u; h++)
            for (pi = 0; pi < 3u; pi++)
                for (route = 0; route < 2u; route++)
                    for (dist = 0; dist < 2u; dist++) {
                        unsigned v0;

                        st = st0;
                        st.x += place[p][0]; st.z += place[p][1];
                        st.yaw = st0.yaw + (float)h * (6.2831853f / 12.0f);
                        st.pitch = pitch[pi];
                        st.route = route ? PSM_ROUTE_B : PSM_ROUTE_A;
                        st.dist_i = dist ? 5u : PSM_DIST_INF;                  /* 30 m, or no cut */
                        frame.place_ok = 0;
                        tris_drawn = 0;
                        v0 = draw(&st, 0u);
                        drawn0 += tris_drawn;
                        memcpy(ref_img, img, sizeof img); memcpy(ref_z, zbuf, sizeof zbuf);
                        v0_sum[pi] += v0; n_p[pi]++;
                        for (o = 0; o < NOPT; o++) {
                            unsigned long d = 0;
                            unsigned v;

                            tris_drawn = 0;
                            if (opts[o] & PSM_PLACE_GROUND)       /* what the ground option leaves unplaced must never be read: poisoned */
                                for (i = 0; i < PSM_GV_MAX; i++)
                                    frame.gclip[i][0] = frame.gclip[i][1] = frame.gclip[i][2] = frame.gscr[i][0] = frame.gscr[i][1] = frame.gscr[i][2] = NAN;
                            v = draw(&st, opts[o]);
                            if (o == 4u) drawn7 += tris_drawn;
                            if (o == 6u) {         /* every option, the ground's blocks among them */
                                drawn15 += tris_drawn;
                                gb_sum[pi] += frame.gb_n; gbc_sum[pi] += frame.gb_culled; gv_sum[pi] += frame.gvn; gvp_sum[pi] += frame.gv_placed;
                            }
                            for (i = 0; i < W * H; i++) d += img[i] != ref_img[i] || zbuf[i] != ref_z[i];
                            if (d) {
                                if (fails < 10) printf("FAIL: place %u heading %u pitch %s route %c dist %u opt %u: %lu pixels differ\n", p, h * 30u, pname[pi], route ? 'B' : 'A', dist, opts[o], d);
                                fails++; bad_px += d;
                            }
                            if (v > v0) { fails++; printf("FAIL: more vertices with opt %u\n", opts[o]); }
                            if (!(opts[o] & (PSM_PLACE_BLOCKS | PSM_PLACE_SIDE)) && v != v0) { fails++; printf("FAIL: the area or ground option changed the send (opt %u)\n", opts[o]); }
                            v_sum[pi][o] += v;
                            if (o == 4u) cull_sum[pi] += frame.mb_culled;
                        }
                        views++;
                    }
    printf("%lu views x %u option sets against the plain placement: %lu pixels differ (colour or depth)\n", views, NOPT, bad_px);
    printf("blocks: %u of at most %u triangles a block\n", frame.mbn, PSM_MB_TRIS);
    for (pi = 0; pi < 3u; pi++)
        printf("  pitch %-4s opaque vertices a view: plain %lu, area %lu, blocks %lu, side %lu, area+blocks %lu, all %lu; blocks left out %lu of %u\n", pname[pi],
               v0_sum[pi] / n_p[pi], v_sum[pi][0] / n_p[pi], v_sum[pi][1] / n_p[pi], v_sum[pi][2] / n_p[pi], v_sum[pi][3] / n_p[pi], v_sum[pi][4] / n_p[pi],
               cull_sum[pi] / n_p[pi], frame.mbn);
    for (pi = 0; pi < 3u; pi++)
        printf("  pitch %-4s ground option (all four on): ground blocks %lu, left out %lu; ground points %lu, placed %lu (%.0f %%)\n", pname[pi],
               gb_sum[pi] / n_p[pi], gbc_sum[pi] / n_p[pi], gv_sum[pi] / n_p[pi], gvp_sum[pi] / n_p[pi],
               gv_sum[pi] ? 100.0 * (double)gvp_sum[pi] / (double)gv_sum[pi] : 0.0);
    printf("  triangles that cover a pixel centre: plain %lu, three options %lu, all four %lu (the same ones: nothing on screen was left out)\n", drawn0, drawn7, drawn15);
    if (drawn0 != drawn7 || drawn0 != drawn15) { fails++; printf("FAIL: the on-screen triangle counts differ\n"); }

    if (argc > 1) {                             /* the boot view, plain and with every option, as pictures */
        st = st0;
        frame.place_ok = 0;
        draw(&st, 0u); ppm(argv[1], "place_plain.ppm");
        draw(&st, PSM_PLACE_AREA | PSM_PLACE_BLOCKS | PSM_PLACE_SIDE); ppm(argv[1], "place_all.ppm");
        st.yaw += 0.7f;
        draw(&st, 0u); ppm(argv[1], "place_plain_turned.ppm");
        draw(&st, PSM_PLACE_AREA | PSM_PLACE_BLOCKS | PSM_PLACE_SIDE); ppm(argv[1], "place_all_turned.ppm");
    }
    printf(fails ? "psm_place_cmp: %d FAILED\n" : "psm_place_cmp: ok\n", fails);
    return fails != 0;
}
