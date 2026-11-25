/* SH4DER-PVR: the chain spike (c4), host test of the model

   demo/psm/compare/test/c4_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Checks ../c4_model.h and prints what the MODEL of the chain gives for the spike's scene. A model in plain C, not a
   measurement: the map and the mask here are made by rays, the way the PVR is expected to fill them. Built twice:
   plain C (SH4DER_CORE_NO_SH4ZAM) and on SH4ZAM's software back end.

     - the tables: every level is the cosine in real arithmetic rounded to a byte, every threshold its whole part
     - the window: lit exactly while the map's code is inside it, for every pair of codes
     - the bands: they tile each face (areas add up), every corner's code is inside its band, a corner two bands
       share is the same point in both (no crack between them), and no triangle of a band's strip has three equal
       corners, in the world or on the mask's screen
     - the camera's matrix: a point through it lands on the pixel whose ray found it
     - the chain against a ray to the sun, for every pixel of the 320 x 240 mask, at five suns (two of them 3 degrees
       off a box's faces, where the first console run showed the most errors) and three band widths:
         acne     the chain says shadow, the ray says lit, and the map pixel read holds the pixel's OWN face
         edge     the same, but the map pixel holds another face nearer the light: the map's pixel edge
         rim      the same, but the map pixel holds a face far BEHIND (its centre is off the edge of the pixel's
                  face): farther than the window reaches, so the cyclic compare reads shadow
         in bias  the chain says lit, the ray says shadow, and the caster is nearer than bias + 2 codes
         other    the same, farther: the map's pixel edge, or the wrap
         wrap     shadowed pixels whose caster is so much nearer the light that the cyclic compare reads lit
         steep    pixels of faces too steep to the light to compare (drawn lit): how many, and how many of them a ray
                  says are in shadow
       acne must be 0 with the bias rule as it stands, and rim must be 0 where the faces are grown. The faces are
       grown in the map pass by 0 and by 0.75 pixel.
     - the same two counts at every sun: azimuth 0 to 357 by 3 degrees at elevations 13, 26.5 and 45
     - c4b, the measured window (fix 4): Q from the window's lower edge equals the old Q for the cosine's edge; the
       edges found from a picture of known pairs are the table's own; on a "chip" whose window is 3 codes narrower at
       each end (the size a third party's reading gives, REPORTED) the edges found are the narrowed ones, and the
       chain with Q from them has no acne where Q from the cosine's edge has some (both at b0 1.5, c4's first value)
*/

#include <math.h>
#include <stdio.h>

#include "../c4_model.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void levels(void) {
    unsigned d, bad = 0, badt = 0;

    for (d = 0; d < 256u; d++) bad += c4_level[d] != (unsigned)floor(128.0 + 127.0 * cos(6.283185307179586 * (double)d / 256.0) + 0.5);
    for (d = 0; d <= 128u; d++) badt += c4_threshold[d] != (unsigned)floor(128.0 - 127.0 * cos(6.283185307179586 * (double)d / 256.0) + 1e-9);
    printf("  MODEL tables: %u of 256 levels and %u of 129 thresholds differ from the cosine in double\n", bad, badt);
    CHECK(bad == 0);
    CHECK(badt == 0);
}

static void window(void) {
    static const unsigned ws[3] = { 16u, 32u, 64u };
    unsigned i, r, m, bad = 0;

    for (i = 0; i < 3u; i++) {
        c4_set_t st;
        unsigned lit = 0;

        c4_set_init(&st, ws[i], 0u, C4_B0, 0.0f);
        for (r = 0; r < 256u; r++) {
            const unsigned q = c4_q((float)r, st.w, 2.0f);

            for (m = 0; m < 256u; m++) {
                const int delta = (int)((m - r + 2u + 256u + 128u) & 255u) - 128;    /* m - r + bias, as -128 .. 127 */
                const int in = delta > 1 && delta < (int)(2u * st.w) - 1;          /* inside the window, its two end codes left out */
                const int s = c4_shadow(&st, m, q);

                if (in && s) bad++;                                                /* the threshold is a whole number: an end may fall either way */
                if (!s) lit++;
            }
        }
        printf("  MODEL window w %u: threshold %u, %u codes of 256 read lit\n", st.w, st.thr, lit / 256u);
    }
    CHECK(bad == 0);
}

