/* Host test of sh4der_hexheat.h against a double-precision transcription of the owner's formulas, written in WORLD
   units (metres) where the header works in pixels at the sheet:
     the lattice: homes from the integers (q, r), the same every call (bit equal);
     the camera: the view agrees with sh4der_pl4nar_viewproj_rows (the street demo's own projection);
     screen to world on the ground plane (the ray-to-ground pick): a known world point's screen place gives the point
       back, the round trip holds, D is right, a ray at or over the horizon is refused;
     the haze's point: the ground short of the sheet, else the sheet, with h and D right, one piece at the join;
     the field: the offset against the numeric derivative of T', a pure function of the time (no state), a small and
       bounded change a frame, the largest offset at most OFFSET_MAX s on the screen;
     the mesh: every drawn hexagon against the transcription (ray, envelope, field, back to pixels), its picture's
       place fixed, the band's edge, the near cut, the gate.
   Tolerance of the float path against the transcription: TOL pixels on the plain C path. On the SH4ZAM path the
   envelope's exp (shz_expf) and the cut's log are SH4ZAM's fast approximations, good to a few per cent, so an offset may
   differ by TOL_REL of the cell's own largest offset more. The largest error found is printed. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "sh4der_hexheat.h"
#include "sh4der_pl4nar.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#ifdef SH4DER_CORE_NO_SH4ZAM
#define TOL 0.005                                      /* pixels */
#define TOL_REL 0.0
#else
#define TOL 0.01
#define TOL_REL 0.07                                   /* SH4ZAM's fast exp: within about 4 % a call, two calls an offset */
#endif
#define PI 3.14159265358979323846

static const double PHI[4] = { 1.7, 4.1, 0.6, 5.3 };
static double SMAX;                                    /* the largest |sum cos(2^i u + phi_i)|, scanned */

static double dot3(const double a[3], const double b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static void cross3(const double a[3], const double b[3], double o[3]) {
    o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0];
}
static void norm3(double v[3]) { const double m = sqrt(dot3(v, v)); v[0] /= m; v[1] /= m; v[2] /= m; }
static double sat(double v) { return v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v; }
/* An offset's error against the transcription, over what is allowed for a cell whose largest offset is `most` pixels. */
static double over(double err, double most) { return fabs(err) - TOL - TOL_REL * most; }

/* ---- the transcription ---------------------------------------------------------------------------------------------- */
typedef struct { double eye[3], f[3], r[3], dn[3], focal, cx, cy, sw, sh, n[3], d, sheet, near_cut; } cam_t;
typedef struct { double p[3], dist, w, h, hs, X, Y; int ground; } rhit_t;
typedef struct { int inside, drawn; double ox, oy, scale, maxoff, A; rhit_t hit; } rcell_t;

static void cam_make(cam_t *c, const float eye[3], const float target[3], const float up[3], double fov, double sw, double sh,
                     const float n[3], double d, double sheet, double near_cut) {
    double u[3];
    unsigned k;
    for (k = 0; k < 3u; k++) { c->eye[k] = eye[k]; c->f[k] = (double)target[k] - (double)eye[k]; u[k] = up[k]; c->n[k] = n[k]; }
    norm3(c->f);
    cross3(c->f, u, c->r); norm3(c->r);
    cross3(c->f, c->r, c->dn);
    c->focal = 0.5 * sh / tan(0.5 * fov); c->cx = 0.5 * sw; c->cy = 0.5 * sh; c->sw = sw; c->sh = sh;
    c->d = d; c->sheet = sheet; c->near_cut = near_cut;
}
static void cam_ground(const cam_t *c, double g[3], double rg[3], double *e, double *ppm) {
    const double nf = dot3(c->n, c->f);
    unsigned k;
    for (k = 0; k < 3u; k++) g[k] = c->f[k] - c->n[k] * nf;
    norm3(g);
    cross3(g, c->n, rg);
    *e = dot3(c->n, c->eye) - c->d;
    *ppm = c->focal / (c->sheet * dot3(g, c->f) - *e * nf);     /* pixels a metre at the sheet's foot */
}
/* The ray through (sx, sy), met with the ground. 0: it never lands. */
static int ref_ground(const cam_t *c, double sx, double sy, double p[3], double *dist) {
    double dir[3], t;
    unsigned k;
    for (k = 0; k < 3u; k++) dir[k] = c->f[k] + c->r[k] * (sx - c->cx) / c->focal + c->dn[k] * (sy - c->cy) / c->focal;
    if (!(dot3(c->n, dir) < 0.0)) return 0;
    t = (c->d - dot3(c->n, c->eye)) / dot3(c->n, dir);
    for (k = 0; k < 3u; k++) p[k] = c->eye[k] + t * dir[k];
    *dist = t * sqrt(dot3(dir, dir));
    return 1;
}
/* The haze's point: the ground short of the sheet, else the sheet. */
static int ref_haze(const cam_t *c, double sx, double sy, rhit_t *o) {
    double g[3], rg[3], e, ppm, dir[3], rel[3], gd, dist;
    unsigned k;
    cam_ground(c, g, rg, &e, &ppm);
    for (k = 0; k < 3u; k++) dir[k] = c->f[k] + c->r[k] * (sx - c->cx) / c->focal + c->dn[k] * (sy - c->cy) / c->focal;
    if (!(dot3(g, dir) > 0.0)) return 0;
    o->hs = e + c->sheet / dot3(g, dir) * dot3(c->n, dir);
    if (ref_ground(c, sx, sy, o->p, &dist)) {
        for (k = 0; k < 3u; k++) rel[k] = o->p[k] - c->eye[k];
        gd = dot3(rel, g);
    } else gd = 1e300;
    if (gd <= c->sheet) { o->ground = 1; o->h = 0.0; o->Y = gd - c->sheet; }
    else {
        const double t = c->sheet / dot3(g, dir);
        for (k = 0; k < 3u; k++) { o->p[k] = c->eye[k] + t * dir[k]; rel[k] = o->p[k] - c->eye[k]; }
        o->ground = 0; o->h = dot3(c->n, o->p) - c->d; o->Y = o->h;
    }
    o->dist = sqrt(dot3(rel, rel)); o->w = dot3(rel, c->f); o->X = dot3(rel, rg);
    return 1;
}
/* T' in the world, metres: theta_i = k_i (X - U t) + m_i (Y - W t) + phi_i. hexw_m: a hexagon's width in metres. */
static double ref_T(double X, double Y, double t, double hexw_m) {
    const double W = (double)SH4DER_HEXHEAT_RISE_HEX * hexw_m, U = (double)SH4DER_HEXHEAT_WIND * W;
    double k = 2.0 * PI / ((double)SH4DER_HEXHEAT_WAVE_HEX * hexw_m), m = 2.0 * PI / ((double)SH4DER_HEXHEAT_WAVE_HEX_UP * hexw_m), a = 1.0, T = 0.0;
    unsigned i;
    for (i = 0; i < SH4DER_HEXHEAT_LAYERS; i++) { T += a * sin(k * (X - U * t) + m * (Y - W * t) + PHI[i]); k *= 2.0; m *= 2.0; a *= 0.5; }
    return T;
}
/* A cell of the lattice at home (hx, hy): what the mesh must give. slip_px, drive_px: the two sums. */
static void ref_cell(const cam_t *c, double hx, double hy, double t, double gate, double x0, double y0, double slip_px, double drive_px, rcell_t *o) {
    const double s = (double)SH4DER_HEXHEAT_S, hexw = s * sqrt(3.0), hw = 0.5 * hexw;
    double left = x0 + 1.0, right = x0 + (double)SH4DER_HEXHEAT_BAND_W - 1.0, top = y0 + 1.0, bottom = y0 + (double)SH4DER_HEXHEAT_BAND_H - 1.0;
    double g[3], rg[3], e, ppm, edge, taper, fade, H, hexw_m, k, m, a, W, U, A0, A, kpix, X, Y, dX = 0.0, dY = 0.0, lens = 0.0;
    unsigned i;
    memset(o, 0, sizeof *o);
    if (left < 0.0) left = 0.0;
    if (top < 0.0) top = 0.0;
    if (right > c->sw) right = c->sw;
    if (bottom > c->sh) bottom = c->sh;
    if (hx - hw < left || hx + hw > right || hy - s < top || hy + s > bottom) return;
    o->inside = 1;
    if (!ref_haze(c, hx, hy, &o->hit)) return;
    cam_ground(c, g, rg, &e, &ppm);
    edge = fmin(fmin(hx - hw - left, right - hx - hw), hy - s - top);
    taper = sat(edge / (double)SH4DER_HEXHEAT_EDGE);
    fade = sat((o->hit.hs - e * (1.0 - c->sheet / c->near_cut)) / ((double)SH4DER_HEXHEAT_NEAR_FADE * s / ppm));
    H = (double)SH4DER_HEXHEAT_LAYER_FRAC * c->sh / ppm;                        /* metres */
    hexw_m = hexw / ppm;
    k = 2.0 * PI / ((double)SH4DER_HEXHEAT_WAVE_HEX * hexw_m); m = 2.0 * PI / ((double)SH4DER_HEXHEAT_WAVE_HEX_UP * hexw_m);
    W = (double)SH4DER_HEXHEAT_RISE_HEX * hexw_m; U = (double)SH4DER_HEXHEAT_WIND * W;
    A0 = (double)SH4DER_HEXHEAT_PEAK * (s / ppm) / (sqrt(k * k + m * m) * SMAX)    /* tuned: the largest offset is PEAK s (in metres) at the sheet's foot */
         / (1.0 - exp(-sqrt(c->sheet * c->sheet + e * e) / (double)SH4DER_HEXHEAT_D0));
    A = A0 * sat(gate) * (1.0 - exp(-o->hit.dist / (double)SH4DER_HEXHEAT_D0)) * exp(-o->hit.h / H) * fade * taper;
    kpix = fmin(c->focal / o->hit.w, (double)SH4DER_HEXHEAT_OFFSET_MAX / (double)SH4DER_HEXHEAT_PEAK * ppm);   /* pixels a metre: focal / depth */
    o->A = A;
    o->maxoff = A * sqrt(k * k + m * m) * SMAX * kpix;
    o->drawn = o->maxoff >= (double)SH4DER_HEXHEAT_CUT;
    if (o->maxoff > (double)SH4DER_HEXHEAT_OFFSET_MAX * s) kpix *= (double)SH4DER_HEXHEAT_OFFSET_MAX * s / o->maxoff;   /* never past OFFSET_MAX s */
    X = o->hit.X - slip_px / ppm; Y = o->hit.Y + drive_px / ppm;
    a = 1.0;
    for (i = 0; i < SH4DER_HEXHEAT_LAYERS; i++) {
        const double th = k * (X - U * t) + m * (Y - W * t) + PHI[i];
        dX -= A * a * k * cos(th); dY -= A * a * m * cos(th); lens += a * (k * k + m * m) * sin(th);
        k *= 2.0; m *= 2.0; a *= 0.5;
    }
    o->scale = 1.0 + (double)SH4DER_HEXHEAT_LENS * A * lens;
    o->ox = (dX * dot3(rg, c->r) + dY * dot3(c->n, c->r)) * kpix;
    o->oy = (dX * dot3(rg, c->dn) + dY * dot3(c->n, c->dn)) * kpix;
}

