/* Host test of demo/psm/psm_shtile.h and psm_ramp.h: the ramp, the town's layout, the take and give-back rule, the strip
   along the sun, the pass transform against the ground quad, the feather weight, the clip and the cut rectangle. */
#include <math.h>
#include <stdio.h>

#include "psm_shtile.h"

static int failed;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failed++; } } while (0)

static void sun_at(float az_deg, float el_deg, float s[3]) {
    const float a = az_deg * 0.0174533f, e = el_deg * 0.0174533f;

    s[0] = sinf(a) * cosf(e); s[1] = sinf(e); s[2] = -cosf(a) * cosf(e);
}

#define N_TILES 12u

int main(void) {
    static const uint8_t kind_of[N_TILES] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 };
    sht_tile_t t[N_TILES];
    sht_caster_t c[13];
    float sun[3], eye[3] = { 0.0f, 1.6f, 0.0f }, m[8];
    unsigned k, j, cw, ch, live;
    psm_ramp_t r;

    /* the ramp: both forms agree, the ends are exact */
    psm_ramp_make(&r, SHT_FADE_START, SHT_FADE_END);
    CHECK(psm_ramp_down(&r, 0.0f) == 1.0f && psm_ramp_down(&r, 40.0f) == 1.0f && psm_ramp_down(&r, 50.0f) == 0.0f && psm_ramp_down(&r, 80.0f) == 0.0f);
    CHECK(fabsf(psm_ramp_down(&r, 45.0f) - 0.5f) < 1e-5f && fabsf(psm_ramp_down(&r, 42.5f) - 0.75f) < 1e-5f);
    for (k = 0; k <= 60u; k++) CHECK(fabsf(psm_ramp_up(&r, (float)k) - psm_ramp_in(40.0f, 10.0f, (float)k)) < 1e-5f);
    CHECK(psm_ramp_in(5.0f, 0.0f, 4.9f) == 0.0f && psm_ramp_in(5.0f, 0.0f, 5.0f) == 1.0f);

    /* the town's layout: 8 strips of 128 x 32 and 4 squares of 32 in 256 x 256, no overlap, on 32s, 40 tiles to cut */
    CHECK(sht_layout(t, N_TILES, kind_of, sht_kinds_default, 256u, 256u));
    for (k = 0; k < N_TILES; k++) {
        CHECK(t[k].x % SHT_UNIT == 0u && t[k].y % SHT_UNIT == 0u && t[k].w % SHT_UNIT == 0u && t[k].h % SHT_UNIT == 0u);
        CHECK(t[k].x + t[k].w <= 256u && t[k].y + t[k].h <= 256u && t[k].caster == -1);
        for (j = 0; j < k; j++)
            CHECK(t[j].x + t[j].w <= t[k].x || t[k].x + t[k].w <= t[j].x || t[j].y + t[j].h <= t[k].y || t[k].y + t[k].h <= t[j].y);
    }
    CHECK(t[7].x == 128u && t[7].y == 96u && t[8].x == 0u && t[8].y == 128u && t[11].x == 96u);
    CHECK(!sht_layout(t, N_TILES, kind_of, sht_kinds_default, 256u, 128u));          /* the hens do not fit: said, not hidden */
    CHECK(sht_layout(t, N_TILES, kind_of, sht_kinds_default, 256u, 256u));

    /* 8 people and 4 hens near the camera at 14:00: every one has a tile; strength grows in over SHT_GROW_S */
    sun_at(213.77f, 26.54f, sun);
    for (k = 0; k < 12u; k++) {
        c[k].id = 100 + (int)k; c[k].kind = k < 8u ? SHT_KIND_PERSON : SHT_KIND_HEN;
        c[k].foot[0] = -8.0f + 1.7f * (float)k; c[k].foot[1] = 0.0f; c[k].foot[2] = -6.0f - 0.9f * (float)k;
        c[k].height = k < 8u ? 1.70f : 0.37f; c[k].radius = k < 8u ? 0.25f : 0.20f; c[k].base_y = 0.0f;
    }
    live = sht_assign(t, N_TILES, sht_kinds_default, c, 12u, eye, sun, 0.1f);
    CHECK(live == 12u);
    for (k = 0; k < N_TILES; k++) CHECK(t[k].caster >= 100 && fabsf(t[k].grow - 0.2f) < 1e-5f && fabsf(t[k].strength - 0.2f) < 1e-5f && t[k].fade == 1.0f);
    for (j = 0; j < 6u; j++) live = sht_assign(t, N_TILES, sht_kinds_default, c, 12u, eye, sun, 0.1f);
    CHECK(live == 12u);
    for (k = 0; k < N_TILES; k++) CHECK(t[k].strength == 1.0f && t[k].kind == (k < 8u ? 0u : 1u));
    CHECK(sht_cut(t, N_TILES, &cw, &ch) && cw == 8u && ch == 5u);                   /* 40 tiles of 32 */

    /* the strip lies along the sun: its long axis points away from it, it starts a radius behind the feet and holds the
       shadow's tip; the pass maps the feet and the tip inside the tile and the ground quad agrees with the pass */
    for (k = 0; k < N_TILES; k++) {
        const sht_caster_t *cs = &c[t[k].caster - 100];
        const float hl = sqrtf(sun[0] * sun[0] + sun[2] * sun[2]), run = cs->height * hl / sun[1];
        const float tip[3] = { cs->foot[0] - sun[0] / hl * run, 0.0f, cs->foot[2] - sun[2] / hl * run };
        const float top[3] = { cs->foot[0], cs->height, cs->foot[2] };
        float pos[4][3], uv[4][2];
        sht_pv_t a, b, q;

        CHECK(!t[k].cut);
        CHECK(t[k].ax[0] * sun[0] + t[k].ax[1] * sun[2] < 0.0f && fabsf(t[k].ax[0] * t[k].ay[0] + t[k].ax[1] * t[k].ay[1]) < 1e-5f);
        CHECK(fabsf(sqrtf(t[k].ay[0] * t[k].ay[0] + t[k].ay[1] * t[k].ay[1]) - 0.5f * sht_kinds_default[t[k].kind].across) < 1e-4f);
        sht_matrix(&t[k], sun, 0.0f, m);
        sht_apply(m, cs->foot, &a);
        sht_apply(m, top, &b);                     /* the top, slid along the sun: the shadow's tip */
        sht_apply(m, tip, &q);
        CHECK(fabsf(b.x - q.x) < 0.02f && fabsf(b.y - q.y) < 0.02f);
        CHECK(sht_inside(&t[k], &a) && sht_inside(&t[k], &b));
        CHECK(fabsf(a.y - ((float)t[k].y + 0.5f * (float)t[k].h)) < 0.02f && b.x > a.x);   /* on the strip's centre line, the tip further along */
        sht_quad(&t[k], 0.0f, 0.03f, 256u, 256u, pos, uv);
        for (j = 0; j < 4u; j++) {                 /* each corner of the ground quad is the pixel the pass puts that ground point at */
            const float g[3] = { pos[j][0], 0.0f, pos[j][2] };

            sht_apply(m, g, &q);
            CHECK(fabsf(q.x - uv[j][0] * 256.0f) < 0.02f && fabsf(q.y - uv[j][1] * 256.0f) < 0.02f && pos[j][1] == 0.03f);
            CHECK(q.x >= (float)t[k].x + 0.98f && q.x <= (float)(t[k].x + t[k].w) - 0.98f);
        }
        if (k == 0u) printf("person at 14:00: strip %.2f x %.2f m, %.1f cm a texel along, %.1f across\n",
                            (double)(2.0f * sqrtf(t[k].ax[0] * t[k].ax[0] + t[k].ax[1] * t[k].ax[1])), (double)sht_kinds_default[0].across,
                            (double)(200.0f * sqrtf(t[k].ax[0] * t[k].ax[0] + t[k].ax[1] * t[k].ax[1]) / 126.0f), (double)(100.0f / 30.0f));
    }

    /* the weight: 1 well inside, 0 on the border line, under 0 outside, linear between */
    CHECK(sht_weight(&t[0], 64.0f, 16.0f) == 1.0f && fabsf(sht_weight(&t[0], 1.0f, 16.0f)) < 1e-6f && sht_weight(&t[0], 0.0f, 16.0f) < 0.0f);
    CHECK(fabsf(sht_weight(&t[0], 5.0f, 16.0f) - 0.5f) < 1e-6f && fabsf(sht_weight(&t[0], 127.0f, 16.0f)) < 1e-6f);
    {
        const float shadow[3] = { 0.4f, 0.5f, 0.6f };

        CHECK(sht_argb(&t[0], shadow, 0.0f) == 0xffffffffu && sht_argb(&t[0], shadow, -3.0f) == 0xffffffffu);
        CHECK(sht_argb(&t[0], shadow, 1.0f) == (0xff000000u | (102u << 16) | (128u << 8) | 153u));
        t[0].strength = 0.5f;
        CHECK(sht_argb(&t[0], shadow, 1.0f) == (0xff000000u | (179u << 16) | (191u << 8) | 204u));
        t[0].strength = 1.0f;
    }

    /* a low sun (16:00): the person's shadow is 7.4 m, the strip stays 4 m and says so; the clip keeps every vertex in the
       tile and the weight on the cut edge is 0 or less */
    sun_at(242.11f, 12.92f, sun);
    live = sht_assign(t, N_TILES, sht_kinds_default, c, 12u, eye, sun, 0.1f);
    CHECK(live == 12u && t[0].cut && t[8].cut);
    CHECK(fabsf(2.0f * sqrtf(t[0].ax[0] * t[0].ax[0] + t[0].ax[1] * t[0].ax[1]) - 4.0f) < 1e-3f);
    {
        const sht_caster_t *cs = &c[t[0].caster - 100];
        const float p0[3] = { cs->foot[0] - 0.2f, 0.0f, cs->foot[2] }, p1[3] = { cs->foot[0] + 0.2f, 0.0f, cs->foot[2] };
        const float p2[3] = { cs->foot[0], cs->height, cs->foot[2] };
        sht_pv_t in[3], out[7];
        unsigned n;

        sht_matrix(&t[0], sun, 0.0f, m);
        sht_apply(m, p0, &in[0]); sht_apply(m, p1, &in[1]); sht_apply(m, p2, &in[2]);
        for (j = 0; j < 3u; j++) in[j].w = sht_weight(&t[0], in[j].x, in[j].y);
        CHECK(sht_inside(&t[0], &in[0]) && sht_inside(&t[0], &in[1]) && !sht_inside(&t[0], &in[2]) && in[2].w < 0.0f);
        n = sht_clip(&t[0], in, out);
        CHECK(n == 4u);
        for (j = 0; j < n; j++) {
            CHECK(out[j].x >= (float)t[0].x + 1.0f - 1e-3f && out[j].x <= (float)(t[0].x + t[0].w) - 1.0f + 1e-3f);
            CHECK(out[j].y >= (float)t[0].y + 1.0f - 1e-3f && out[j].y <= (float)(t[0].y + t[0].h) - 1.0f + 1e-3f);
            if (out[j].x > (float)(t[0].x + t[0].w) - 1.5f) CHECK(out[j].w <= 1e-3f);          /* on the cut: white */
        }
        in[2] = in[0]; in[2].y += 3.0f;                                                       /* wholly inside: unchanged */
        CHECK(sht_clip(&t[0], in, out) == 3u && out[0].x == in[0].x && out[2].y == in[2].y);
        in[0].x += 500.0f; in[1].x += 500.0f; in[2].x += 500.0f;                             /* wholly outside: nothing */
        CHECK(sht_clip(&t[0], in, out) == 0u);
    }

    /* the fade and the give-back rule. Caster 0 walks out: full to 40 m, half at 45 m, kept (strength above 0) to 50 m,
       given back there; a 13th caster waiting inside the range gets nothing while all are held, takes the freed tile at
       grow 0 and grows in */
    sun_at(213.77f, 26.54f, sun);
    c[12] = c[0]; c[12].id = 200; c[12].foot[0] = 3.0f; c[12].foot[2] = -3.0f;
    for (j = 0; j < 10u; j++) sht_assign(t, N_TILES, sht_kinds_default, c, 13u, eye, sun, 0.1f);
    for (k = 0; k < N_TILES; k++) CHECK(t[k].caster != 200);
    c[0].foot[0] = 0.0f; c[0].foot[2] = -45.0f;
    sht_assign(t, N_TILES, sht_kinds_default, c, 13u, eye, sun, 0.1f);
    CHECK(t[0].caster == 100 && fabsf(t[0].strength - 0.5f) < 5e-3f);
    c[0].foot[2] = -49.9f;
    sht_assign(t, N_TILES, sht_kinds_default, c, 13u, eye, sun, 0.1f);
    CHECK(t[0].caster == 100 && t[0].strength > 0.0f && t[0].strength < 0.02f);
    for (k = 0; k < N_TILES; k++) CHECK(t[k].caster != 200);                                 /* a visible shadow is never taken */
    c[0].foot[2] = -50.5f;
    live = sht_assign(t, N_TILES, sht_kinds_default, c, 13u, eye, sun, 0.1f);
    CHECK(t[0].caster == 200 && fabsf(t[0].grow - 0.2f) < 1e-5f && fabsf(t[0].strength - 0.2f) < 1e-5f && live == 12u);
    c[0].foot[2] = -45.0f;                                                                   /* back inside: no tile is free, it waits */
    sht_assign(t, N_TILES, sht_kinds_default, c, 13u, eye, sun, 0.1f);
    for (k = 0; k < N_TILES; k++) CHECK(t[k].caster != 100);
    live = sht_assign(t, N_TILES, sht_kinds_default, c + 1, 12u, eye, sun, 0.1f);            /* still 12 casters, 12 tiles */
    CHECK(live == 12u);
    live = sht_assign(t, N_TILES, sht_kinds_default, c + 1, 7u, eye, sun, 0.1f);             /* casters gone from the list: their tiles are free */
    CHECK(live == 7u);
    CHECK(sht_cut(t, N_TILES, &cw, &ch) && cw == 8u && ch == 4u);                           /* the hens' row is no longer cut or cleared */

    /* a sun too low: nothing is held, no pass */
    sun_at(250.0f, 4.0f, sun);
    live = sht_assign(t, N_TILES, sht_kinds_default, c, 13u, eye, sun, 0.1f);
    CHECK(live == 0u && sht_cut(t, N_TILES, &cw, &ch) && cw == 0u && ch == 0u);
    for (k = 0; k < N_TILES; k++) CHECK(t[k].caster == -1);

    /* a car tile is the same entry: four 128 squares in the right half of a 512 x 256 target would be the caller's offset */
    {
        static const uint8_t cars[4] = { SHT_KIND_CAR, SHT_KIND_CAR, SHT_KIND_CAR, SHT_KIND_CAR };
        sht_tile_t ct[4];

        CHECK(sht_layout(ct, 4u, cars, sht_kinds_default, 256u, 256u) && ct[3].x == 128u && ct[3].y == 128u && ct[3].w == 128u && ct[3].h == 128u);
        CHECK(!sht_layout(ct, 4u, cars, sht_kinds_default, 256u, 128u));
    }

    printf(failed ? "psm_shtile_test: %d FAILED\n" : "psm_shtile_test: ok\n", failed);
    return failed != 0;
}
