/* The demo scene. World: right-handed, Y up, metres; the street runs along +Z and repeats every PERIOD.
 * Frame: (1) the world mirrored about the road plane into the reflection target (pl4nar), (2) the screen: opaque world +
 * car, then one translucent layer on the car's paint and glass with the target as its texture and Fresnel as vertex alpha.
 * SH4ZAM for all maths: XMTRX chains (screen x perspective x lookat x translate x rotate), FTRV per vertex, shz_sincosf,
 * shz_vec3_*, shz_lerpf / shz_saturatef / shz_fmaf. Hand arithmetic is scalar a * b + c only (named where it is). */
#include <stdio.h>
#include <string.h>
#include "demo.h"
#include "gfx.h"
#include "hud.h"
#include "assets.h"
#include "assets_gen.h"
#include "sh4der_pl4nar.h"
#include "sh4der_spark.h"
#include "sh4der_pool.h"

/* ------------------------------------------------------------------------------------------------ world numbers */
#define ROAD_HALF    8.0f          /* asphalt half width */
#define BLD_X        12.0f         /* building line (asphalt + 4 m of sidewalk): the road texture's half width */
#define BLOCK        96.0f
#define NBLOCK       4
#define PERIOD       (BLOCK * NBLOCK)
#define CROSS        12.0f         /* cross street width */
#define ROAD_TILE    24.0f         /* the road texture is 24 m x 24 m */
#define FACADE_TILE  12.8f         /* a facade texture is 4 bays x 4 floors of 3.2 m */
#define DRAW_BACK    48.0f
#define DRAW_FWD     336.0f
#define LANE_MAX     5.2f          /* the car's |x| limit: the kerb lane stays free for parked cars */
#define MAX_BLD      112
#define MAX_PROP     64
#define SCRATCH_N    MDL_MAX_NV    /* the largest model of the bake */
#define FOV_MAIN     0.96f         /* 55 degrees, vertical */
#define CAM_DIST     7.0f
#define CAM_HEIGHT   3.3f
static float s_amb = 0.56f, s_dif = 0.50f;        /* the light of the hour (tod_eval) */
#define AMBIENT      s_amb
#define DIFFUSE      s_dif
#define HAZE_START   90.0f
#define HAZE_INV     (1.0f / 320.0f)
#define HAZE_MAX     0.88f
static uint32_t s_haze = 0xffc4ced6u;
#define COL_HAZE     s_haze
#define COL_LAMP     0x00ffc070u   /* street lamps: sodium */
#define COL_HEAD     0x00fff0c8u   /* headlights */
#define COL_TAIL     0x00d81818u
#define LAMP_NEAR    60.0f         /* street lamps light the road and glow within this of the car */
#define LAMP_MIRROR  40.0f         /* and are drawn in the mirror pass within this */
#define LAMP_HEAD_Y  7.45f
#define LAMP_ARM     2.6f          /* the lamp head hangs this far out over the road */
#define COL_ASPHALT  0xff383836u
#define COL_ROOF     0xff4a4a50u
#define COL_GROUND_REFLECT 0xff2c2c2eu   /* what a ray into the road sees: dark asphalt */
#define COL_GLASS    0xff1c2430u

typedef struct bld { float x0, x1, z0, z1, h; uint8_t mat, side, pad[2]; } bld_t;
typedef struct prop { const mdl_t *body, *wheels; float x, z, yaw; } prop_t;

static bld_t   s_bld[MAX_BLD];
static unsigned s_nbld;
static prop_t  s_prop[MAX_PROP];
static unsigned s_nprop;
static float   s_dome[5][12][3];                 /* unit directions: 4 rings of 12 + the zenith in [4][0] */
static uint32_t s_dome_col[5] = { 0xff8c949cu, 0xffb8cadcu, 0xff86aedau, 0xff4a80c8u, 0xff2c58a8u };
static float s_sun[3] = { 0.4472f, 0.6200f, 0.6442f };           /* unit: ahead, right-hand side of the street, 38 degrees up by day */

/* The hour. One clock (0 .. 1, a 45 s loop) drives the sky, the haze, the light's colour and strength, the sun's
   height and the lamps. No layer: the tint goes into the vertex colours the frame computes anyway. */
typedef struct tod_key { uint32_t dome[5], haze, light, sunc; float amb, dif, elev, night; } tod_key_t;
static const tod_key_t s_tod_key[3] = {
    { { 0xff8c949cu, 0xffb8cadcu, 0xff86aedau, 0xff4a80c8u, 0xff2c58a8u }, 0xffc4ced6u, 0xffffffffu, 0xfffffcecu, 0.56f, 0.50f, 38.0f, 0.0f },   /* day */
    { { 0xff6a5450u, 0xffe89a5cu, 0xffb47c7cu, 0xff5c5c92u, 0xff2a3068u }, 0xffb48c70u, 0xffffc8a0u, 0xffffb060u, 0.46f, 0.44f, 7.0f, 0.55f },    /* dusk, dawn */
    { { 0xff0a0e16u, 0xff141c2eu, 0xff0e1628u, 0xff080e1eu, 0xff040814u }, 0xff0e141eu, 0xff6878a0u, 0xffd0d8ecu, 0.30f, 0.12f, 30.0f, 1.0f }     /* night (the disc is the moon) */
};
static const struct { float t; unsigned key; } s_tod_knot[7] = { { 0.0f, 0u }, { 0.35f, 0u }, { 0.45f, 1u }, { 0.55f, 2u }, { 0.85f, 2u }, { 0.93f, 1u }, { 1.0f, 0u } };
static const char *const s_tod_name[4] = { "AUTO", "DAY", "NIGHT", "PAUSE" };
static float    s_tod = 0.10f;                    /* the clock */
static unsigned s_tod_mode;                       /* 0 AUTO, 1 DAY, 2 NIGHT, 3 PAUSE */
static uint32_t s_light = 0xffffffffu, s_sunc = 0xfffffcecu, s_sun_sky = 0xff6e9cccu, s_road_col = 0xffffffffu;
static float    s_night, s_lamp;                  /* 0 day .. 1 night; the lamps' ramp */
static unsigned s_lamp_k;                         /* s_lamp as 0 .. 256 */
static float    s_lamp_gain = 2.0f;               /* reflection layer gain at night (the lamps on the paint) */
static sh4der_spark_view_t s_sv;                  /* this pass's camera for the halos */

static gfx_vert_t s_scr[SCRATCH_N];               /* model scratch */
/* sized by the bake (assets_gen.h): a model that outgrows an array is refused, never written past it */
static gfx_vert_t s_body[MDL_PLAYER_BODY_NV];     /* the player's body, opaque pass */
static gfx_vert_t s_refl[MDL_PLAYER_BODY_NV];     /* the same vertices for the reflection layer */
static float      s_wp[MDL_PLAYER_BODY_NV][3], s_wn[MDL_PLAYER_BODY_NV][3], s_wsn[MDL_PLAYER_BODY_NV][3];   /* world: position, flat normal (light), smoothed normal (reflection) */
static unsigned   s_refused;                      /* models refused for size: logged once each kind */

/* car + camera */
static struct { float x, z, h, v, cam_h, orbit, clock; } s_car;
static int      s_reflect = 1, s_info = 1, s_auto;
static int      s_low = 1;                         /* the low look: 64 x 64, 16-colour, point sampled textures (A switches) */
static unsigned s_size;                            /* index into s_sizes */
static float    s_f0 = 0.14f;                      /* paint F0: 0.05 is physical, the default leans on the clear coat */
static const uint16_t s_sizes[3][2] = { { 256, 128 }, { 128, 64 }, { 512, 256 } };
static uint32_t s_refl_cpu_us, s_tris_main, s_tris_refl;

