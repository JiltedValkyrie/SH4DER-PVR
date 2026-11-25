/* SH4DER-PVR: ray bench, host count of the steps P2's slice takes (not a measurement)

   demo/psm/bench/ray_mean_n.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   P2 (bench_ray.c) times a slice of PK = 1,024 stand-in receivers, ray_points(RAYS, 1024, 7), on the town grid at
   14:00 and at 16:00. The console said the 16:00 slice is the cheaper one. This counts, on the host, the steps each of
   those same 1,024 rays takes (ray_walk_ref, the reference the kernel matches byte for byte), at both suns: the mean
   n, split into rays that end lit and rays that end in shadow, and what P1's fitted line (C_setup + n C_step, the
   console's numbers passed on the command line or the P1.4 fit by default) gives for that mean.

   Build (from the repository's root):
     gcc -std=c11 -O2 -Wall -Wextra -I third_party/sh4zam/include demo/psm/bench/ray_mean_n.c -lm -o build/ray_mean_n
*/

#include <stdio.h>
#include <stdlib.h>

#include "ray_kernels.h"

#define PK 1024u

int main(int argc, char **argv) {
    static const float az[2] = { TOWN_RAY_SUN14_AZ, TOWN_RAY_SUN16_AZ }, el[2] = { TOWN_RAY_SUN14_EL, TOWN_RAY_SUN16_EL };
    static const char *const name[2] = { "14:00", "16:00" };
    static float pts[PK][4];
    const double c_setup = argc > 2 ? atof(argv[1]) : 74.09, c_step = argc > 2 ? atof(argv[2]) : 22.07;
    unsigned sun, r;

    (void)ray_slice; (void)ray_slice_ref; (void)ray_proxy_pass; (void)ray_proxy_pass_ref;
    ray_points(pts, PK, 7u);
    printf("P2's %u stand-ins (ray_points seed 7), steps by ray_walk_ref; line C_setup %.2f + n x C_step %.2f\n", PK, c_setup, c_step);
    for (sun = 0; sun < 2u; sun++) {
        unsigned lit = 0, sum = 0, sum_lit = 0, nmax = 0, hist[8] = { 0 };
        double line = 0.0;
        ray_sun_t s;

        ray_sun_init(&s, az[sun], el[sun]);
        for (r = 0; r < PK; r++) {
            unsigned n;
            const unsigned l = ray_walk_ref(town_ray_h, &s, pts[r][0], pts[r][1], pts[r][2], &n) != 0u;   /* a byte, not 0 / 1 */

            lit += l; sum += n; if (l) sum_lit += n;
            if (n > nmax) nmax = n;
            hist[n < 8u ? 0 : n < 16u ? 1 : n < 24u ? 2 : n < 32u ? 3 : n < 48u ? 4 : n < 64u ? 5 : n < 96u ? 6 : 7]++;
            line += c_setup + c_step * (double)n;
        }
        printf("%s (az %.1f el %.1f): lit %u, shadow %u; mean n %.2f (lit %.2f, shadow %.2f), max %u\n", name[sun],
               (double)az[sun], (double)el[sun], lit, PK - lit, (double)sum / PK, lit ? (double)sum_lit / lit : 0.0,
               PK - lit ? (double)(sum - sum_lit) / (PK - lit) : 0.0, nmax);
        printf("  n in 0-7 / 8-15 / 16-23 / 24-31 / 32-47 / 48-63 / 64-95 / 96+: %u %u %u %u %u %u %u %u\n",
               hist[0], hist[1], hist[2], hist[3], hist[4], hist[5], hist[6], hist[7]);
        printf("  the line over these rays: %.2f cycles a ray (mean of C_setup + n C_step)\n", line / PK);
    }
    return 0;
}