/* ---- the test ------------------------------------------------------------------------------------------------------- */
static sh4der_hexheat_vtx_t m1[SH4DER_HEXHEAT_CELLS_MAX * 6u], m2[SH4DER_HEXHEAT_CELLS_MAX * 6u];
static float was[SH4DER_HEXHEAT_CELLS_MAX][2];

/* A drawn hexagon's home on the screen and its cell (q, r): vertex 0 is the top corner. */
static void home_of(const sh4der_hexheat_vtx_t *v, float x0, float y0, double *hx, double *hy, int *q, int *r) {
    const double s = (double)SH4DER_HEXHEAT_S, hexw = s * sqrt(3.0);
    *hx = (double)v[0].u + (double)x0; *hy = (double)v[0].v + (double)y0 + s;
    *r = (int)floor(*hy / (1.5 * s) + 0.5); *q = (int)floor(*hx / hexw - 0.5 * (double)*r + 0.5);
}

int main(void) {
    const float s = SH4DER_HEXHEAT_S, bw = (float)SH4DER_HEXHEAT_BAND_W, bh = (float)SH4DER_HEXHEAT_BAND_H, dt = 1.0f / 60.0f;
    const double hexw = (double)s * sqrt(3.0);
    /* the street demo's high camera: 3.3 m up and 7 m behind the car, looking at a point 1 m up and 1.2 m ahead of it */
    const float eye[3] = { 2.0f, 3.3f, -7.0f }, target[3] = { 2.0f, 1.0f, 1.2f }, up[3] = { 0.0f, 1.0f, 0.0f }, nrm[3] = { 0.0f, 1.0f, 0.0f };
    const float fov = 0.96f, sheet = 70.0f, near_cut = 64.0f;
    sh4der_hexheat_view_t v, bad;
    sh4der_hexheat_t x;
    cam_t cam;
    float x0 = 0.0f, y0 = 0.0f, ax = 0.0f, ay = 0.0f, peak = 0.0f;
    unsigned i, k, n, fr;

    {   /* the phases' own largest sum, scanned: the header's table (A0 is set by it) */
        unsigned N;
        for (N = 1u; N <= 4u; N++) {
            double best = 0.0;
            unsigned j;
            for (j = 0; j < 400000u; j++) {
                const double u = 2.0 * PI * (double)j / 400000.0;
                double sum = 0.0;
                for (i = 0; i < N; i++) sum += cos((double)(1u << i) * u + PHI[i]);
                if (fabs(sum) > best) best = fabs(sum);
            }
            CHECK(fabs(best - (double)sh4der_hexheat_sum_[N - 1u]) < 2e-5 && (double)sh4der_hexheat_sum_[N - 1u] >= best - 1e-6);
            if (N == SH4DER_HEXHEAT_LAYERS) SMAX = best;
        }
        for (i = 0; i < 4u; i++) CHECK((double)sh4der_hexheat_phi_[i] == (double)(float)PHI[i]);
    }

    /* the lattice: x0 = s sqrt3 (q + r / 2), y0 = 1.5 s r; the same every call, bit for bit; no two cells alike */
    {
        int q, r;
        for (r = -3; r <= 40; r++) for (q = -25; q <= 50; q++) {
            float hx, hy, hx2, hy2, nx, ny;
            sh4der_hexheat_home(q, r, &hx, &hy);
            sh4der_hexheat_home(q, r, &hx2, &hy2);
            CHECK(!memcmp(&hx, &hx2, sizeof hx) && !memcmp(&hy, &hy2, sizeof hy));
            CHECK(fabs((double)hx - hexw * ((double)q + 0.5 * (double)r)) < 1e-3 && fabs((double)hy - 1.5 * (double)s * (double)r) < 1e-4);
            sh4der_hexheat_home(q + 1, r, &nx, &ny);
            CHECK(fabs((double)(nx - hx) - hexw) < 1e-3 && ny == hy);                      /* a neighbour: a hexagon's width on */
            sh4der_hexheat_home(q, r + 1, &nx, &ny);
            CHECK(fabs((double)(nx - hx) - 0.5 * hexw) < 1e-3 && fabs((double)(ny - hy) - 1.5 * (double)s) < 1e-4);
        }
    }

    /* the camera: refusals, then the same projection as the library's view rows (the street demo's) */
    CHECK(!sh4der_hexheat_view_look(&bad, eye, eye, up, fov, 640.0f, 480.0f) && !bad.ok);
    CHECK(!sh4der_hexheat_view_look(&bad, eye, target, up, 4.0f, 640.0f, 480.0f) && !sh4der_hexheat_view_look(&bad, eye, target, up, NAN, 640.0f, 480.0f));
    { const float down3[3] = { 2.0f, 0.0f, -7.0f }; CHECK(!sh4der_hexheat_view_look(&bad, eye, down3, up, fov, 640.0f, 480.0f)); }
    CHECK(sh4der_hexheat_view_look(&v, eye, target, up, fov, 640.0f, 480.0f) && !v.ok);
    CHECK(fabsf(v.focal - 240.0f / tanf(0.48f)) < 0.01f && fabsf(v.focal * v.inv_focal - 1.0f) < 1e-5f);
    {
        sh4der_pl4nar_row_t row[4];
        static const float pts[5][3] = { { 2.0f, 0.0f, 63.0f }, { -6.0f, 0.0f, 20.0f }, { 9.0f, 12.0f, 80.0f }, { 2.0f, 3.3f, 300.0f }, { 0.0f, 1.0f, 5.0f } };
        CHECK(sh4der_pl4nar_viewproj_rows(row, NULL, eye, target, up, fov, 0.3f, 640.0f, 480.0f));
        for (i = 0; i < 5u; i++) {
            const float *p = pts[i];
            const float w = row[3].x * p[0] + row[3].y * p[1] + row[3].z * p[2] + row[3].w;
            const float px = (row[0].x * p[0] + row[0].y * p[1] + row[0].z * p[2] + row[0].w) / w, py = (row[1].x * p[0] + row[1].y * p[1] + row[1].z * p[2] + row[1].w) / w;
            float sx = 0.0f, sy = 0.0f;
            CHECK(sh4der_hexheat_world_to_screen(&v, p, &sx, &sy) && fabsf(sx - px) < 0.02f && fabsf(sy - py) < 0.02f);
        }
        { const float behind[3] = { 2.0f, 3.3f, -20.0f }; float sx = 0.0f, sy = 0.0f; CHECK(!sh4der_hexheat_world_to_screen(&v, behind, &sx, &sy)); }
    }
    /* a hill: the plane through the sheet's foot is over the eye (a climb toward a crest). The view holds, no ray lands,
       every cell is on the sheet and the haze stays on: as many hexagons as with the eye just over the plane, none past
       the limit, and no jump in the count where the eye crosses the plane */
    {
        static sh4der_hexheat_vtx_t mh[SH4DER_HEXHEAT_CELLS_MAX * 6u];
        static const float lift[5] = { 3.28f, 3.3f, 3.32f, 5.0f, 9.0f };          /* the plane's height: the eye is at 3.3 */
        sh4der_hexheat_t still;
        sh4der_hexheat_hit_t hit;
        unsigned cnt[5];
        float p3[3] = { 7.0f, 7.0f, 7.0f }, dist = 7.0f, bx = 0.0f, by = 0.0f, fx = 0.0f, fy = 0.0f, pk = 0.0f;
        sh4der_hexheat_reset(&still);
        for (i = 0; i < 5u; i++) {
            const float foot[3] = { 2.0f, lift[i], 63.0f };
            CHECK(sh4der_hexheat_view_ground(&v, nrm, lift[i], sheet, near_cut) && v.ok && fabsf(v.e - (3.3f - lift[i])) < 1e-5f);
            CHECK(sh4der_hexheat_world_to_screen(&v, foot, &fx, &fy) && sh4der_hexheat_band(fx, fy, &bx, &by));
            cnt[i] = sh4der_hexheat_mesh(&still, &v, 0.5f, 1.0f, bx, by, mh, SH4DER_HEXHEAT_CELLS_MAX, &pk);
            CHECK(cnt[i] > 40u && pk > 0.5f && pk <= SH4DER_HEXHEAT_OFFSET_MAX * s);
            if (!(v.e > 0.0f)) {
                CHECK(!sh4der_hexheat_screen_to_ground(&v, 320.0f, 470.0f, p3, &dist) && p3[0] == 7.0f && dist == 7.0f);   /* no ray lands; nothing written */
                CHECK(sh4der_hexheat_screen_to_haze(&v, fx, fy - 20.0f, &hit) && hit.ground == 0 && hit.h > 0.0f);      /* over the foot: on the sheet */
                CHECK(!sh4der_hexheat_screen_to_haze(&v, fx, fy + 20.0f, &hit));                                       /* under it: no point */
            }
        }
        CHECK(cnt[0] > cnt[2] ? cnt[0] - cnt[2] <= 2u : cnt[2] - cnt[0] <= 2u);       /* 2 cm over the plane and 2 cm under it */
        printf("a hill, the plane 0.02 m under the eye, level, 0.02 m, 1.7 m and 5.7 m over it: %u, %u, %u, %u, %u hexagons (the haze never drops out)\n", cnt[0], cnt[1], cnt[2], cnt[3], cnt[4]);
        v.ok = 0;
    }
    /* the ground: refusals (a view straight down, the two distances out of order), then the real one */
    CHECK(!sh4der_hexheat_view_ground(&v, nrm, 0.0f, near_cut, sheet) && !sh4der_hexheat_view_ground(&v, nrm, 0.0f, sheet, 0.0f) && !sh4der_hexheat_view_ground(&v, nrm, 0.0f, NAN, near_cut));
    {
        const float lowt[3] = { 2.0f, 0.0f, -7.0f + 1e-5f };
        CHECK(sh4der_hexheat_view_look(&bad, eye, lowt, (const float[3]){ 0.0f, 0.0f, 1.0f }, fov, 640.0f, 480.0f) && !sh4der_hexheat_view_ground(&bad, nrm, 0.0f, sheet, near_cut));
    }
    CHECK(sh4der_hexheat_view_ground(&v, nrm, 0.0f, sheet, near_cut) && v.ok);
    cam_make(&cam, eye, target, up, (double)fov, 640.0, 480.0, nrm, 0.0, (double)sheet, (double)near_cut);
    {
        double g[3], rg[3], e, ppm;
        cam_ground(&cam, g, rg, &e, &ppm);
        CHECK(fabs((double)v.e - 3.3) < 1e-5 && fabs((double)v.ppm - ppm) < 1e-4 * ppm && fabs((double)v.w0 - cam.focal / ppm) < 1e-3);
        CHECK(fabs(1.0 / (double)v.inv_h - (double)SH4DER_HEXHEAT_LAYER_FRAC * 480.0 / ppm) < 1e-3);
        CHECK(fabs((double)v.h_cut - 3.3 * (1.0 - 70.0 / 64.0)) < 1e-5);
    }

    /* screen to world on the ground plane. A known point of the road -> its screen place -> the point again. */
    {
        static const float road[6][3] = { { 2.0f, 0.0f, 43.0f }, { -3.5f, 0.0f, 20.0f }, { 6.0f, 0.0f, 58.0f }, { 2.0f, 0.0f, 3.0f }, { 14.0f, 0.0f, 90.0f }, { -9.0f, 0.0f, 150.0f } };
        double worst_m = 0.0, worst_px = 0.0;
        for (i = 0; i < 6u; i++) {
            float sx = 0.0f, sy = 0.0f, p[3] = { 0.0f, 0.0f, 0.0f }, dist = 0.0f, bx = 0.0f, by = 0.0f;
            double rp[3], rd, e2;
            CHECK(sh4der_hexheat_world_to_screen(&v, road[i], &sx, &sy));
            CHECK(sh4der_hexheat_screen_to_ground(&v, sx, sy, p, &dist));
            e2 = sqrt(pow((double)p[0] - road[i][0], 2) + pow((double)p[1] - road[i][1], 2) + pow((double)p[2] - road[i][2], 2));
            if (e2 > worst_m) worst_m = e2;
            CHECK(e2 < 2e-4 * sqrt(pow((double)road[i][0] - eye[0], 2) + 3.3 * 3.3 + pow((double)road[i][2] - eye[2], 2)) + 1e-3);   /* 0.2 mm a metre of distance */
            CHECK(fabs((double)dist - sqrt(pow((double)road[i][0] - eye[0], 2) + 3.3 * 3.3 + pow((double)road[i][2] - eye[2], 2))) < 0.02);
            CHECK(ref_ground(&cam, (double)sx, (double)sy, rp, &rd) && fabs(rp[0] - p[0]) < 0.03 && fabs(rp[2] - p[2]) < 0.03 && fabs(rd - dist) < 0.03);
            CHECK(sh4der_hexheat_world_to_screen(&v, p, &bx, &by));                           /* and back to the screen */
            if (fabs((double)(bx - sx)) > worst_px) worst_px = fabs((double)(bx - sx));
            if (fabs((double)(by - sy)) > worst_px) worst_px = fabs((double)(by - sy));
        }
        CHECK(worst_px < 0.01);
        printf("ray to ground: a known road point comes back within %.5f m (20 to 150 m off); screen -> world -> screen within %.5f px\n", worst_m, worst_px);
        {   /* the horizon and over it: the ray never lands, nothing is written */
            float hz[3] = { 2.0f, 3.3f, 5000.0f }, sx = 0.0f, sy = 0.0f, p[3] = { 7.0f, 8.0f, 9.0f }, dist = 5.0f;
            CHECK(sh4der_hexheat_world_to_screen(&v, hz, &sx, &sy));
            CHECK(!sh4der_hexheat_screen_to_ground(&v, sx, sy, p, &dist) && !sh4der_hexheat_screen_to_ground(&v, sx, sy - 40.0f, p, &dist));
            CHECK(p[0] == 7.0f && p[1] == 8.0f && p[2] == 9.0f && dist == 5.0f);
            CHECK(sh4der_hexheat_screen_to_ground(&v, sx, sy + 3.0f, p, NULL) && fabsf(p[1]) < 0.05f);        /* just under it: far down the road */
            CHECK(!sh4der_hexheat_screen_to_ground(&bad, sx, sy + 3.0f, p, NULL));                           /* a view that was refused */
        }
    }

    /* the haze's point: the ground short of the sheet (the same point the pick gives), else the sheet */
    {
        const float near_pt[3] = { 5.0f, 0.0f, 30.0f }, far_pt[3] = { -4.0f, 0.0f, 200.0f }, wall[3] = { 8.0f, 9.0f, 120.0f };
        sh4der_hexheat_hit_t hit, hit2;
        rhit_t rh;
        float sx = 0.0f, sy = 0.0f, p[3] = { 0.0f, 0.0f, 0.0f }, dist = 0.0f;
        memset(&hit, 0, sizeof hit); memset(&hit2, 0, sizeof hit2); memset(&rh, 0, sizeof rh);
        CHECK(sh4der_hexheat_world_to_screen(&v, near_pt, &sx, &sy) && sh4der_hexheat_screen_to_haze(&v, sx, sy, &hit) && sh4der_hexheat_screen_to_ground(&v, sx, sy, p, &dist));
        CHECK(hit.ground == 1 && hit.h == 0.0f && hit.hs < 0.0f && fabsf(hit.p[0] - p[0]) < 1e-3f && fabsf(hit.p[2] - p[2]) < 1e-3f && fabsf(hit.dist - dist) < 1e-3f);
        CHECK(fabsf(hit.up - (37.0f - sheet)) < 0.02f && fabsf(hit.lat - (near_pt[0] - eye[0]) * -1.0f) < 0.02f);   /* 37 m ahead; the world's +x is the screen's left */
        CHECK(ref_haze(&cam, (double)sx, (double)sy, &rh) && rh.ground == 1 && fabs(rh.X - hit.lat) < 0.02 && fabs(rh.Y - hit.up) < 0.02 && fabs(rh.dist - hit.dist) < 0.02);
        /* a road point beyond the sheet: the ray passes over the sheet's foot; the point is on the sheet, h over the road */
        CHECK(sh4der_hexheat_world_to_screen(&v, far_pt, &sx, &sy) && sh4der_hexheat_screen_to_haze(&v, sx, sy, &hit));
        CHECK(hit.ground == 0 && fabsf(hit.p[2] - (eye[2] + sheet)) < 0.01f && fabsf(hit.h - hit.p[1]) < 1e-4f && hit.h > 0.0f && hit.h < 3.3f && hit.up == hit.h);
        CHECK(fabsf(hit.h - 3.3f * (1.0f - 70.0f / 207.0f)) < 0.01f);                         /* the ray to a point 207 m ahead, at 70 m */
        CHECK(fabs((double)hit.dist - sqrt(pow((double)hit.p[0] - eye[0], 2) + pow((double)hit.p[1] - eye[1], 2) + pow((double)hit.p[2] - eye[2], 2))) < 0.01);
        CHECK(ref_haze(&cam, (double)sx, (double)sy, &rh) && rh.ground == 0 && fabs(rh.h - hit.h) < 0.01 && fabs(rh.X - hit.lat) < 0.02 && fabs(rh.dist - hit.dist) < 0.02 && fabs(rh.w - hit.w) < 0.02);
        /* a point over the horizon (a far wall): the ray never lands; the point is on the sheet all the same */
        CHECK(sh4der_hexheat_world_to_screen(&v, wall, &sx, &sy) && !sh4der_hexheat_screen_to_ground(&v, sx, sy, p, &dist) && sh4der_hexheat_screen_to_haze(&v, sx, sy, &hit));
        CHECK(hit.ground == 0 && hit.h > 3.3f && fabsf(hit.p[2] - (eye[2] + sheet)) < 0.01f);
        CHECK(fabsf(hit.h - (3.3f + (9.0f - 3.3f) * 70.0f / 127.0f)) < 0.02f);                /* the line from the eye to the wall, at 70 m */
        /* one piece at the join: a pixel each side of the sheet's foot */
        {
            const float foot[3] = { 2.0f, 0.0f, eye[2] + sheet };
            CHECK(sh4der_hexheat_world_to_screen(&v, foot, &sx, &sy) && sh4der_hexheat_screen_to_haze(&v, sx, sy + 0.5f, &hit) && sh4der_hexheat_screen_to_haze(&v, sx, sy - 0.5f, &hit2));
            CHECK(hit.ground == 1 && hit2.ground == 0 && fabsf(hit.up - hit2.up) < 2.5f && fabsf(hit.lat - hit2.lat) < 0.05f && fabsf(hit.dist - hit2.dist) < 2.5f && fabsf(hit.w - v.w0) < 2.5f);
            CHECK(fabsf(sx - 320.0f) < 0.01f);
            ax = sx; ay = sy;                                                             /* the band's anchor */
        }
        /* a camera looking steeply down: a ray off the bottom of the screen goes back along the ground: no point */
        {
            const float t2[3] = { 2.0f, 0.0f, -6.6f };
            CHECK(sh4der_hexheat_view_look(&bad, eye, t2, up, 2.0f, 640.0f, 480.0f) && sh4der_hexheat_view_ground(&bad, nrm, 0.0f, sheet, near_cut));
            CHECK(!sh4der_hexheat_screen_to_haze(&bad, 320.0f, 479.0f, &hit));
        }
    }

    /* the field: the offset is minus A times the gradient of T' (the numeric derivative, metres at 1 px a metre) */
    sh4der_hexheat_reset(&x);
    {
        double gworst = 0.0;
        for (i = 0; i < 60u; i++) {
            const double X = -120.0 + 4.3 * (double)i, Y = -30.0 + 2.9 * (double)i, t = 3.7, e = 1e-3;
            const double gx = (ref_T(X + e, Y, t, hexw) - ref_T(X - e, Y, t, hexw)) / (2.0 * e), gy = (ref_T(X, Y + e, t, hexw) - ref_T(X, Y - e, t, hexw)) / (2.0 * e);
            float d[2], d2[2], sc, sc2;
            sh4der_hexheat_field(&x, (float)X, (float)Y, (float)t, 1.0f, d, &sc);
            sh4der_hexheat_field(&x, (float)X, (float)Y, (float)t, 1.0f, d2, &sc2);
            CHECK(d[0] == d2[0] && d[1] == d2[1] && sc == sc2);                               /* the same t: the same offset */
            if (fabs((double)d[0] + gx) > gworst) gworst = fabs((double)d[0] + gx);
            if (fabs((double)d[1] + gy) > gworst) gworst = fabs((double)d[1] + gy);
        }
        CHECK(gworst < 2e-3);
        printf("gradient against the numeric derivative of T': largest difference %.6f (A = 1)\n", gworst);
    }
    /* A0: tuned so that the largest offset is PEAK s at the sheet's foot (a scan of the wave there), never more */
    {
        const float dfoot = sqrtf(sheet * sheet + 3.3f * 3.3f), Afoot = sh4der_hexheat_amp(&v, dfoot, 1.0f);
        double big = 0.0;
        for (i = 0; i < 200000u; i++) {
            float d[2];
            sh4der_hexheat_field(&x, (float)i * 0.0137f, 0.0f, 0.0f, Afoot, d, NULL);
            if (sqrt((double)d[0] * d[0] + (double)d[1] * d[1]) > big) big = sqrt((double)d[0] * d[0] + (double)d[1] * d[1]);
        }
        CHECK(big <= (double)SH4DER_HEXHEAT_PEAK * (double)s + 1e-3 && big > 0.995 * (double)SH4DER_HEXHEAT_PEAK * (double)s);
        CHECK(fabs((double)Afoot - (double)v.a0 * (1.0 - exp(-(double)dfoot / (double)SH4DER_HEXHEAT_D0))) < (1e-4 + TOL_REL) * (double)v.a0);
        CHECK(fabs((double)Afoot * (double)v.grad - (double)SH4DER_HEXHEAT_PEAK * (double)s) < 1e-3);
        CHECK(sh4der_hexheat_amp(&v, 0.0f, 1.0f) == 0.0f && sh4der_hexheat_amp(&v, 70.0f, 0.0f) == 0.0f && sh4der_hexheat_amp(&v, NAN, 1.0f) == 0.0f);
        CHECK(sh4der_hexheat_amp(&v, 70.0f, 3.0f) == sh4der_hexheat_amp(&v, 70.0f, 1.0f) && sh4der_hexheat_amp(&v, 30.0f, 1.0f) < sh4der_hexheat_amp(&v, 70.0f, 1.0f));
        { sh4der_hexheat_view_t off = v; off.ok = 0; CHECK(sh4der_hexheat_amp(&off, 70.0f, 1.0f) == 0.0f); }   /* a view that was refused */
    }
    /* the slip and the drive: a slip of p moves the pattern p to the right; a drive moves it down; cuts are not summed; wrapped */
    {
        sh4der_hexheat_t y;
        float a[2], b[2];
        sh4der_hexheat_reset(&y);
        sh4der_hexheat_slip(&y, 5.0f);
        sh4der_hexheat_field(&y, 105.0f, 20.0f, 2.0f, 9.0f, a, NULL);
        sh4der_hexheat_field(&x, 100.0f, 20.0f, 2.0f, 9.0f, b, NULL);
        CHECK(fabsf(a[0] - b[0]) < 0.01f && fabsf(a[1] - b[1]) < 0.01f);
        sh4der_hexheat_reset(&y);
        sh4der_hexheat_drive(&y, 0.5f, 6.0f);                                             /* 0.5 m at 6 px a metre: 3 px x DRIVE */
        sh4der_hexheat_field(&y, 100.0f, 20.0f, 2.0f, 9.0f, a, NULL);
        sh4der_hexheat_field(&x, 100.0f, 20.0f + 3.0f * SH4DER_HEXHEAT_DRIVE, 2.0f, 9.0f, b, NULL);
        CHECK(fabsf(a[0] - b[0]) < 0.01f && fabsf(a[1] - b[1]) < 0.01f);
        sh4der_hexheat_slip(&y, 5000.0f); sh4der_hexheat_slip(&y, NAN); sh4der_hexheat_drive(&y, NAN, 6.0f); sh4der_hexheat_drive(&y, 50.0f, 6.0f); sh4der_hexheat_drive(&y, 0.5f, NAN);
        CHECK(y.sx == 0.0f && fabsf(y.sy - 3.0f * SH4DER_HEXHEAT_DRIVE) < 1e-4f);
        {   /* a frame's step is held at a quarter of the finest wave, both ways */
            const float most = sh4der_hexheat_ly_() * 0.25f / (float)(1u << (SH4DER_HEXHEAT_LAYERS - 1u)), was_sy = y.sy;
            sh4der_hexheat_drive(&y, 7.9f, 60.0f);
            CHECK(fabsf(y.sy - (was_sy + most)) < 1e-3f && most > 1.0f && SH4DER_HEXHEAT_DRIVE * 7.9f * 60.0f > most);
            sh4der_hexheat_drive(&y, -7.9f, 60.0f);
            CHECK(fabsf(y.sy - was_sy) < 1e-3f);
        }
        for (fr = 0; fr < 100000u; fr++) { sh4der_hexheat_drive(&y, 0.4f, 6.0f); sh4der_hexheat_slip(&y, -3.0f); }
        CHECK(y.sy >= 0.0f && y.sy <= sh4der_hexheat_ly_() + 1e-3f && y.sx >= 0.0f && y.sx <= sh4der_hexheat_lx_() + 1e-3f);   /* wrapped: no growth */
    }

    /* the camera since the frame before (sh4der_hexheat_follow): a turn takes the pattern with the far picture, the way
       forward streams it down, a cut is not summed */
    {
        const float yaw = 0.02f;                                                          /* a turn of about 1.1 degrees */
        const float tgt2[3] = { eye[0] + 8.2f * sinf(yaw), 1.0f, eye[2] + 8.2f * cosf(yaw) };
        const float far_pt[3] = { eye[0] - 3.0f, 0.4f, eye[2] + sheet };                   /* on the sheet, just over its foot */
        const float eye3[3] = { eye[0], eye[1], eye[2] + 0.5f }, tgt3[3] = { target[0], target[1], target[2] + 0.5f };
        const float eye4[3] = { eye[0], eye[1], eye[2] + 50.0f }, tgt4[3] = { target[0], target[1], target[2] + 50.0f };
        const float tgt5[3] = { eye[0] + 8.2f, 1.0f, eye[2] - 3.0f };                       /* most of a half turn */
        sh4der_hexheat_view_t v2, v3;
        sh4der_hexheat_t y;
        sh4der_hexheat_hit_t h0, h1;
        float sx = 0.0f, sy = 0.0f, sx2 = 0.0f, sy2 = 0.0f, d0[2], d1[2], A, moved;
        memset(&h0, 0, sizeof h0); memset(&h1, 0, sizeof h1);
        CHECK(sh4der_hexheat_view_look(&v2, eye, tgt2, up, fov, 640.0f, 480.0f) && sh4der_hexheat_view_ground(&v2, nrm, 0.0f, sheet, near_cut));
        sh4der_hexheat_reset(&y);
        CHECK(sh4der_hexheat_world_to_screen(&v, far_pt, &sx, &sy) && sh4der_hexheat_screen_to_haze(&v, sx, sy, &h0));
        A = sh4der_hexheat_amp(&v, h0.dist, 1.0f);
        sh4der_hexheat_field(&y, h0.lat * v.ppm, h0.up * v.ppm, 1.5f, A, d0, NULL);
        sh4der_hexheat_follow(&y, &v2, v.g, v.eye);
        CHECK(sh4der_hexheat_world_to_screen(&v2, far_pt, &sx2, &sy2) && sh4der_hexheat_screen_to_haze(&v2, sx2, sy2, &h1));
        moved = sx2 - sx;                                                                 /* the far point on the screen: the turn's slip */
        CHECK(fabsf(moved) > 8.0f && fabsf(y.sx - sh4der_hexheat_wrap_(moved, sh4der_hexheat_lx_())) < 0.15f && fabsf(y.sy) < 1e-3f);
        sh4der_hexheat_field(&y, h1.lat * v2.ppm, h1.up * v2.ppm, 1.5f, A, d1, NULL);
        CHECK(fabsf(d1[0] - d0[0]) < 0.02f && fabsf(d1[1] - d0[1]) < 0.02f);                /* its offset is the same: the pattern went with it */
        printf("a turn that moves the far picture %.2f px: the pattern follows (a far point's offset changes by %.4f px)\n",
               (double)moved, (double)fmaxf(fabsf(d1[0] - d0[0]), fabsf(d1[1] - d0[1])));
        sh4der_hexheat_reset(&y);
        CHECK(sh4der_hexheat_view_look(&v3, eye3, tgt3, up, fov, 640.0f, 480.0f) && sh4der_hexheat_view_ground(&v3, nrm, 0.0f, sheet, near_cut));
        sh4der_hexheat_follow(&y, &v3, v.g, v.eye);                                       /* half a metre forward */
        CHECK(fabsf(y.sy - SH4DER_HEXHEAT_DRIVE * 0.5f * v.ppm) < 1e-3f && fabsf(y.sx) < 1e-3f);
        CHECK(sh4der_hexheat_view_look(&v3, eye4, tgt4, up, fov, 640.0f, 480.0f) && sh4der_hexheat_view_ground(&v3, nrm, 0.0f, sheet, near_cut));
        sh4der_hexheat_reset(&y);
        sh4der_hexheat_follow(&y, &v3, v.g, v.eye);                                       /* 50 m at once: a cut */
        CHECK(y.sy == 0.0f && y.sx == 0.0f);
        CHECK(sh4der_hexheat_view_look(&v3, eye, tgt5, up, fov, 640.0f, 480.0f) && sh4der_hexheat_view_ground(&v3, nrm, 0.0f, sheet, near_cut));
        sh4der_hexheat_follow(&y, &v3, v.g, v.eye);                                       /* a turn past a right angle: a cut */
        CHECK(y.sx == 0.0f);
        { sh4der_hexheat_view_t off = v2; off.ok = 0; sh4der_hexheat_follow(&y, &off, v.g, v.eye); CHECK(y.sx == 0.0f && y.sy == 0.0f); }
    }

    /* the mesh, the demo's camera standing still: every drawn hexagon against the transcription, 10 s at 60 a second */
    CHECK(sh4der_hexheat_band(ax, ay, &x0, &y0) && x0 == 192.0f && y0 == (float)(int)(ay + 0.5f) + SH4DER_HEXHEAT_FOOT - bh);
    {
        double worst = 0.0, worst_sc = 0.0, big = 0.0, step = 0.0, lo = 1.0, hi = 1.0;
        unsigned n0 = 0, on_ground = 0, misses = 0, bad = 0;
        for (fr = 0; fr < 600u; fr++) {
            const float t = (float)fr * dt;
            n = sh4der_hexheat_mesh(&x, &v, t, 1.0f, x0, y0, m1, SH4DER_HEXHEAT_CELLS_MAX, &peak);
            if (!fr) { n0 = n; CHECK(n > 40u && n < SH4DER_HEXHEAT_CELLS_MAX); memcpy(m2, m1, sizeof m1); }
            CHECK(n == n0);                                                               /* the same cells every frame: nothing pops */
            for (i = 0; i < n; i++) {
                const sh4der_hexheat_vtx_t *h = &m1[i * 6u];
                double hx, hy, mx = 0.0, my = 0.0, off;
                float lx, ly;
                rcell_t rc;
                int q, r;
                home_of(h, x0, y0, &hx, &hy, &q, &r);
                sh4der_hexheat_home(q, r, &lx, &ly);
                CHECK(fabs((double)lx - hx) < 2e-3 && fabs((double)ly - hy) < 2e-3);            /* its home is the lattice's */
                for (k = 0; k < 6u; k++) {
                    CHECK(h[k].u == m2[i * 6u + k].u && h[k].v == m2[i * 6u + k].v);        /* the picture a cell carries never moves: bit equal */
                    CHECK(h[k].u >= 1.0f - 2e-3f && h[k].u <= bw - 1.0f + 2e-3f && h[k].v >= 1.0f - 2e-3f && h[k].v <= bh - 1.0f + 2e-3f);
                    mx += (double)h[k].x; my += (double)h[k].y;
                }
                mx = mx / 6.0 - hx; my = my / 6.0 - hy;                                    /* position - home = offset(t) */
                ref_cell(&cam, hx, hy, (double)t, 1.0, (double)x0, (double)y0, 0.0, 0.0, &rc);
                CHECK(rc.inside && rc.maxoff > (0.98 - TOL_REL) * (double)SH4DER_HEXHEAT_CUT);
                if (fabs(mx - rc.ox) > worst) worst = fabs(mx - rc.ox);
                if (fabs(my - rc.oy) > worst) worst = fabs(my - rc.oy);
                bad += over(mx - rc.ox, rc.maxoff) > 0.0 || over(my - rc.oy, rc.maxoff) > 0.0;
                {
                    const double sc = ((double)h[5].y - (double)h[0].y) / (2.0 * (double)s);
                    if (fabs(sc - rc.scale) > worst_sc) worst_sc = fabs(sc - rc.scale);
                    if (sc < lo) lo = sc;
                    if (sc > hi) hi = sc;
                }
                off = sqrt(mx * mx + my * my);
                if (off > big) big = off;
                if (fr) { const double ch = sqrt(pow(mx - (double)was[i][0], 2) + pow(my - (double)was[i][1], 2)); if (ch > step) step = ch; }
                was[i][0] = (float)mx; was[i][1] = (float)my;
                if (!fr && rc.hit.ground) on_ground++;
            }
            CHECK(fabs((double)peak - 0.0) >= 0.0 && (double)peak <= (double)SH4DER_HEXHEAT_OFFSET_MAX * (double)s);
        }
        /* and no cell the transcription draws is left out */
        {
            int q, r;
            for (r = 0; r < 40; r++) for (q = -20; q < 48; q++) {
                rcell_t rc;
                ref_cell(&cam, hexw * ((double)q + 0.5 * (double)r), 1.5 * (double)s * (double)r, 0.0, 1.0, (double)x0, (double)y0, 0.0, 0.0, &rc);
                if (rc.inside && rc.maxoff > (1.02 + 2.0 * TOL_REL) * (double)SH4DER_HEXHEAT_CUT) {
                    unsigned found = 0;
                    for (i = 0; i < n0 && !found; i++) { double hx, hy; int q2, r2; home_of(&m2[i * 6u], x0, y0, &hx, &hy, &q2, &r2); found = q2 == q && r2 == r; }
                    misses += !found;
                }
            }
            CHECK(misses == 0u);
        }
        CHECK(bad == 0u && worst_sc < 0.01);
        CHECK(big > 0.5 && big < (double)s / 3.0 && big <= (double)SH4DER_HEXHEAT_PEAK * (double)s * 1.05);   /* a hexagon's middle stays in its own cell */
        CHECK(step < 0.04 * (double)(1u << SH4DER_HEXHEAT_LAYERS));                       /* a smooth field: no jump a frame (0.32 px for 3 layers: 0.126 there; the finest layer doubles it, 0.254 for 4) */
        CHECK(lo > 0.9 && hi < 1.1);
        printf("the demo's view (eye 3.3 m up, sheet 70 m, near cut 64 m): %u hexagons a frame (%u on the ground short of the sheet), %u strip vertices, %.1f KB of TA with the header\n",
               n0, on_ground, n0 * 6u, ((double)(n0 * 6u) + 1.0) * 32.0 / 1024.0);
        printf("offset, camera still: peak %.2f px (the aim at the sheet's foot: %.2f; the limit OFFSET_MAX s = %.2f); largest change a frame at 60 Hz %.3f px; scale %.3f .. %.3f\n",
               big, (double)SH4DER_HEXHEAT_PEAK * (double)s, (double)SH4DER_HEXHEAT_OFFSET_MAX * (double)s, step, lo, hi);
        printf("float path against the world-unit transcription: offset %.5f px, scale %.5f (allowed: %.3f px + %.0f %% of a cell's largest offset)\n", worst, worst_sc, TOL, 100.0 * TOL_REL);
    }

    /* driving: the camera goes 14 and 40 m/s along the road; the two sums against the transcription, the change a frame */
    {
        static const float speed[2] = { 14.0f, 40.0f };
        unsigned sp;
        for (sp = 0; sp < 2u; sp++) {
            double worst = 0.0, step = 0.0, drive = 0.0, slip = 0.0;
            unsigned bad = 0;
            sh4der_hexheat_reset(&x);
            for (fr = 0; fr < 300u; fr++) {
                const float t = (float)fr * dt;
                if (fr) {
                    sh4der_hexheat_drive(&x, speed[sp] * dt, v.ppm);
                    drive += fmin((double)SH4DER_HEXHEAT_DRIVE * (double)(speed[sp] * dt) * (double)v.ppm,
                                  (double)sh4der_hexheat_ly_() * 0.25 / (double)(1u << (SH4DER_HEXHEAT_LAYERS - 1u)));
                    sh4der_hexheat_slip(&x, 0.3f); slip += 0.3;
                }
                n = sh4der_hexheat_mesh(&x, &v, t, 1.0f, x0, y0, m1, SH4DER_HEXHEAT_CELLS_MAX, NULL);
                for (i = 0; i < n; i++) {
                    const sh4der_hexheat_vtx_t *h = &m1[i * 6u];
                    double hx, hy, mx = 0.0, my = 0.0;
                    rcell_t rc;
                    int q, r;
                    home_of(h, x0, y0, &hx, &hy, &q, &r);
                    for (k = 0; k < 6u; k++) { mx += (double)h[k].x; my += (double)h[k].y; }
                    mx = mx / 6.0 - hx; my = my / 6.0 - hy;
                    ref_cell(&cam, hx, hy, (double)t, 1.0, (double)x0, (double)y0, slip, drive, &rc);
                    if (fabs(mx - rc.ox) > worst) worst = fabs(mx - rc.ox);
                    if (fabs(my - rc.oy) > worst) worst = fabs(my - rc.oy);
                    bad += over(mx - rc.ox, rc.maxoff) > 0.0 || over(my - rc.oy, rc.maxoff) > 0.0;
                    if (fr) { const double ch = sqrt(pow(mx - (double)was[i][0], 2) + pow(my - (double)was[i][1], 2)); if (ch > step) step = ch; }
                    was[i][0] = (float)mx; was[i][1] = (float)my;
                }
            }
            CHECK(bad == 0u && step < 1.5);
            printf("driving %.0f m/s with the far picture slipping 18 px a second (the pattern streams down %.0f px a second at the sheet): largest change a frame %.3f px; against the transcription %.5f px\n",
                   (double)speed[sp], fmin((double)(SH4DER_HEXHEAT_DRIVE * speed[sp] * v.ppm), 60.0 * (double)sh4der_hexheat_ly_() * 0.25 / (double)(1u << (SH4DER_HEXHEAT_LAYERS - 1u))), step, worst);
        }
        sh4der_hexheat_reset(&x);
    }

    /* A property of the doubling rule, pinned so nobody is surprised by it: k and m double together, so every wave runs
       along one line and the field is stripes. The stripes cross the screen at the rise plus the wind's share; a car's
       stream takes that away, and at one speed (no turn) they stand still: every offset holds. Slower they climb, faster
       they run down. (The 14 m/s run above sits near it only because of its slip.) */
    if (SH4DER_HEXHEAT_DRIVE > 0.0f) {
        const float v0 = (sh4der_hexheat_w_() + sh4der_hexheat_u_() * sh4der_hexheat_k1_() / sh4der_hexheat_m1_()) / (SH4DER_HEXHEAT_DRIVE * v.ppm);
        static const float part[3] = { 0.0f, 1.0f, 2.0f };
        double step[3] = { 0.0, 0.0, 0.0 };
        unsigned sp;
        for (sp = 0; sp < 3u; sp++) {
            sh4der_hexheat_reset(&x);
            for (fr = 0; fr < 120u; fr++) {
                if (fr) sh4der_hexheat_drive(&x, part[sp] * v0 * dt, v.ppm);
                n = sh4der_hexheat_mesh(&x, &v, (float)fr * dt, 1.0f, x0, y0, m1, SH4DER_HEXHEAT_CELLS_MAX, NULL);
                for (i = 0; i < n; i++) {
                    const sh4der_hexheat_vtx_t *h = &m1[i * 6u];
                    double hx, hy, mx = 0.0, my = 0.0;
                    int q, r;
                    home_of(h, x0, y0, &hx, &hy, &q, &r);
                    for (k = 0; k < 6u; k++) { mx += (double)h[k].x; my += (double)h[k].y; }
                    mx = mx / 6.0 - hx; my = my / 6.0 - hy;
                    if (fr) { const double ch = sqrt(pow(mx - (double)was[i][0], 2) + pow(my - (double)was[i][1], 2)); if (ch > step[sp]) step[sp] = ch; }
                    was[i][0] = (float)mx; was[i][1] = (float)my;
                }
            }
        }
        CHECK(step[1] < 0.02 && step[0] > 0.05 && step[2] > 0.05);                        /* still at v0; moving at rest and at twice v0 */
        printf("the stripes stand still at %.1f m/s (%.0f km/h, no turn): largest change a frame %.4f px there, %.3f px at rest, %.3f px at twice that speed\n",
               (double)v0, (double)v0 * 3.6, step[1], step[0], step[2]);
        sh4der_hexheat_reset(&x);
    }

    /* the gate; the band a pixel down (the homes stay on the screen); a view that was refused */
    CHECK(sh4der_hexheat_mesh(&x, &v, 1.0f, 0.0f, x0, y0, m1, SH4DER_HEXHEAT_CELLS_MAX, &peak) == 0u && peak == 0.0f);
    CHECK(sh4der_hexheat_mesh(&x, &v, 1.0f, NAN, x0, y0, m1, SH4DER_HEXHEAT_CELLS_MAX, NULL) == 0u);
    CHECK(sh4der_hexheat_mesh(&x, &bad, 1.0f, 1.0f, x0, y0, m1, SH4DER_HEXHEAT_CELLS_MAX, NULL) <= SH4DER_HEXHEAT_CELLS_MAX);
    { sh4der_hexheat_view_t off = v; off.ok = 0; CHECK(sh4der_hexheat_mesh(&x, &off, 1.0f, 1.0f, x0, y0, m1, SH4DER_HEXHEAT_CELLS_MAX, NULL) == 0u); }
    CHECK(sh4der_hexheat_mesh(&x, &v, 1.0f, 1.0f, x0, y0, m1, 7u, NULL) == 7u);            /* the caller's room is kept */
    {
        float pk1, pk5;
        const unsigned full = sh4der_hexheat_mesh(&x, &v, 1.0f, 1.0f, x0, y0, m1, SH4DER_HEXHEAT_CELLS_MAX, &pk1);
        const unsigned half = sh4der_hexheat_mesh(&x, &v, 1.0f, 0.5f, x0, y0, m2, SH4DER_HEXHEAT_CELLS_MAX, &pk5);
        CHECK(half > 0u && half < full && fabsf(pk5 - 0.5f * pk1) < 0.02f);                /* half the gate: half the offsets, fewer cells reach CUT */
        n = sh4der_hexheat_mesh(&x, &v, 1.0f, 1.0f, x0, y0 + 1.0f, m2, SH4DER_HEXHEAT_CELLS_MAX, NULL);
        CHECK(n > 0u);
        for (i = 0; i < n; i++) {                                                         /* the band moved: every home is still a lattice point of the screen */
            double hx, hy;
            float lx, ly;
            int q, r;
            home_of(&m2[i * 6u], x0, y0 + 1.0f, &hx, &hy, &q, &r);
            sh4der_hexheat_home(q, r, &lx, &ly);
            CHECK(fabs((double)lx - hx) < 2e-3 && fabs((double)ly - hy) < 2e-3);
        }
    }

    /* a high camera with the cut near (eye 9 m up, sheet 70 m, near cut 25 m): cells on the ground at many distances.
       Their offsets are larger by focal / D, and still under OFFSET_MAX s; each against the transcription. */
    {
        const float eye2[3] = { 0.0f, 9.0f, 0.0f }, tgt2[3] = { 0.0f, 0.0f, 45.0f };
        sh4der_hexheat_view_t v2;
        cam_t cam2;
        double worst = 0.0, big = 0.0, kbig = 0.0;
        unsigned grounded = 0, bad = 0;
        CHECK(sh4der_hexheat_view_look(&v2, eye2, tgt2, up, fov, 640.0f, 480.0f) && sh4der_hexheat_view_ground(&v2, nrm, 0.0f, 70.0f, 25.0f));
        cam_make(&cam2, eye2, tgt2, up, (double)fov, 640.0, 480.0, nrm, 0.0, 70.0, 25.0);
        for (fr = 0; fr < 120u; fr++) {
            n = sh4der_hexheat_mesh(&x, &v2, (float)fr * dt, 1.0f, 192.0f, 150.0f, m1, SH4DER_HEXHEAT_CELLS_MAX, &peak);
            CHECK(n > 20u);
            for (i = 0; i < n; i++) {
                const sh4der_hexheat_vtx_t *h = &m1[i * 6u];
                double hx, hy, mx = 0.0, my = 0.0;
                rcell_t rc;
                int q, r;
                home_of(h, 192.0f, 150.0f, &hx, &hy, &q, &r);
                for (k = 0; k < 6u; k++) { mx += (double)h[k].x; my += (double)h[k].y; }
                mx = mx / 6.0 - hx; my = my / 6.0 - hy;
                ref_cell(&cam2, hx, hy, (double)((float)fr * dt), 1.0, 192.0, 150.0, 0.0, 0.0, &rc);
                if (fabs(mx - rc.ox) > worst) worst = fabs(mx - rc.ox);
                if (fabs(my - rc.oy) > worst) worst = fabs(my - rc.oy);
                bad += over(mx - rc.ox, fmin(rc.maxoff, (double)SH4DER_HEXHEAT_OFFSET_MAX * (double)s)) > 0.0 || over(my - rc.oy, fmin(rc.maxoff, (double)SH4DER_HEXHEAT_OFFSET_MAX * (double)s)) > 0.0;
                if (sqrt(mx * mx + my * my) > big) big = sqrt(mx * mx + my * my);
                if (!fr && rc.hit.ground) { grounded++; if ((double)v2.w0 / rc.hit.w > kbig) kbig = (double)v2.w0 / rc.hit.w; }
            }
            CHECK((double)peak <= (double)SH4DER_HEXHEAT_OFFSET_MAX * (double)s + 1e-3);
        }
        CHECK(bad == 0u && grounded > 10u && kbig > 1.2 && big <= (double)SH4DER_HEXHEAT_OFFSET_MAX * (double)s + 1e-3);
        printf("a high camera, the cut at 25 m: %u cells on the ground, the nearest's offsets x %.2f (focal / D), held at x %.2f; peak %.2f px (at most OFFSET_MAX s = %.2f); against the transcription %.5f px\n",
               grounded, kbig, (double)(SH4DER_HEXHEAT_OFFSET_MAX / SH4DER_HEXHEAT_PEAK), big, (double)SH4DER_HEXHEAT_OFFSET_MAX * (double)s, worst);
    }

    /* the band's bottom edge: the same high camera, the band where sh4der_hexheat_band puts it (the sheet's foot), the
       camera pitching slowly. The near cut's line is far under the band's lowest home there, so the offsets must be 0 at
       the lowest home instead: a cell that comes or goes, at any edge, does it with a small offset (the float path alone:
       the transcription has no band edge). */
    {
        static int hq[2][SH4DER_HEXHEAT_CELLS_MAX], hr[2][SH4DER_HEXHEAT_CELLS_MAX];
        static double ho[2][SH4DER_HEXHEAT_CELLS_MAX];
        const float eye2[3] = { 0.0f, 9.0f, 0.0f }, foot[3] = { 0.0f, 0.0f, 70.0f };
        unsigned cnt[2] = { 0u, 0u }, cur = 0u, changed = 0u, j;
        double worst = 0.0;
        for (fr = 0; fr < 600u; fr++, cur ^= 1u) {
            const float tgt2[3] = { 0.0f, 0.0f, 45.0f + 15.0f * (float)fr * (1.0f / 600.0f) };
            sh4der_hexheat_view_t v2;
            float fx, fy, bx = 0.0f, by = 0.0f;
            CHECK(sh4der_hexheat_view_look(&v2, eye2, tgt2, up, fov, 640.0f, 480.0f) && sh4der_hexheat_view_ground(&v2, nrm, 0.0f, 70.0f, 25.0f));
            CHECK(sh4der_hexheat_world_to_screen(&v2, foot, &fx, &fy) && sh4der_hexheat_band(fx, fy, &bx, &by));
            n = sh4der_hexheat_mesh(&x, &v2, (float)fr * dt, 1.0f, bx, by, m1, SH4DER_HEXHEAT_CELLS_MAX, NULL);
            CHECK(n > 20u);
            for (i = 0; i < n; i++) {
                const sh4der_hexheat_vtx_t *h = &m1[i * 6u];
                double hx, hy, mx = 0.0, my = 0.0;
                home_of(h, bx, by, &hx, &hy, &hq[cur][i], &hr[cur][i]);
                for (k = 0; k < 6u; k++) { mx += (double)h[k].x; my += (double)h[k].y; }
                mx = mx / 6.0 - hx; my = my / 6.0 - hy;
                ho[cur][i] = sqrt(mx * mx + my * my);
            }
            cnt[cur] = n;
            if (fr) for (k = 0; k < 2u; k++) {                 /* k 0: this frame's cells the last had not; 1: the reverse */
                const unsigned a = k ? cur ^ 1u : cur, b = a ^ 1u;
                for (i = 0; i < cnt[a]; i++) {
                    for (j = 0; j < cnt[b]; j++) if (hq[a][i] == hq[b][j] && hr[a][i] == hr[b][j]) break;
                    if (j == cnt[b]) { changed++; if (ho[a][i] > worst) worst = ho[a][i]; }
                }
            }
        }
        CHECK(changed > 20u && worst < 0.75);                                              /* the band steps a whole pixel: a quarter of the 4 pixel ramp (2.05 px before the bottom had its own 0) */
        printf("the band's bottom edge (eye 9 m, the cut at 25 m, the camera pitching): %u cells came or went, the largest with an offset of %.2f px\n", changed, worst);
    }

    /* an anchor no screen holds: no band */
    CHECK(!sh4der_hexheat_band(NAN, 0.0f, &x0, &y0) && !sh4der_hexheat_band(0.0f, 1e9f, &x0, &y0));

    printf(fails ? "hexheat_test: %d FAILED\n" : "hexheat_test: ok\n", fails);
    return fails != 0;
}