/* per pass */
static shz_vec4_t s_vp[4];                         /* rows of screen x projection x view (x mirror) of the pass being drawn */
#define VP_LOAD() shz_xmtrx_load_rows_4x4(&s_vp[0], &s_vp[1], &s_vp[2], &s_vp[3])
static float s_vw, s_vh, s_focal, s_camz;
static unsigned s_haze_max = (unsigned)(HAZE_MAX * 256.0f);   /* this pass: the reflection pass keeps more contrast (less haze) */
static sh4der_pl4nar_view_t s_pv;
static int s_pv_ok;                               /* this frame's reflection view was built: the layer may be drawn */
static const sh4der_pl4nar_plane_t s_road_plane = { { 0.0f, 1.0f, 0.0f }, 0.0f };

int demo_reflect_on(void) { return s_reflect; }

/* ------------------------------------------------------------------------------------------------ init */
static uint32_t s_rng = 0x1234abcdu;
static unsigned rnd(unsigned n) { s_rng = s_rng * 1664525u + 1013904223u; return (s_rng >> 10) % n; }

static void add_bld(float x0, float x1, float z0, float z1, float h, unsigned mat, unsigned side) {
    if (s_nbld < MAX_BLD) { bld_t *b = &s_bld[s_nbld++]; b->x0 = x0; b->x1 = x1; b->z0 = z0; b->z1 = z1; b->h = h; b->mat = (uint8_t)mat; b->side = (uint8_t)side; }
}
static void add_prop(const mdl_t *body, const mdl_t *wheels, float x, float z, float yaw) {
    if (s_nprop < MAX_PROP) { prop_t *p = &s_prop[s_nprop++]; p->body = body; p->wheels = wheels; p->x = x; p->z = z; p->yaw = yaw; }
}

void demo_init(void) {
    static const float ring_deg[4] = { -9.0f, 0.0f, 13.0f, 38.0f };
    unsigned side, b, r, k;
    for (r = 0; r < 4u; r++)
        for (k = 0; k < 12u; k++) {
            const shz_sincos_t e = shz_sincosf_deg(ring_deg[r]), a = shz_sincosf_deg(30.0f * (float)k);
            s_dome[r][k][0] = e.cos * a.sin; s_dome[r][k][1] = e.sin; s_dome[r][k][2] = e.cos * a.cos;    /* hand: scalar products */
        }
    s_dome[4][0][0] = 0.0f; s_dome[4][0][1] = 1.0f; s_dome[4][0][2] = 0.0f;
    for (side = 0; side < 2u; side++) {
        const float sgn = side ? 1.0f : -1.0f;
        for (b = 0; b < NBLOCK; b++) {
            float z = (float)b * BLOCK + CROSS * 0.5f;
            const float end = (float)(b + 1u) * BLOCK - CROSS * 0.5f;
            while (z < end - 0.1f) {
                float w = 12.8f + 3.2f * (float)rnd(5u);
                const float depth = 14.0f + 2.0f * (float)rnd(5u), h = 3.2f * (float)(3u + rnd(11u));
                if (end - (z + w) < 12.8f) w = end - z;
                if (side) add_bld(BLD_X, BLD_X + depth, z, z + w, h, rnd(4u), side);
                else      add_bld(-BLD_X - depth, -BLD_X, z, z + w, h, rnd(4u), side);
                z += w;
            }
            /* a building across the far end of each cross street closes the view */
            add_bld(sgn * 96.0f - 10.0f, sgn * 96.0f + 10.0f, (float)(b + 1u) * BLOCK - 22.0f, (float)(b + 1u) * BLOCK + 22.0f,
                    3.2f * (float)(6u + rnd(6u)), rnd(4u), side);
            /* street furniture: lamps both sides, poles (wires) on the right-hand side, a signal at the cross street */
            for (k = 0; k < 3u; k++)
                add_prop(&mdl_lamp, NULL, sgn * (ROAD_HALF + 0.9f), (float)b * BLOCK + 16.0f + 32.0f * (float)k, side ? 1.5708f : -1.5708f);
            if (side == 0u)
                for (k = 0; k < 3u; k++) add_prop(&mdl_pole, NULL, -(BLD_X - 0.7f), (float)b * BLOCK + 32.0f * (float)k + 30.0f, 1.5708f);
            add_prop(&mdl_signal, NULL, sgn * (ROAD_HALF + 0.6f), (float)(b + 1u) * BLOCK - CROSS * 0.5f - 1.0f, side ? 3.1416f : 0.0f);
        }
    }
    /* parked cars in the kerb lanes (the left side of a right-handed +Z street is +X) */
    add_prop(&mdl_taxi_body, &mdl_taxi_wheels, 6.7f, 40.0f, 0.0f);
    add_prop(&mdl_van_body, &mdl_van_wheels, -6.7f, 70.0f, 3.1416f);
    add_prop(&mdl_police_body, &mdl_police_wheels, 6.7f, 150.0f, 0.0f);
    add_prop(&mdl_taxi_body, &mdl_taxi_wheels, -6.7f, 215.0f, 3.1416f);
    add_prop(&mdl_van_body, &mdl_van_wheels, 6.7f, 262.0f, 0.0f);
    add_prop(&mdl_police_body, &mdl_police_wheels, -6.7f, 330.0f, 3.1416f);
    s_car.x = -2.0f; s_car.z = 8.0f; s_car.h = 0.0f; s_car.v = 0.0f; s_car.cam_h = 0.0f; s_car.orbit = 0.0f; s_car.clock = 0.0f;
    gfx_reflect_resize(s_sizes[s_size][0], s_sizes[s_size][1]);
}

