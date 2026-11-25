/* The new map's loop: a place by (s, lat), and the traffic. SH4ZAM maths (shz_sincosf, shz_invf, shz_fmaf, shz_lerpf);
 * hand arithmetic is scalar a * b + c only. */
#include "map.h"
#include "map_gen.h"
#include "sh4der_core.h"

#define MAP_PI  3.14159265f
#define MAP_TAU 6.28318531f

static float s_traffic[MAP_CARS];

float map_wrap(float s) {
    while (s >= MAP_LOOP_LEN) s -= MAP_LOOP_LEN;
    while (s < 0.0f) s += MAP_LOOP_LEN;
    return s;
}

/* The path segment that holds s: map_path[k][0] <= s < map_path[k + 1][0]. Binary search: 8 steps for 191 points. */
static unsigned seg_of(float s) {
    unsigned lo = 0u, hi = MAP_PATH_N - 1u;
    while (hi - lo > 1u) {
        const unsigned mid = (lo + hi) >> 1;
        if (map_path[mid][0] <= s) lo = mid; else hi = mid;
    }
    return lo;
}

map_pose_t map_at(float s, float lat) {
    const unsigned k = seg_of(map_wrap(s));
    const float *a = map_path[k], *b = map_path[k + 1u];
    const float t = shz_saturatef((map_wrap(s) - a[0]) * shz_invf(b[0] - a[0]));
    float dy = b[3] - a[3];
    map_pose_t p;
    shz_sincos_t sc;
    if (dy > MAP_PI) dy -= MAP_TAU;                   /* the short way round */
    if (dy < -MAP_PI) dy += MAP_TAU;
    p.yaw = shz_fmaf(dy, t, a[3]);
    sc = shz_sincosf(p.yaw);
    p.x = shz_fmaf(sc.cos, lat, shz_lerpf(a[1], b[1], t));
    p.z = shz_fmaf(-sc.sin, lat, shz_lerpf(a[2], b[2], t));
    return p;
}

void map_traffic_reset(void) {
    unsigned k;
    for (k = 0; k < MAP_CARS; k++) s_traffic[k] = map_car[k].s0;
}

void map_traffic_step(float dt) {
    unsigned k;
    for (k = 0; k < MAP_CARS; k++) s_traffic[k] = map_wrap(shz_fmaf(map_lane[map_car[k].lane][1], dt, s_traffic[k]));
}

unsigned map_traffic_n(void) { return MAP_CARS; }

map_pose_t map_traffic_pose(unsigned k) {
    const float *l = map_lane[map_car[k].lane];
    map_pose_t p = map_at(s_traffic[k], l[0]);
    if (l[1] < 0.0f) p.yaw += MAP_PI;
    return p;
}