static float area(const c4_corner_t *p, unsigned n) {
    float a[3] = { 0.0f, 0.0f, 0.0f };
    unsigned i;

    for (i = 1; i + 1u < n; i++) {
        const float e1[3] = { p[i].p[0] - p[0].p[0], p[i].p[1] - p[0].p[1], p[i].p[2] - p[0].p[2] };
        const float e2[3] = { p[i + 1u].p[0] - p[0].p[0], p[i + 1u].p[1] - p[0].p[1], p[i + 1u].p[2] - p[0].p[2] };
        float c[3];

        sh4der_cross_(e1, e2, c);
        a[0] += c[0]; a[1] += c[1]; a[2] += c[2];
    }
    return 0.5f * sqrtf(c4_dot(a, a));
}

/* 1 when the three corners are one point: the same place in the world, or the same pixel position through m. */
static int equal3(const c4_corner_t *a, const c4_corner_t *b, const c4_corner_t *c, const float m[16]) {
    const c4_corner_t *v[3] = { a, b, c };
    float sx[3], sy[3];
    unsigned k;

    if (a->p[0] == b->p[0] && a->p[1] == b->p[1] && a->p[2] == b->p[2] && a->p[0] == c->p[0] && a->p[1] == c->p[1] && a->p[2] == c->p[2]) return 1;
    for (k = 0; k < 3u; k++) {
        const float *p = v[k]->p, w = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];

        sx[k] = (m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]) / w;
        sy[k] = (m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]) / w;
    }
    return sx[0] == sx[1] && sx[0] == sx[2] && sy[0] == sy[1] && sy[0] == sy[2];
}

/* The bands of every face that is compared. quiet: no line printed. Returns the triangles with three equal corners. */
static unsigned bands(const c4_light_t *L, const c4_set_t *st, int quiet) {
    const unsigned gs = st->gs;
    c4_face_t f[C4_FACES];
    c4_cam_t cam;
    float cm[16];
    unsigned i, k, j, nb = 0, nc = 0, lit_faces = 0, shared = 0, cracked = 0, equal = 0;

    c4_camera(&cam, C4_MASK_W, C4_MASK_H);
    c4_cam_matrix(&cam, cm);
    c4_faces(f, 0.0f);
    for (i = 0; i < C4_FACES; i++) {
        c4_corner_t face[4], out[C4_BAND_MAX], before[C4_BAND_MAX];
        float inv[4], lo = 1e30f, hi = -1e30f, sum = 0.0f;
        unsigned nbefore = 0;
        int b;

        if (c4_face_kind(L, f[i].n, st) != C4_FACE_COMPARE) continue;
        lit_faces++;
        c4_face_corners(L, &f[i], face, inv);
        for (k = 0; k < 4u; k++) { if (face[k].m[2] < lo) lo = face[k].m[2]; if (face[k].m[2] > hi) hi = face[k].m[2]; }
        for (b = c4_band(lo, gs); b <= c4_band(hi, gs); b++) {
            const float edge = (float)(b * (1 << gs)) - 0.5f;
            const unsigned n = c4_band_poly(face, inv, b, gs, out);

            for (k = 0; k < nbefore; k++) {                      /* the corners the band before left on this band's lower level */
                int found = 0;

                if (before[k].m[2] != edge) continue;
                shared++;
                for (j = 0; j < n; j++) found |= !memcmp(before[k].p, out[j].p, sizeof out[j].p);
                cracked += !found && n != 0u;
            }
            nbefore = n; memcpy(before, out, sizeof before);
            if (n < 3u) continue;
            nb++; nc += n;
            sum += area(out, n);
            for (k = 0; k < n; k++) CHECK(out[k].m[2] >= edge - 0.001f && out[k].m[2] <= edge + (float)(1 << gs) + 0.001f);
            for (k = 0; k + 2u < n; k++) {                       /* the strip's triangles, in the order the spike sends the corners */
                const c4_corner_t *t[3];

                for (j = 0; j < 3u; j++) { const unsigned q = k + j; t[j] = &out[(q & 1u) ? (q + 1u) / 2u : q ? n - q / 2u : 0u]; }
                equal += (unsigned)equal3(t[0], t[1], t[2], cm);
            }
        }
        CHECK(fabsf(sum - area(face, 4u)) < 1e-2f * area(face, 4u) + 1e-3f);
    }
    if (!quiet)
        printf("    bands %u codes wide: %u faces compared, cut into %u bands, %u corners; %u corners shared by two bands, %u not the same point; %u triangles of three equal corners\n",
               1u << gs, lit_faces, nb, nc, shared, cracked, equal);
    CHECK(cracked == 0);
    CHECK(equal == 0);
    if (!quiet) CHECK(shared > nb / 2u);
    return equal;
}