/* ------------------------------------------------------------------------------------------------ update */
void demo_update(const demo_input_t *in, float dt) {
    float rate;
    if (in->pressed & DEMO_BTN_REFLECT) s_reflect = !s_reflect;
    if (in->pressed & DEMO_BTN_INFO) s_info = !s_info;
    if (in->pressed & DEMO_BTN_AUTO) s_auto = !s_auto;
    if (in->pressed & DEMO_BTN_MORE) s_f0 = shz_fminf(s_f0 + 0.02f, 0.60f);
    if (in->pressed & DEMO_BTN_LESS) s_f0 = shz_fmaxf(s_f0 - 0.02f, 0.02f);
    if (in->pressed & DEMO_BTN_LAMP_UP) s_lamp_gain = shz_fminf(s_lamp_gain + 0.25f, 4.0f);
    if (in->pressed & DEMO_BTN_LAMP_DN) s_lamp_gain = shz_fmaxf(s_lamp_gain - 0.25f, 0.5f);
    if (in->pressed & DEMO_BTN_LOOK) s_low = !s_low;
    if (in->pressed & DEMO_BTN_TOD) s_tod_mode = (s_tod_mode + 1u) % 4u;
    if (s_tod_mode == 0u) s_tod += dt * (1.0f / 45.0f);
    else if (s_tod_mode < 3u) {                    /* DAY, NIGHT: the clock runs the short way round to that hour, 0.55 of a turn in 1.6 s */
        float d = (s_tod_mode == 1u ? 0.15f : 0.70f) - s_tod;
        if (d > 0.5f) d -= 1.0f;
        if (d < -0.5f) d += 1.0f;
        s_tod += shz_clampf(d, -dt * 0.34f, dt * 0.34f);
    }
    if (s_tod >= 1.0f) s_tod -= 1.0f;
    if (s_tod < 0.0f) s_tod += 1.0f;
    if (in->pressed & DEMO_BTN_SIZE) {
        s_size = (s_size + 1u) % 3u;
        gfx_reflect_resize(s_sizes[s_size][0], s_sizes[s_size][1]);
    }
    /* arcade car: each step is a x b + c, one FMAC (shz_fmaf); the rates are constants x dt */
    if (in->gas > 0.05f)        s_car.v = shz_fmaf(in->gas, 13.0f * dt, s_car.v);
    if (in->brake > 0.05f)      s_car.v = shz_fmaf(in->brake, -(s_car.v > 0.5f ? 26.0f : 7.0f) * dt, s_car.v);
    s_car.v = shz_fmaf(s_car.v, -0.22f * dt, s_car.v);
    s_car.v = shz_clampf(s_car.v, -8.0f, 46.0f);
    rate = 1.25f * shz_saturatef(shz_fabsf(s_car.v) * (1.0f / 5.0f)) * shz_invf(1.0f + shz_fabsf(s_car.v) * (1.0f / 28.0f));
    s_car.h = shz_fmaf(in->steer * rate, s_car.v < 0.0f ? dt : -dt, s_car.h);   /* right-handed: a right turn lowers the heading */
    s_car.h = shz_clampf(s_car.h, -0.85f, 0.85f);
    {
        const shz_sincos_t sc = shz_sincosf(s_car.h);
        s_car.x = shz_fmaf(sc.sin, s_car.v * dt, s_car.x);
        s_car.z = shz_fmaf(sc.cos, s_car.v * dt, s_car.z);
    }
    if (s_car.x > LANE_MAX)  { s_car.x = LANE_MAX;  if (s_car.h > 0.0f) s_car.h -= s_car.h * shz_saturatef(4.0f * dt); }
    if (s_car.x < -LANE_MAX) { s_car.x = -LANE_MAX; if (s_car.h < 0.0f) s_car.h -= s_car.h * shz_saturatef(4.0f * dt); }
    if (s_car.z >= PERIOD) s_car.z -= PERIOD;
    if (s_car.z < 0.0f)    s_car.z += PERIOD;
    s_car.cam_h = shz_lerpf(s_car.cam_h, s_car.h, shz_saturatef(3.5f * dt));
    s_car.orbit += (in->orbit * 1.8f + (s_auto ? 0.35f : 0.0f)) * dt;
    if (s_car.orbit > 3.14159f)  s_car.orbit -= 6.28318f;
    if (s_car.orbit < -3.14159f) s_car.orbit += 6.28318f;
    if (!s_auto && shz_fabsf(in->orbit) < 0.05f && shz_fabsf(s_car.v) > 4.0f)  /* driving: the camera eases back behind the car */
        s_car.orbit -= s_car.orbit * shz_saturatef(1.2f * dt);
    s_car.clock += dt;
}

/* ------------------------------------------------------------------------------------------------ helpers */
static inline void haze_tex(gfx_vert_t *v) {       /* textured: base x (1 - f), offset = haze x f */
    const unsigned t = (unsigned)(shz_saturatef((v->w - HAZE_START) * HAZE_INV) * (float)s_haze_max);
    v->oargb = gfx_scale(COL_HAZE, t);
    v->argb = gfx_scale(v->argb, 256u - t);
}
static inline void haze_flat(gfx_vert_t *v) {      /* untextured: the PVR has no offset colour there */
    const unsigned t = (unsigned)(shz_saturatef((v->w - HAZE_START) * HAZE_INV) * (float)s_haze_max);
    v->argb = gfx_mix(v->argb, COL_HAZE, t);
    v->oargb = 0u;
}
static inline uint32_t grey(float k) { const unsigned g = (unsigned)(shz_saturatef(k) * 255.0f); return gfx_rgb(255u, g, g, g); }
static inline uint32_t lit(uint32_t c) { return gfx_mod(c, s_light); }       /* the hour's light colour on a surface */

/* The sky's colour at an elevation (degrees), between the dome's rings. */
static uint32_t dome_at(float deg) {
    static const float ring[5] = { -9.0f, 0.0f, 13.0f, 38.0f, 90.0f };
    unsigned r = 0u;
    while (r < 3u && deg >= ring[r + 1u]) r++;
    return gfx_mix(s_dome_col[r], s_dome_col[r + 1u], (unsigned)(shz_saturatef(shz_divf(deg - ring[r], ring[r + 1u] - ring[r])) * 256.0f));
}

static void tod_eval(void) {
    unsigned i = 0u, k, m;
    const tod_key_t *a, *b;
    float f, elev;
    shz_sincos_t sc;
    while (i < 5u && s_tod >= s_tod_knot[i + 1u].t) i++;
    a = &s_tod_key[s_tod_knot[i].key]; b = &s_tod_key[s_tod_knot[i + 1u].key];
    f = shz_saturatef(shz_divf(s_tod - s_tod_knot[i].t, s_tod_knot[i + 1u].t - s_tod_knot[i].t));
    m = (unsigned)(f * 256.0f);
    for (k = 0; k < 5u; k++) s_dome_col[k] = gfx_mix(a->dome[k], b->dome[k], m);
    s_haze = gfx_mix(a->haze, b->haze, m);
    s_light = gfx_mix(a->light, b->light, m);
    s_sunc = gfx_mix(a->sunc, b->sunc, m);
    s_amb = shz_lerpf(a->amb, b->amb, f);
    s_dif = shz_lerpf(a->dif, b->dif, f);
    s_night = shz_lerpf(a->night, b->night, f);
    elev = shz_lerpf(a->elev, b->elev, f);
    sc = shz_sincosf_deg(elev);
    s_sun[0] = 0.5702f * sc.cos; s_sun[1] = sc.sin; s_sun[2] = 0.8214f * sc.cos;              /* hand: two scales */
    s_sun_sky = dome_at(elev);
    s_road_col = lit(grey(shz_fmaf(s_dif, s_sun[1], s_amb) * (1.0f / 0.87f)));               /* the flat ground: 1 at noon */
    s_lamp = shz_saturatef((s_night - 0.3f) * 2.5f);
    s_lamp_k = (unsigned)(s_lamp * 256.0f);
}

/* z of something in the repeating street nearest to the camera's z */
static inline float wrap_z(float z) {
    float d = z - s_camz;
    while (d > DRAW_FWD) d -= PERIOD;              /* DRAW_BACK + DRAW_FWD = PERIOD: every z has one image in the drawn range */
    while (d <= -DRAW_BACK) d += PERIOD;
    return s_camz + d;
}

/* 1 = a sphere (centre c, radius r) may be on the target. Leaves XMTRX = s_vp. */
static int sphere_seen(float x, float y, float z, float r) {
    gfx_vert_t c;
    c.x = c.y = 0.0f;
    VP_LOAD();
    gfx_xform(&c, shz_vec3_init(x, y, z));
    if (c.w < -r || c.w > DRAW_FWD + CAM_DIST + r) return 0;
    if (c.w > r + GFX_NEAR) {
        const float rp = r * s_focal * shz_invf(c.w);         /* hand: scalar products */
        if (c.x + rp < 0.0f || c.x - rp > s_vw || c.y + rp < 0.0f || c.y - rp > s_vh) return 0;
    }
    return 1;
}

static void quad(const gfx_vert_t *q) { gfx_tri(&q[0], &q[1], &q[2]); gfx_tri(&q[0], &q[2], &q[3]); }

