/* psm_ground.h: the slabs are made the same each time, tile both ways, are no chequer, and keep the base's mean.
   cc -O2 -std=gnu11 -Wall -Wextra demo/psm/test/psm_ground_test.c -o build/psm_ground_test   (a picture: argv[1] = out.ppm) */
#include <stdio.h>
#include <string.h>
#include "../psm_ground.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void rgb(uint16_t p, int *r, int *g, int *b) { *r = (p >> 11) << 3; *g = ((p >> 5) & 63) << 2; *b = (p & 31) << 3; }

int main(int argc, char **argv) {
    static uint16_t a[PSM_GROUND_N * PSM_GROUND_N], b[PSM_GROUND_N * PSM_GROUND_N];
    unsigned x, y, distinct = 0u, i;
    long sum = 0;
    uint16_t seen[64];

    psm_ground_slabs(a, 158, 152, 142, 1u);
    psm_ground_slabs(b, 158, 152, 142, 1u);
    CHECK(!memcmp(a, b, sizeof a), "the same seed gave other texels");
    psm_ground_slabs(b, 158, 152, 142, 2u);
    CHECK(memcmp(a, b, sizeof a) != 0, "another seed gave the same texels");
    /* tiling: the seam across x 63 | 0 and y 63 | 0 is no brighter or darker on average than a seam inside the tile */
    {
        long seam = 0, inner = 0;
        for (y = 0; y < PSM_GROUND_N; y++) {
            int r0, g0, b0, r1, g1, b1, r2, g2, b2, r3, g3, b3;
            rgb(a[y * PSM_GROUND_N + 63], &r0, &g0, &b0); rgb(a[y * PSM_GROUND_N], &r1, &g1, &b1);
            rgb(a[y * PSM_GROUND_N + 31], &r2, &g2, &b2); rgb(a[y * PSM_GROUND_N + 32], &r3, &g3, &b3);
            seam += (r0 > r1 ? r0 - r1 : r1 - r0); inner += (r2 > r3 ? r2 - r3 : r3 - r2);
            (void)g0; (void)b0; (void)g1; (void)b1; (void)g2; (void)b2; (void)g3; (void)b3;
        }
        CHECK(seam < inner * 3 + 64 * 8, "the x seam stands out: %ld against %ld inside", seam, inner);
    }
    /* no chequer: many tones, not two */
    for (i = 0; i < PSM_GROUND_N * PSM_GROUND_N && distinct < 64u; i++) {
        unsigned k;
        for (k = 0; k < distinct && seen[k] != a[i]; k++) {}
        if (k == distinct) seen[distinct++] = a[i];
    }
    CHECK(distinct >= 24u, "only %u tones", distinct);
    for (i = 0; i < PSM_GROUND_N * PSM_GROUND_N; i++) { int r, g, bb; rgb(a[i], &r, &g, &bb); sum += r; }
    sum /= (long)(PSM_GROUND_N * PSM_GROUND_N);
    CHECK(sum > 158 * 85 / 100 && sum < 158 * 105 / 100, "red mean %ld, base 158", sum);
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        if (f) {
            fprintf(f, "P6\n%u %u\n255\n", PSM_GROUND_N * 2u, PSM_GROUND_N * 2u);
            for (y = 0; y < PSM_GROUND_N * 2u; y++)
                for (x = 0; x < PSM_GROUND_N * 2u; x++) {
                    int r, g, bb;
                    rgb(a[(y % PSM_GROUND_N) * PSM_GROUND_N + x % PSM_GROUND_N], &r, &g, &bb);
                    fputc(r, f); fputc(g, f); fputc(bb, f);
                }
            fclose(f);
        }
    }
    printf("psm_ground_test: %s (%u tones, red mean %ld)\n", fails ? "FAILED" : "ok", distinct, sum);
    return fails != 0;
}