static void camera(const c4_light_t *L) {
    static uint8_t map[C4_MAP * C4_MAP];
    c4_cam_t cam;
    c4_set_t st;
    float m[16], worst = 0.0f;
    unsigned x, y, n = 0;

    c4_camera(&cam, C4_MASK_W, C4_MASK_H);
    c4_cam_matrix(&cam, m);
    c4_set_init(&st, 32u, 1u, C4_B0, 0.0f);
    memset(map, 255, sizeof map);
    for (y = 3; y < C4_MASK_H; y += 7u) {
        for (x = 2; x < C4_MASK_W; x += 9u) {
            const float ifo = 1.0f / cam.focal, sx = ((float)x + 0.5f - cam.cx) * ifo, sy = -((float)y + 0.5f - cam.cy) * ifo;
            const float d[3] = { cam.fwd[0] + sx * cam.right[0] + sy * cam.up[0], cam.fwd[1] + sx * cam.right[1] + sy * cam.up[1], cam.fwd[2] + sx * cam.right[2] + sy * cam.up[2] };
            float t, nrm[3], p[3], X, Y, W, e;
            unsigned face;

            if (!c4_ray(cam.eye, d, 0.0f, &t, nrm, &face)) continue;
            p[0] = cam.eye[0] + t * d[0]; p[1] = cam.eye[1] + t * d[1]; p[2] = cam.eye[2] + t * d[2];
            X = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
            Y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
            W = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
            CHECK(W > 1.0f);
            e = fabsf(X / W - ((float)x + 0.5f)); if (e > worst) worst = e;
            e = fabsf(Y / W - ((float)y + 0.5f)); if (e > worst) worst = e;
            n++;
        }
    }
    printf("  MODEL camera matrix: %u points, the farthest lands %.4f pixel from its ray's pixel\n", n, (double)worst);
    CHECK(n > 500u && worst < 0.02f);
    (void)L;
}

typedef struct tally {
    unsigned lit, sh, acne, edge, rim, inbias, other, wrap, steep, steep_sh;
} tally_t;

/* Every pixel of the mask through the model, for the light and the settings given. */
static void tally(const c4_light_t *L, const c4_set_t *st, tally_t *t) {
    static uint8_t map[C4_MAP * C4_MAP], ids[C4_MAP * C4_MAP];
    static const float up[3] = { 0.0f, 1.0f, 0.0f };
    const float b = c4_bias(L, up, st->b0, st->gs);                /* the ground's, as the yardstick */
    c4_cam_t cam;
    unsigned x, y;

    memset(t, 0, sizeof *t);
    c4_camera(&cam, C4_MASK_W, C4_MASK_H);
    c4_model_map(L, st, map, ids);
    for (y = 0; y < C4_MASK_H; y++) {
        for (x = 0; x < C4_MASK_W; x++) {
            c4_pixel_t px;
            const int m = c4_model_pixel(L, &cam, map, st, x, y, &px);

            if (m == C4_SKY) continue;
            if (px.kind == C4_FACE_STEEP) { t->steep++; t->steep_sh += px.truth == C4_SHADOW; }
            if (px.truth == C4_LIT) {
                t->lit++;
                if (m == C4_SHADOW && px.kind == C4_FACE_COMPARE) { if (ids[px.my * C4_MAP + px.mx] == px.face) t->acne++; else if (px.sep > 0.0f) t->edge++; else t->rim++; }
            } else {
                t->sh++;
                if (m == C4_LIT && px.kind == C4_FACE_COMPARE) { if (px.sep < b + 2.0f) t->inbias++; else t->other++; }
                if (px.sep > 256.0f - 2.0f * (float)st->w + b) t->wrap++;
            }
        }
    }
}