/* ------------------------------------------------------------------------------------------------ sky */
static void draw_sky(shz_vec3_t eye) {
    static gfx_vert_t v[4][12], top, sun[21];
    unsigned r, k;
    const float R = 420.0f;
    VP_LOAD();
    for (r = 0; r < 4u; r++)
        for (k = 0; k < 12u; k++) {
            gfx_xform(&v[r][k], shz_vec3_init(shz_fmaf(s_dome[r][k][0], R, eye.x), shz_fmaf(s_dome[r][k][1], R, eye.y), shz_fmaf(s_dome[r][k][2], R, eye.z)));
            v[r][k].argb = s_dome_col[r]; v[r][k].oargb = 0u; v[r][k].u = v[r][k].v = 0.0f;
        }
    gfx_xform(&top, shz_vec3_init(eye.x, eye.y + R, eye.z));
    top.argb = s_dome_col[4]; top.oargb = 0u; top.u = top.v = 0.0f;
    gfx_bind(GFX_MAT_FLAT);
    gfx_cull(GFX_CULL_NONE);
    for (k = 0; k < 12u; k++) {
        const unsigned n = (k + 1u) % 12u;
        for (r = 0; r < 3u; r++) { gfx_tri(&v[r][k], &v[r][n], &v[r + 1u][n]); gfx_tri(&v[r][k], &v[r + 1u][n], &v[r + 1u][k]); }
        gfx_tri(&v[3][k], &v[3][n], &top);
    }
    {   /* the sun: a white core and a halo that ends in the sky's colour (opaque gouraud, no alpha) */
        const shz_vec3_t d = shz_vec3_init(s_sun[0], s_sun[1], s_sun[2]);
        const shz_vec3_t a = shz_vec3_normalize(shz_vec3_cross(d, shz_vec3_init(0.0f, 1.0f, 0.0f)));
        const shz_vec3_t b = shz_vec3_cross(a, d);
        const shz_vec3_t c = shz_vec3_add(eye, shz_vec3_scale(d, R * 0.96f));
        gfx_xform(&sun[0], c);
        sun[0].argb = s_sunc; sun[0].oargb = 0u; sun[0].u = sun[0].v = 0.0f;
        for (k = 0; k < 10u; k++) {
            const shz_sincos_t sc = shz_sincosf_deg(36.0f * (float)k);
            const shz_vec3_t dir = shz_vec3_add(shz_vec3_scale(a, sc.cos), shz_vec3_scale(b, sc.sin));
            gfx_xform(&sun[1u + k], shz_vec3_add(c, shz_vec3_scale(dir, 13.0f)));
            gfx_xform(&sun[11u + k], shz_vec3_add(c, shz_vec3_scale(dir, 44.0f)));
            sun[1u + k].argb = gfx_mix(s_sunc, s_sun_sky, 48u); sun[11u + k].argb = s_sun_sky;
            sun[1u + k].oargb = sun[11u + k].oargb = 0u;
            sun[1u + k].u = sun[1u + k].v = sun[11u + k].u = sun[11u + k].v = 0.0f;
        }
        for (k = 0; k < 10u; k++) {
            const unsigned n = (k + 1u) % 10u;
            gfx_tri(&sun[0], &sun[1u + k], &sun[1u + n]);
            gfx_tri(&sun[1u + k], &sun[11u + k], &sun[11u + n]);
            gfx_tri(&sun[1u + k], &sun[11u + n], &sun[1u + n]);
        }
    }
}

/* ------------------------------------------------------------------------------------------------ road + buildings */
static void draw_road(void) {
    gfx_vert_t q[4];
    const float z_first = ROAD_TILE * shz_floorf((s_camz - DRAW_BACK) * (1.0f / ROAD_TILE));
    unsigned i, b;
    VP_LOAD();
    gfx_bind(s_low ? GFX_MAT_P_ROAD : GFX_MAT_ROAD);
    for (i = 0; i < (unsigned)((DRAW_BACK + DRAW_FWD) / ROAD_TILE) + 1u; i++) {
        const float z0 = z_first + ROAD_TILE * (float)i, z1 = z0 + ROAD_TILE;
        gfx_xform(&q[0], shz_vec3_init(-BLD_X, 0.0f, z1)); q[0].u = 1.0f; q[0].v = (float)(i + 1u);
        gfx_xform(&q[1], shz_vec3_init(BLD_X, 0.0f, z1));  q[1].u = 0.0f; q[1].v = (float)(i + 1u);
        gfx_xform(&q[2], shz_vec3_init(BLD_X, 0.0f, z0));  q[2].u = 0.0f; q[2].v = (float)i;
        gfx_xform(&q[3], shz_vec3_init(-BLD_X, 0.0f, z0)); q[3].u = 1.0f; q[3].v = (float)i;
        for (b = 0; b < 4u; b++) { q[b].argb = s_road_col; haze_tex(&q[b]); }
        quad(q);
    }
    gfx_bind(GFX_MAT_FLAT);                           /* cross streets: plain asphalt out to the closing buildings */
    for (b = 0; b <= NBLOCK; b++) {
        const float zc = wrap_z((float)b * BLOCK);
        int s;
        if (zc < s_camz - DRAW_BACK - CROSS || zc > s_camz + DRAW_FWD) continue;
        for (s = -1; s <= 1; s += 2) {
            const float xa = s < 0 ? -86.0f : BLD_X, xb = s < 0 ? -BLD_X : 86.0f;
            gfx_xform(&q[0], shz_vec3_init(xa, 0.0f, zc + CROSS * 0.5f));
            gfx_xform(&q[1], shz_vec3_init(xb, 0.0f, zc + CROSS * 0.5f));
            gfx_xform(&q[2], shz_vec3_init(xb, 0.0f, zc - CROSS * 0.5f));
            gfx_xform(&q[3], shz_vec3_init(xa, 0.0f, zc - CROSS * 0.5f));
            for (i = 0; i < 4u; i++) { q[i].argb = gfx_mod(COL_ASPHALT, s_road_col); q[i].u = q[i].v = 0.0f; haze_flat(&q[i]); }
            quad(q);
        }
    }
}

static void wall(float ax, float az, float bx, float bz, float h, float len, float shade) {
    gfx_vert_t q[4];
    unsigned i;
    gfx_xform(&q[0], shz_vec3_init(ax, 0.0f, az)); q[0].u = 0.0f; q[0].v = h * (1.0f / FACADE_TILE);
    gfx_xform(&q[1], shz_vec3_init(bx, 0.0f, bz)); q[1].u = len * (1.0f / FACADE_TILE); q[1].v = q[0].v;
    gfx_xform(&q[2], shz_vec3_init(bx, h, bz));    q[2].u = q[1].u; q[2].v = 0.0f;
    gfx_xform(&q[3], shz_vec3_init(ax, h, az));    q[3].u = 0.0f; q[3].v = 0.0f;
    for (i = 0; i < 4u; i++) { q[i].argb = lit(grey(shade)); haze_tex(&q[i]); }
    quad(q);
}

