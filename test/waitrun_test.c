/* SH4DER-PVR: test for include/sh4der_waitrun.h (the three-mode run and its statistics; plain C) */
#include <stdio.h>
#include "sh4der_waitrun.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    static sh4der_waitrun_t w;
    sh4der_waitrun_stats_t s;
    uint64_t t = 1000000u;
    unsigned i, m, x, k;
    int done = 0;

    sh4der_waitrun_init(&w, 2u);
    CHECK(!sh4der_waitrun_frame(&w, t));                         /* the first frame starts the clock */
    CHECK(w.n[0] == 1u && w.us[0][0] == 0u && w.phase == 0u);
    for (i = 1; i < 60u; i++) { t += (i == 30u) ? 25000u : 16667u; CHECK(!sh4der_waitrun_frame(&w, t)); }
    CHECK(w.us[0][30] == 25000u && w.us[0][31] == 16667u);
    sh4der_waitrun_stats(&w, 0, &s);
    CHECK(s.counted == 60u - SH4DER_WAITRUN_SKIP);
    CHECK(s.min_us == 16667u && s.max_us == 25000u && s.slow == 1u);
    CHECK(s.mean_us > 16667u && s.mean_us < 17500u);
    for (k = 0; k < 400u && !done; k++) { t += 16667u; done = sh4der_waitrun_frame(&w, t); }
    CHECK(done && w.phase == SH4DER_WAITRUN_MODES - 1u);         /* the run ended after its 2 s */
    for (k = 0; k < SH4DER_WAITRUN_MODES; k++) CHECK(w.n[k] >= 100u && w.n[k] <= 125u);
    sh4der_waitrun_slice(&w, 0, 10, &m, &x);
    CHECK(m > 0u && x >= m);
    sh4der_waitrun_init(&w, 100u);                               /* a full table moves on to the next mode */
    for (i = 0; i < SH4DER_WAITRUN_MAX; i++) { t += 16667u; if (sh4der_waitrun_frame(&w, t)) break; }
    CHECK(w.n[0] == SH4DER_WAITRUN_MAX && i == SH4DER_WAITRUN_MAX - 1u);
    sh4der_waitrun_init(&w, 5u);                                 /* an empty run has no figures */
    sh4der_waitrun_stats(&w, 0, &s);
    CHECK(s.counted == 0u && s.mean_us == 0u);
    if (fails) { printf("waitrun_test: %d failed\n", fails); return 1; }
    printf("waitrun_test: ok\n");
    return 0;
}