static void chain(float az, float el, unsigned w, float grow) {
    static const float up[3] = { 0.0f, 1.0f, 0.0f };
    c4_light_t L;
    unsigned gs;

    c4_light_fit(&L, az, el);
    printf("  MODEL sun azimuth %.0f elevation %.1f, window %u, faces grown %.2f: a code is %.1f cm of depth, a map pixel %.1f x %.1f cm\n", (double)az, (double)el, w, (double)grow,
           100.0 / (double)L.sd, 100.0 * (double)L.iu, 100.0 * (double)L.iv);
    for (gs = 0; gs < 3u; gs++) {
        c4_set_t st;
        tally_t t;

        c4_set_init(&st, w, gs, C4_B0, grow);
        bands(&L, &st, 0);
        tally(&L, &st, &t);
        printf("      ground bias %.1f codes; pixels lit %u, shadow %u; acne %u, edge %u, rim %u; lit in error: in bias %u, other %u; wrap %u; steep %u (%u of them in shadow)\n",
               (double)c4_bias(&L, up, st.b0, st.gs), t.lit, t.sh, t.acne, t.edge, t.rim, t.inbias, t.other, t.wrap, t.steep, t.steep_sh);
        CHECK(t.acne == 0);
        if (grow > 0.5f) CHECK(t.rim == 0);
        CHECK(t.lit > 10000u && t.sh > 500u);
    }
}

/* The face kinds: the ground is compared at every sun the spike allows; a wall 3 degrees off the sun is steep; a
   wall turned from it is away. And the two counts that must be 0, at every sun. */
static void kinds(void) {
    static const float up[3] = { 0.0f, 1.0f, 0.0f }, front[3] = { 0.0f, 0.0f, -1.0f }, right[3] = { 1.0f, 0.0f, 0.0f }, left[3] = { -1.0f, 0.0f, 0.0f };
    static const float els[3] = { 13.0f, 26.5f, 45.0f };
    unsigned e, a, worst_edge = 0, worst_steep = 0, worst_steep_sh = 0, acne = 0, rim = 0, equal = 0, suns = 0;
    c4_light_t L;
    c4_set_t st;

    c4_set_init(&st, 32u, 1u, C4_B0, 0.75f);
    for (e = 0; e < 2u; e++) {
        c4_light_fit(&L, 93.0f, e ? 80.0f : 8.0f);
        CHECK(c4_face_kind(&L, up, &st) == C4_FACE_COMPARE);
    }
    c4_light_fit(&L, 93.0f, 26.5f);
    CHECK(c4_face_kind(&L, front, &st) == C4_FACE_STEEP);
    CHECK(c4_face_kind(&L, right, &st) == C4_FACE_COMPARE);
    CHECK(c4_face_kind(&L, left, &st) == C4_FACE_AWAY);
    CHECK(c4_bias(&L, front, st.b0, st.gs) > (float)st.w - C4_FIT);
    for (e = 0; e < 3u; e++) {
        for (a = 0; a < 360u; a += 3u, suns++) {
            unsigned gs;
            tally_t t;

            c4_light_fit(&L, (float)a, els[e]);
            for (gs = 0; gs < 3u; gs++) { c4_set_t sb; c4_set_init(&sb, 32u, gs, C4_B0, 0.75f); equal += bands(&L, &sb, 1); }
            tally(&L, &st, &t);
            acne += t.acne; rim += t.rim;
            if (t.edge > worst_edge) worst_edge = t.edge;
            if (t.steep > worst_steep) worst_steep = t.steep;
            if (t.steep_sh > worst_steep_sh) worst_steep_sh = t.steep_sh;
        }
    }
    printf("  MODEL %u suns (azimuth by 3 degrees, elevations 13, 26.5, 45), window 32, bands 2 wide, grown 0.75: acne %u, rim %u in all;\n"
           "      the worst sun: edge %u; steep faces %u pixels, %u of them in shadow; triangles of three equal corners %u\n", suns, acne, rim, worst_edge, worst_steep, worst_steep_sh, equal);
    CHECK(acne == 0);
    CHECK(rim == 0);
    CHECK(equal == 0);
}

/* c4b: the window measured from a picture of known pairs, and Q made from it. */
static uint16_t e_sh[256], e_n[256];
static uint8_t e_tab[256], chip[256];