static void draw_buildings(int roofs) {
    static uint8_t seen[MAX_BLD];
    static float zoff[MAX_BLD];
    unsigned i, m;
    for (i = 0; i < s_nbld; i++) {
        const bld_t *b = &s_bld[i];
        const float zc = wrap_z((b->z0 + b->z1) * 0.5f), hw = (b->z1 - b->z0) * 0.5f, hx = (b->x1 - b->x0) * 0.5f;
        zoff[i] = zc - (b->z0 + b->z1) * 0.5f;
        seen[i] = (uint8_t)(zc + hw > s_camz - DRAW_BACK && zc - hw < s_camz + DRAW_FWD &&
                            sphere_seen((b->x0 + b->x1) * 0.5f, b->h * 0.5f, zc, shz_sqrtf(hw * hw + hx * hx + b->h * b->h * 0.25f)));
    }
    VP_LOAD();
    for (m = 0; m < 4u; m++) {
        gfx_bind((s_low ? GFX_MAT_P_FACADE0 : GFX_MAT_FACADE0) + (int)m);
        for (i = 0; i < s_nbld; i++) {
            const bld_t *b = &s_bld[i];
            const float z0 = b->z0 + zoff[i], z1 = b->z1 + zoff[i];
            if (!seen[i] || b->mat != m) continue;
            /* shades: AMBIENT + DIFFUSE x max(0, n . sun), per face normal */
            if (b->side) wall(b->x0, z0, b->x0, z1, b->h, z1 - z0, AMBIENT);                                     /* street face, -X: in shade */
            else         wall(b->x1, z1, b->x1, z0, b->h, z1 - z0, AMBIENT + DIFFUSE * s_sun[0]);                /* street face, +X */
            wall(b->x0, z1, b->x1, z1, b->h, b->x1 - b->x0, AMBIENT + DIFFUSE * s_sun[2]);                       /* +Z */
            wall(b->x1, z0, b->x0, z0, b->h, b->x1 - b->x0, AMBIENT);                                            /* -Z */
        }
    }
    if (!roofs) return;
    gfx_bind(GFX_MAT_FLAT);
    for (i = 0; i < s_nbld; i++) {
        const bld_t *b = &s_bld[i];
        gfx_vert_t q[4];
        unsigned k;
        if (!seen[i]) continue;
        gfx_xform(&q[0], shz_vec3_init(b->x0, b->h, b->z1 + zoff[i]));
        gfx_xform(&q[1], shz_vec3_init(b->x1, b->h, b->z1 + zoff[i]));
        gfx_xform(&q[2], shz_vec3_init(b->x1, b->h, b->z0 + zoff[i]));
        gfx_xform(&q[3], shz_vec3_init(b->x0, b->h, b->z0 + zoff[i]));
        for (k = 0; k < 4u; k++) { q[k].argb = gfx_mod(COL_ROOF, s_road_col); q[k].u = q[k].v = 0.0f; haze_flat(&q[k]); }
        quad(q);
    }
}

/* ------------------------------------------------------------------------------------------------ models */
/* Opaque model, palette colour x (AMBIENT + DIFFUSE x n . sun). XMTRX is s_vp x translate x rotate on return. */
static void draw_model(const mdl_t *m, float x, float z, float yaw) {
    const shz_sincos_t sc = shz_sincosf(yaw);
    /* the sun in model space (rotation by -yaw about Y): two FMACs (shz_fmaf), once a model */
    const float lm[3] = { shz_fmaf(s_sun[0], sc.cos, -s_sun[2] * sc.sin), s_sun[1], shz_fmaf(s_sun[0], sc.sin, s_sun[2] * sc.cos) };
    unsigned i;
    if (m->nv > SCRATCH_N) {
        if (!(s_refused & 1u)) { s_refused |= 1u; printf("demo: model of %u vertices refused (scratch holds %u)\n", (unsigned)m->nv, (unsigned)SCRATCH_N); }
        return;
    }
    VP_LOAD();
    shz_xmtrx_translate(x, 0.0f, z);
    shz_xmtrx_rotate_y(yaw);
    for (i = 0; i < m->nv; i++) {
        const mdl_vert_t *s = &m->v[i];
        gfx_vert_t *o = &s_scr[i];
        const float k = shz_fmaf(DIFFUSE, shz_fmaxf(0.0f, sh4der_dot3_(s->n, lm)), AMBIENT);
        gfx_xform(o, shz_vec3_init(s->p[0], s->p[1], s->p[2]));
        o->argb = lit(gfx_scale(0xff000000u | s->rgb, (unsigned)(shz_fminf(k, 1.0f) * 256.0f)));
        o->u = o->v = 0.0f;
        haze_flat(o);
    }
    for (i = 0; i + 2u < m->ni; i += 3u) gfx_tri(&s_scr[m->idx[i]], &s_scr[m->idx[i + 1u]], &s_scr[m->idx[i + 2u]]);
}

static void draw_props(int reflect_pass) {
    unsigned i;
    gfx_bind(GFX_MAT_FLAT);
    for (i = 0; i < s_nprop; i++) {
        const prop_t *p = &s_prop[i];
        const float z = wrap_z(p->z);
        if (z < s_camz - DRAW_BACK || z > s_camz + (p->wheels ? 220.0f : 170.0f)) continue;
        if (reflect_pass && !p->wheels && z > s_camz + 90.0f) continue;       /* small far furniture is not worth mirroring */
        if (!sphere_seen(p->x, p->body->radius * 0.5f, z, p->body->radius)) continue;
        draw_model(p->body, p->x, z, p->yaw);
        if (p->wheels && !reflect_pass && z < s_camz + 110.0f) draw_model(p->wheels, p->x, z, p->yaw);
    }
    if (reflect_pass) return;
    {   /* wires between the poles: two thin ribbons, both faces */
        gfx_vert_t q[4];
        const float z_first = 32.0f * shz_floorf((s_camz - DRAW_BACK) * (1.0f / 32.0f)) - 2.0f;
        unsigned w, k;
        VP_LOAD();
        gfx_cull(GFX_CULL_NONE);
        for (i = 0; i < 7u; i++)
            for (w = 0; w < 2u; w++) {
                const float z0 = z_first + 32.0f * (float)i, x = -(BLD_X - 0.7f) + (w ? 1.1f : -1.1f), y = 6.35f;
                gfx_xform(&q[0], shz_vec3_init(x, y, z0));
                gfx_xform(&q[1], shz_vec3_init(x, y, z0 + 32.0f));
                gfx_xform(&q[2], shz_vec3_init(x, y + 0.07f, z0 + 32.0f));
                gfx_xform(&q[3], shz_vec3_init(x, y + 0.07f, z0));
                for (k = 0; k < 4u; k++) { q[k].argb = 0xff202024u; q[k].u = q[k].v = 0.0f; haze_flat(&q[k]); }
                quad(q);
            }
        gfx_cull(GFX_CULL_BACK);
    }
}

/* ------------------------------------------------------------------------------------------------ the player's car */
/* Opaque body; fills s_refl (the reflection layer's vertices) when the reflection is on. */
static void draw_player(void) {
    const mdl_t *m = &mdl_player_body;
    unsigned i;
    uint64_t t0;
    draw_model(&mdl_player_wheels, s_car.x, s_car.z, s_car.h);
    if (m->nv > MDL_PLAYER_BODY_NV) {
        if (!(s_refused & 2u)) { s_refused |= 2u; printf("demo: player body of %u vertices refused (arrays hold %u)\n", (unsigned)m->nv, (unsigned)MDL_PLAYER_BODY_NV); }
        return;
    }
    /* world positions + normals (model matrix in XMTRX) */
    shz_xmtrx_init_translation(s_car.x, 0.0f, s_car.z);
    shz_xmtrx_rotate_y(s_car.h);
    for (i = 0; i < m->nv; i++) {
        const mdl_vert_t *s = &m->v[i];
        const shz_vec3_t p = shz_xmtrx_transform_point3(shz_vec3_init(s->p[0], s->p[1], s->p[2]));
        /* normals: directions through the same matrix (shz_xmtrx_transform_vec3: the rotation, no translation) */
        const float *m_sn = mdl_player_body_sn[i];
        const shz_vec3_t n = shz_xmtrx_transform_vec3(shz_vec3_init(s->n[0], s->n[1], s->n[2]));
        const shz_vec3_t sn = shz_xmtrx_transform_vec3(shz_vec3_init(m_sn[0], m_sn[1], m_sn[2]));
        s_wp[i][0] = p.x; s_wp[i][1] = p.y; s_wp[i][2] = p.z;
        s_wn[i][0] = n.x; s_wn[i][1] = n.y; s_wn[i][2] = n.z;
        s_wsn[i][0] = sn.x; s_wsn[i][1] = sn.y; s_wsn[i][2] = sn.z;
    }
    VP_LOAD();
    for (i = 0; i < m->nv; i++) {
        const mdl_vert_t *s = &m->v[i];
        gfx_vert_t *o = &s_body[i];
        const float k = shz_fmaf(DIFFUSE, shz_fmaxf(0.0f, sh4der_dot3_(s_wn[i], s_sun)), AMBIENT);
        gfx_xform(o, shz_vec3_init(s_wp[i][0], s_wp[i][1], s_wp[i][2]));
        o->argb = lit(gfx_scale(s->cls == CLS_GLASS ? COL_GLASS : (0xff000000u | s->rgb), (unsigned)(shz_fminf(k, 1.0f) * 256.0f)));
        if (s->cls == CLS_LAMP) o->argb = gfx_mix(o->argb, 0xff000000u | s->rgb, s_lamp_k);   /* lamps lit: their own colour, no shading */
        o->oargb = 0u; o->u = o->v = 0.0f;
    }
    gfx_bind(GFX_MAT_FLAT);
    for (i = 0; i + 2u < m->ni; i += 3u) gfx_tri(&s_body[m->idx[i]], &s_body[m->idx[i + 1u]], &s_body[m->idx[i + 2u]]);
    if (!s_pv_ok) return;
    /* pl4nar: one lookup + Fresnel per paint / glass vertex. Two materials from the library's presets, tuned here. */
    t0 = plat_us();
    {
        sh4der_pl4nar_material_t paint = sh4der_pl4nar_preset(SH4DER_PL4NAR_PAINT), glass = sh4der_pl4nar_preset(SH4DER_PL4NAR_GLASS);
        const float us = gfx_reflect_u_scale();
        paint.f0 = s_f0; glass.f0 = s_f0 + 0.10f;
        paint.gain = glass.gain = shz_lerpf(1.0f, s_lamp_gain, s_night);      /* at night the lamps want more of the layer */
        sh4der_pl4nar_begin(&s_pv);                           /* XMTRX = the reflection view until the loop ends */
        for (i = 0; i < m->nv; i++) {
            const mdl_vert_t *s = &m->v[i];
            const sh4der_pl4nar_material_t *mat = s->cls == CLS_GLASS ? &glass : &paint;
            gfx_vert_t *o = &s_refl[i];
            sh4der_pl4nar_sample_t smp;
            if (s->cls != CLS_PAINT && s->cls != CLS_GLASS) continue;
            sh4der_pl4nar_shade(&s_pv, s_wp[i], s_wsn[i], mat, &smp);
            *o = s_body[i];                                   /* the opaque vertex's own x, y, z: the layer has its depth */
            o->u = smp.u * us; o->v = smp.v;                  /* hand: one scale (1 on a KallistiOS with a sized target) */
            o->argb = sh4der_pl4nar_argb(&smp, mat);
        }
    }
    s_refl_cpu_us = (uint32_t)(plat_us() - t0);
}

/* The layer: paint first (added: a clear coat keeps the paint's colour), then glass (blended: glass is its reflection). */
static void draw_player_reflection(void) {
    const mdl_t *m = &mdl_player_body;
    unsigned i, pass;
    gfx_cull(GFX_CULL_BACK);
    for (pass = 0; pass < 2u; pass++) {
        const uint32_t want = pass ? CLS_GLASS : CLS_PAINT;     /* the bake gives every triangle one class (tools/bake.py checks) */
        gfx_bind(pass ? GFX_MAT_REFLECT : GFX_MAT_REFLECT_ADD);
        for (i = 0; i + 2u < m->ni; i += 3u)
            if (m->v[m->idx[i]].cls == want && m->v[m->idx[i + 1u]].cls == want && m->v[m->idx[i + 2u]].cls == want)   /* all three were written */
                gfx_tri(&s_refl[m->idx[i]], &s_refl[m->idx[i + 1u]], &s_refl[m->idx[i + 2u]]);
    }
}

/* ------------------------------------------------------------------------------------------------ lights + shadows */
/* A sh4der_pool fan through the pass's matrix. closed: a disc (the rim wraps); else a wedge. */
static void fan(const sh4der_pool_vtx_t *p, unsigned nv, int closed) {
    static gfx_vert_t v[SH4DER_POOL_MAX_N + 2];
    const unsigned n = closed ? nv - 1u : nv - 2u;
    unsigned i;
    if (nv < 3u || nv > SH4DER_POOL_MAX_N + 2u) return;       /* 0 = the pool call refused; never past the array */
    for (i = 0; i < nv; i++) {
        gfx_xform(&v[i], shz_vec3_init(p[i].x, p[i].y, p[i].z));
        v[i].argb = p[i].argb; v[i].oargb = 0u; v[i].u = v[i].v = 0.0f;
    }
    for (i = 0; i < n; i++) gfx_tri(&v[0], &v[i + 1u], &v[closed ? (i + 1u) % n + 1u : i + 2u]);
}

/* A halo: SH4DER-PVR's camera-facing quad (sh4der_spark_quad) at a world point, projected by hand (the quad comes
   back in view space). rgb x gain is the brightness; the bound material decides how it is drawn. */
static void halo(float x, float y, float z, float half, uint32_t rgb, float gain) {
    sh4der_spark_t s;
    sh4der_spark_vtx_t q[4];
    gfx_vert_t v[4];
    unsigned k;
    memset(&s, 0, sizeof s);
    s.p[0] = x; s.p[1] = y; s.p[2] = z;
    s.q[0] = half;
    s.argb = gfx_scale(0xff000000u | rgb, (unsigned)(shz_saturatef(gain) * 256.0f));
    s.life_ms = 1000u; s.shape = SH4DER_SPARK_BILLBOARD;
    if (!sh4der_spark_quad(&s, &s_sv, q)) return;
    for (k = 0; k < 4u; k++) {
        const float iz = shz_invf(q[k].z);
        v[k].x = shz_fmaf(q[k].x * iz, s_focal, s_vw * 0.5f);          /* hand: the projection of a view-space point */
        v[k].y = shz_fmaf(-q[k].y * iz, s_focal, s_vh * 0.5f);
        v[k].z = iz; v[k].w = q[k].z; v[k].cx = v[k].x * q[k].z; v[k].cy = v[k].y * q[k].z;      /* hand: two plain products, no SH4ZAM op */
        v[k].u = q[k].u; v[k].v = q[k].v; v[k].argb = q[k].argb; v[k].oargb = 0u; v[k].clip = 0u;
    }
    gfx_tri(&v[0], &v[1], &v[2]);
    gfx_tri(&v[1], &v[3], &v[2]);
}