static void edges(void) {
    static const unsigned ws[3] = { 16u, 32u, 64u };
    static const float suns[2][2] = { { 150.0f, 26.5f }, { 100.0f, 13.0f } };
    unsigned i, d, k, badq = 0;
    int lo, hi, lo_n, hi_n, mixed;
    c4_set_t st;

    for (i = 0; i < 3u; i++) {
        c4_set_init(&st, ws[i], 0u, C4_B0, 0.0f);
        for (k = 0; k < 2048u; k++) {
            const float r = (float)k * 0.125f, b = (float)(k % 41u) * 0.25f;

            badq += c4_q_set(&st, r, b) != c4_q(r, st.w, b);
        }
    }
    printf("  MODEL c4b: Q from the window's lower edge against the old Q, 6144 pairs: %u differ\n", badq);
    CHECK(badq == 0);

    c4_set_init(&st, 32u, 1u, 1.5f, 0.75f);
    for (d = 0; d < 256u; d++) { e_n[d] = 160u; e_sh[d] = c4_level[d] >= st.thr ? 160u : 0u; }
    mixed = c4_edges(e_sh, e_n, &lo, &hi, e_tab);
    for (lo_n = 128; lo_n > 0 && c4_level[lo_n - 1] < st.thr; lo_n--) { }
    for (hi_n = 128; hi_n < 255 && c4_level[hi_n + 1] < st.thr; hi_n++) { }
    printf("  MODEL c4b: the cosine table at window 32 (threshold %u): edges found %d .. %d, the table's %d .. %d, mixed %d\n", st.thr, lo, hi, lo_n, hi_n, mixed);
    CHECK(lo == lo_n && hi == hi_n && mixed == 0);
    for (d = 0; d < 256u; d++) CHECK(e_tab[d] == (c4_level[d] >= st.thr));

    for (d = 0; d < 256u; d++) chip[d] = (uint8_t)(c4_level[d] >= st.thr || (int)d < lo_n + 3 || (int)d > hi_n - 3);   /* 3 codes narrower at each end */
    for (d = 0; d < 256u; d++) { e_n[d] = 160u; e_sh[d] = chip[d] ? 160u : 0u; }
    e_sh[lo_n + 3] = 40u;                                          /* one difference read both ways */
    mixed = c4_edges(e_sh, e_n, &lo, &hi, e_tab);
    printf("  MODEL c4b: a chip 3 codes narrower at each end, one difference mixed: edges found %d .. %d, mixed %d\n", lo, hi, mixed);
    CHECK(lo == lo_n + 4 && hi == hi_n - 3 && mixed == 1);

    for (k = 0; k < 2u; k++) {
        c4_light_t L;
        tally_t ta, tb;
        c4_set_t sa, sb;

        c4_light_fit(&L, suns[k][0], suns[k][1]);
        c4_set_init(&sa, 32u, 1u, 1.5f, 0.75f); c4_set_edges(&sa, 128 - 32, 128 + 32, chip);   /* Q from the cosine's edge */
        c4_set_init(&sb, 32u, 1u, 1.5f, 0.75f); c4_set_edges(&sb, lo, hi, chip);               /* Q from the measured edge */
        tally(&L, &sa, &ta);
        tally(&L, &sb, &tb);
        printf("  MODEL c4b sun %.0f / %.1f, b0 1.5, on that chip: acne %u with Q from the cosine's edge, %u with Q from the measured edge (edge %u / %u, real shadow shown lit %u / %u)\n",
               (double)suns[k][0], (double)suns[k][1], ta.acne, tb.acne, ta.edge, tb.edge, ta.inbias + ta.other, tb.inbias + tb.other);
        CHECK(tb.acne == 0);
        CHECK(ta.acne > tb.acne);
    }
}

int main(void) {
    c4_light_t L;

    levels();
    window();
    c4_light_fit(&L, 150.0f, 26.5f);
    camera(&L);
    chain(150.0f, 26.5f, 32u, 0.0f);
    chain(150.0f, 26.5f, 32u, 0.75f);
    chain(150.0f, 26.5f, 64u, 0.75f);
    chain(210.0f, 45.0f, 32u, 0.75f);
    chain(177.0f, 26.5f, 32u, 0.75f);
    chain(93.0f, 26.5f, 32u, 0.75f);
    chain(100.0f, 13.0f, 32u, 0.0f);
    chain(100.0f, 13.0f, 32u, 0.75f);
    chain(100.0f, 13.0f, 64u, 0.75f);
    kinds();
    edges();
    printf(fails ? "c4_model_test: %d FAILED\n" : "c4_model_test: ok\n", fails);
    return fails != 0;
}