static void spark_view(shz_vec3_t eye, shz_vec3_t at) {
    const shz_vec3_t f = shz_vec3_normalize(shz_vec3_sub(at, eye));
    const shz_vec3_t r = shz_vec3_normalize(shz_vec3_cross(f, shz_vec3_init(0.0f, 1.0f, 0.0f)));
    const shz_vec3_t u = shz_vec3_cross(r, f);
    s_sv.right[0] = r.x; s_sv.right[1] = r.y; s_sv.right[2] = r.z;
    s_sv.up[0] = u.x; s_sv.up[1] = u.y; s_sv.up[2] = u.z;
    s_sv.fwd[0] = f.x; s_sv.fwd[1] = f.y; s_sv.fwd[2] = f.z;
    s_sv.eye[0] = eye.x; s_sv.eye[1] = eye.y; s_sv.eye[2] = eye.z;
    s_sv.near_z = GFX_NEAR; s_sv.focal_px = s_focal; s_sv.min_px = SH4DER_SPARK_MIN_PX; s_sv.inset = 0.5f / (float)TEX_GLOW_SIZE;
    s_sv.cols = 1u; s_sv.rows = 1u;
}

/* The street lamps near the car. what 0: the pools on the road (GFX_MAT_ADD bound, XMTRX = s_vp). 1: the halos
   (GFX_MAT_GLOW). 2: the mirror pass: each head's glow at its mirrored place (GFX_MAT_GLOW, the target's translucent list). */
static void draw_lamps(int what) {
    const float range = what == 2 ? LAMP_MIRROR : LAMP_NEAR;
    static const float ux[3] = { 7.5f, 0.0f, 0.0f }, uz[3] = { 0.0f, 0.0f, 7.5f };
    sh4der_pool_vtx_t p[SH4DER_POOL_MAX_N + 2];
    unsigned i;
    if (s_lamp <= 0.02f) return;
    for (i = 0; i < s_nprop; i++) {
        const prop_t *pr = &s_prop[i];
        const float z = wrap_z(pr->z), hx = pr->x - LAMP_ARM * shz_sinf(pr->yaw);
        if (pr->body != &mdl_lamp || shz_fabsf(z - s_car.z) > range) continue;
        if (what == 0) {
            const float c[3] = { hx, 0.03f, z };
            fan(p, sh4der_pool_disc(c, ux, uz, 10u, COL_LAMP, 0.60f * s_lamp, p), 1);
        } else if (what == 1) halo(hx, LAMP_HEAD_Y, z, 2.2f, COL_LAMP, s_lamp);
        else halo(hx, -LAMP_HEAD_Y, z, 1.5f, COL_LAMP, s_lamp);
    }
}

/* The car's own lights: two wedges on the road ahead (GFX_MAT_ADD), then four halos (GFX_MAT_GLOW). */
static void draw_car_lights(int halos) {
    const shz_sincos_t hc = shz_sincosf(s_car.h);
    const float fw[3] = { hc.sin, 0.0f, hc.cos }, rt[3] = { hc.cos, 0.0f, -hc.sin };
    sh4der_pool_vtx_t p[SH4DER_POOL_MAX_N + 2];
    int s;
    if (s_lamp <= 0.02f) return;
    for (s = -1; s <= 1; s += 2) {
        const float side = 0.62f * (float)s;
        if (!halos) {
            const float a[3] = { shz_fmaf(fw[0], 2.3f, shz_fmaf(rt[0], side, s_car.x)), 0.035f, shz_fmaf(fw[2], 2.3f, shz_fmaf(rt[2], side, s_car.z)) };
            fan(p, sh4der_pool_wedge(a, fw, rt, 30.0f, 0.11f, 2u, COL_HEAD, 0.45f * s_lamp, p), 0);      /* the beam */
            fan(p, sh4der_pool_wedge(a, fw, rt, 20.0f, 0.30f, 4u, COL_HEAD, 0.22f * s_lamp, p), 0);      /* its spill: a soft edge */
        } else {
            halo(shz_fmaf(fw[0], 2.40f, shz_fmaf(rt[0], side, s_car.x)), 0.60f, shz_fmaf(fw[2], 2.40f, shz_fmaf(rt[2], side, s_car.z)), 0.55f, COL_HEAD, s_lamp);
            halo(shz_fmaf(fw[0], -2.42f, shz_fmaf(rt[0], side, s_car.x)), 0.60f, shz_fmaf(fw[2], -2.42f, shz_fmaf(rt[2], side, s_car.z)), 0.40f, COL_TAIL, s_lamp);
        }
    }
}

/* Blob shadows: a dark ellipse under each car (GFX_MAT_HUD: vertex colour, blended). */
static void blob(float x, float z, float yaw, float alpha) {
    const shz_sincos_t hc = shz_sincosf(yaw);
    const float c[3] = { x, 0.02f, z }, u[3] = { hc.sin * 2.9f, 0.0f, hc.cos * 2.9f }, v[3] = { hc.cos * 1.35f, 0.0f, -hc.sin * 1.35f };
    sh4der_pool_vtx_t p[SH4DER_POOL_MAX_N + 2];
    fan(p, sh4der_pool_disc(c, u, v, 8u, 0x00000000u, alpha, p), 1);
}

static void draw_shadows(void) {
    const float alpha = shz_lerpf(0.50f, 0.25f, s_night);
    unsigned i;
    blob(s_car.x, s_car.z, s_car.h, alpha);
    for (i = 0; i < s_nprop; i++) {
        const prop_t *pr = &s_prop[i];
        const float z = wrap_z(pr->z);
        if (pr->wheels && shz_fabsf(z - s_car.z) < 80.0f) blob(pr->x, z, pr->yaw, alpha);
    }
}

/* ------------------------------------------------------------------------------------------------ frame */
static void overlay(const demo_stats_t *st) {
    char t[96];
    const unsigned cs = (unsigned)(s_car.clock * 100.0f);
    const float kmh = shz_fabsf(s_car.v) * 3.6f;
    gfx_cull(GFX_CULL_NONE);
    /* game HUD: timer, two bars, speed, map */
    snprintf(t, sizeof t, "%u:%02u.%02u", cs / 6000u, (cs / 100u) % 60u, cs % 100u);
    hud_text(24.0f, 20.0f, 2.0f, 0xffffffffu, t);
    hud_bar(24.0f, 58.0f, 150.0f, 10.0f, 0.18f, 0xffd04030u, "BODY");
    hud_bar(24.0f, 86.0f, 150.0f, 10.0f, shz_saturatef(0.30f + 0.2f * shz_sinf(s_car.clock * 0.7f)), 0xff3070e0u, "HEAT");
    snprintf(t, sizeof t, "%3u km/h", (unsigned)kmh);
    hud_text(500.0f, 430.0f, 2.0f, 0xffffffffu, t);
    hud_map(530.0f, 20.0f, 88.0f, s_car.x * (1.0f / BLD_X), s_car.z * (1.0f / PERIOD), s_car.h);
    if (!s_info) return;
    {
        const unsigned tw = s_sizes[s_size][0], th = s_sizes[s_size][1];
        float y = 328.0f;
        snprintf(t, sizeof t, "FPS %u.%u  CPU %u.%02u ms  PVR %u.%02u ms", (unsigned)(st->fps_x10 / 10u), (unsigned)(st->fps_x10 % 10u),
                 (unsigned)(st->cpu_us / 1000u), (unsigned)((st->cpu_us % 1000u) / 10u), (unsigned)(st->pvr_us / 1000u), (unsigned)((st->pvr_us % 1000u) / 10u));
        hud_text(24.0f, y, 1.0f, 0xffffffffu, t); y += 16.0f;
        snprintf(t, sizeof t, "RAM free %u KB  VRAM free %u KB", (unsigned)st->ram_free_kb, (unsigned)st->vram_free_kb);
        hud_text(24.0f, y, 1.0f, 0xffffffffu, t); y += 16.0f;
        if (s_reflect) {
            snprintf(t, sizeof t, "PL4NAR ON  %ux%u = %u KB  F0 %u.%02u", tw, th, gfx_reflect_bytes() / 1024u, (unsigned)s_f0, (unsigned)(s_f0 * 100.0f + 0.5f) % 100u);
            hud_text(24.0f, y, 1.0f, 0xff80ff90u, t); y += 16.0f;
            snprintf(t, sizeof t, "lookup %u.%02u ms  PVR mirror %u.%02u ms  tris %u + %u mirror", (unsigned)(s_refl_cpu_us / 1000u), (unsigned)((s_refl_cpu_us % 1000u) / 10u),
                     (unsigned)(st->rtt_us / 1000u), (unsigned)((st->rtt_us % 1000u) / 10u), (unsigned)s_tris_main, (unsigned)s_tris_refl);
            hud_text(24.0f, y, 1.0f, 0xff80ff90u, t); y += 16.0f;
        } else {
            hud_text(24.0f, y, 1.0f, 0xffff9080u, "PL4NAR OFF (flat)"); y += 16.0f;
            snprintf(t, sizeof t, "tris %u", (unsigned)s_tris_main);
            hud_text(24.0f, y, 1.0f, 0xffff9080u, t); y += 16.0f;
        }
        snprintf(t, sizeof t, "%s  TIME %s  clock %u.%02u  lamps %u%%  night gain %u.%02u", s_low ? "LOOK 64x64 16col" : "LOOK full", s_tod_name[s_tod_mode], (unsigned)s_tod, (unsigned)(s_tod * 100.0f) % 100u,
                 (unsigned)(s_lamp * 100.0f), (unsigned)s_lamp_gain, (unsigned)(s_lamp_gain * 100.0f + 0.5f) % 100u);
        hud_text(24.0f, y, 1.0f, 0xffffe0a0u, t); y += 16.0f;
        hud_text(24.0f, y, 1.0f, 0xffc0c0c0u, "RT gas  LT brake  stick steer  B+stick camera  B+UP/DOWN night gain"); y += 16.0f;
        hud_text(24.0f, y, 1.0f, 0xffc0c0c0u, "X reflect  Y time  A look  B+A size  UP / DOWN F0  START info");
        if (s_reflect) {                                  /* the target itself, top centre: what the mirrored camera sees */
            const float w = 128.0f, h = 64.0f, x = 256.0f, y0 = 14.0f;
            gfx_bind(GFX_MAT_HUD);
            gfx_quad2d(x - 2.0f, y0 - 2.0f, x + w + 2.0f, y0 + h + 2.0f, 8.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xc0000000u);
            gfx_bind(GFX_MAT_REFLECT);
            gfx_quad2d(x, y0, x + w, y0 + h, 8.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0xffffffffu);   /* v turned over: the mirrored world shown right side up */
        }
    }
}

void demo_draw(const demo_stats_t *st) {
    const shz_sincos_t hc = shz_sincosf(s_car.h), cc = shz_sincosf(s_car.cam_h + s_car.orbit);
    const shz_vec3_t eye = shz_vec3_init(s_car.x - cc.sin * CAM_DIST, CAM_HEIGHT, s_car.z - cc.cos * CAM_DIST);
    const shz_vec3_t at = shz_vec3_init(s_car.x + hc.sin * 1.2f, 1.0f, s_car.z + hc.cos * 1.2f);
    const float eye3[3] = { eye.x, eye.y, eye.z }, at3[3] = { at.x, at.y, at.z }, up3[3] = { 0.0f, 1.0f, 0.0f };
    s_camz = eye.z;
    s_tris_refl = 0u;
    tod_eval();
    gfx_clear_colour(s_haze);
    s_pv_ok = 0;
    if (s_reflect) {
        const float tw = (float)s_sizes[s_size][0], th = (float)s_sizes[s_size][1], fov = FOV_MAIN * SH4DER_PL4NAR_FOV_SCALE;
        s_pv_ok = sh4der_pl4nar_view_build(&s_pv, &s_road_plane, eye3, at3, up3, fov, GFX_NEAR, tw, th, SH4DER_PL4NAR_FADE_W);
    }
    if (s_pv_ok) {
        const float tw = (float)s_sizes[s_size][0], th = (float)s_sizes[s_size][1], fov = FOV_MAIN * SH4DER_PL4NAR_FOV_SCALE;
        memcpy(s_vp, s_pv.row, sizeof s_vp);
        s_vw = tw; s_vh = th;
        s_focal = th * 0.5f * shz_divf(shz_cosf(fov * 0.5f), shz_sinf(fov * 0.5f));
        s_haze_max = (unsigned)(HAZE_MAX * 0.45f * 256.0f);
        if (gfx_reflect_begin() == 0) {
            gfx_viewport(tw, th);
            gfx_bind(GFX_MAT_FLAT);
            gfx_cull(GFX_CULL_NONE);
            gfx_quad2d(0.0f, 0.0f, tw, th, 0.00002f, 0.0f, 0.0f, 0.0f, 0.0f, gfx_mod(COL_GROUND_REFLECT, s_road_col));   /* behind everything: a ray into the road */
            draw_sky(eye);
            gfx_cull(GFX_CULL_MIRROR);
            draw_buildings(0);
            draw_props(1);
            if (s_lamp > 0.02f) {                             /* the lamp heads, lit: what sweeps over the paint at night */
                spark_view(eye, at);
                gfx_list_translucent();
                gfx_bind(GFX_MAT_GLOW);
                gfx_cull(GFX_CULL_NONE);
                draw_lamps(2);
            }
            s_tris_refl = gfx_tris_sent();
            gfx_reflect_end();
        }
    }
    s_haze_max = (unsigned)(HAZE_MAX * 256.0f);
    sh4der_pl4nar_viewproj_rows(s_vp, NULL, eye3, at3, up3, FOV_MAIN, GFX_NEAR, GFX_SCREEN_W, GFX_SCREEN_H);
    s_vw = GFX_SCREEN_W; s_vh = GFX_SCREEN_H;
    s_focal = GFX_SCREEN_H * 0.5f * shz_divf(shz_cosf(FOV_MAIN * 0.5f), shz_sinf(FOV_MAIN * 0.5f));
    gfx_frame_begin();
    gfx_viewport(GFX_SCREEN_W, GFX_SCREEN_H);
    draw_sky(eye);
    gfx_cull(GFX_CULL_BACK);
    draw_road();
    draw_buildings(1);
    draw_props(0);
    draw_player();
    gfx_list_translucent();
    spark_view(eye, at);
    gfx_cull(GFX_CULL_NONE);
    VP_LOAD();
    gfx_bind(GFX_MAT_HUD);
    draw_shadows();
    gfx_bind(GFX_MAT_ADD);
    draw_lamps(0);
    draw_car_lights(0);
    if (s_pv_ok) draw_player_reflection();
    gfx_cull(GFX_CULL_NONE);
    gfx_bind(GFX_MAT_GLOW);
    draw_lamps(1);
    draw_car_lights(1);
    s_tris_main = gfx_tris_sent();
    overlay(st);
    gfx_frame_end();
}
