/* SH4DER-PVR

   test/guard_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the real hardware guards: a triangle with no screen area is
   dropped (three points on a line, two the same, all three the same, three
   points on one ray from the eye), a tiny but real one and a normal one are
   kept, a point at or behind the near plane is not judged, a non-finite point
   is dropped, the threshold is the caller's; a polygon header of zeros is
   refused and a compiled one (command word 0x80000000 and 0xA0000000 types)
   is accepted.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_guard.h"

#define NEAR 0.0625f

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int ok3(const float a[3], const float b[3], const float c[3]) {
    return sh4der_tri_area_ok(a, b, c, NEAR, SH4DER_GUARD_MIN_AREA2);
}

/* A normal triangle: 1 unit apart at depth 10, a fifth of a screen across. */
static int normal_kept(void) {
    const float a[3] = { 0.0f, 0.0f, 10.0f }, b[3] = { 1.0f, 0.0f, 10.0f }, c[3] = { 0.0f, 1.0f, 10.0f };

    if (!ok3(a, b, c)) return fail("normal triangle dropped");
    if (!ok3(a, c, b)) return fail("normal triangle dropped with the other winding");
    return 0;
}

/* No area: a line, a repeated point, one point three times, a triangle seen edge on along its own plane through the eye. */
static int degenerate_dropped(void) {
    const float a[3] = { 0.0f, 0.0f, 10.0f }, b[3] = { 1.0f, 0.5f, 10.0f }, line[3] = { 2.0f, 1.0f, 10.0f };
    const float far_a[3] = { 1.0f, 2.0f, 4.0f }, far_b[3] = { 2.0f, 4.0f, 8.0f }, far_c[3] = { 3.0f, 6.0f, 12.0f };   /* one ray from the eye: all three project to the same point */

    if (ok3(a, b, line)) return fail("three points on a line kept");
    if (ok3(a, a, b)) return fail("two equal points kept");
    if (ok3(a, a, a)) return fail("one point three times kept");
    if (ok3(far_a, far_b, far_c)) return fail("three points on one ray from the eye kept");
    return 0;
}

/* A thin sliver with real area is kept; one far below the threshold is dropped; the threshold is the caller's. */
static int thin_and_threshold(void) {
    const float a[3] = { 0.0f, 0.0f, 10.0f }, b[3] = { 10.0f, 0.0f, 10.0f }, thin[3] = { 5.0f, 0.001f, 10.0f };   /* twice the area in screen units: 1 * 0.0001 = 1e-4 */
    const float tiny[3] = { 5.0f, 1.0e-6f, 10.0f };                                                               /* 1 * 1e-7 = 1e-7: right at the default, below 2e-7 */

    if (!ok3(a, b, thin)) return fail("thin sliver with area dropped");
    if (sh4der_tri_area_ok(a, b, tiny, NEAR, 2.0e-7f)) return fail("a sliver under the caller's threshold kept");
    if (!sh4der_tri_area_ok(a, b, tiny, NEAR, 1.0e-9f)) return fail("a sliver over the caller's threshold dropped");
    if (!sh4der_tri_area_ok(a, b, a, NEAR, 0.0f)) return fail("min_area2 0 dropped a triangle: nothing is under 0, so it turns the area test off");
    return 0;
}

/* A point at or behind the near plane is not judged: the clipper cuts it. */
static int near_plane_not_judged(void) {
    const float a[3] = { 0.0f, 0.0f, 10.0f }, same[3] = { 0.0f, 0.0f, 10.0f }, at[3] = { 0.0f, 0.0f, NEAR }, behind[3] = { 0.0f, 0.0f, -1.0f };

    if (ok3(a, same, same) != 0) return fail("a degenerate triangle in front was kept");
    if (!ok3(a, same, behind)) return fail("a triangle with a point behind the eye was judged");
    /* exactly at the near plane is in front (z >= near): judged, and degenerate here */
    if (ok3(at, at, at)) return fail("three points at the near plane kept");
    if (!sh4der_tri_area_ok(a, same, same, 0.0f, SH4DER_GUARD_MIN_AREA2)) return fail("a near plane of 0 was judged");
    if (!sh4der_tri_area_ok(a, same, same, -1.0f, SH4DER_GUARD_MIN_AREA2)) return fail("a negative near plane was judged");
    return 0;
}

/* A point that is not a number or not finite never reaches the list. */
static int not_finite_dropped(void) {
    const float a[3] = { 0.0f, 0.0f, 10.0f }, b[3] = { 1.0f, 0.0f, 10.0f }, c[3] = { 0.0f, 1.0f, 10.0f };
    float bad[3] = { 0.0f, 1.0f, 10.0f };

    bad[0] = NAN;
    if (ok3(a, b, bad)) return fail("a NaN x kept");
    bad[0] = 0.0f;
    bad[1] = INFINITY;
    if (ok3(a, b, bad)) return fail("an infinite y kept");
    bad[1] = 1.0f;
    bad[2] = NAN;
    if (ok3(a, b, bad)) return fail("a NaN depth kept");
    if (!ok3(a, b, c)) return fail("the control triangle dropped");
    return 0;
}

/* The header test: zeros are refused, a polygon header and a sprite header are accepted, an end of list and a vertex command are not headers. */
static int header_words(void) {
    uint32_t h[8];

    memset(h, 0, sizeof(h));
    if (sh4der_poly_hdr_ok(h)) return fail("a zeroed header accepted");
    h[0] = 0x80000000u;   /* polygon / modifier volume header, parameter type 4 */
    if (!sh4der_poly_hdr_ok(h)) return fail("a polygon header refused");
    h[0] = 0x80840012u;   /* with flag bits set, as a compiled header has */
    if (!sh4der_poly_hdr_ok(h)) return fail("a compiled polygon header with flags refused");
    h[0] = 0xa0000000u;   /* sprite header, type 5 */
    if (!sh4der_poly_hdr_ok(h)) return fail("a sprite header refused");
    h[0] = 0xe0000000u;   /* a vertex, type 7, is not a header */
    if (sh4der_poly_hdr_ok(h)) return fail("a vertex command accepted as a header");
    h[0] = 0x00ffffffu;   /* end of list with junk in the low bits */
    if (sh4der_poly_hdr_ok(h)) return fail("an end of list command with junk accepted as a header");
    return 0;
}

int main(void) {
    int bad = 0;

    bad |= normal_kept();
    bad |= degenerate_dropped();
    bad |= thin_and_threshold();
    bad |= near_plane_not_judged();
    bad |= not_finite_dropped();
    bad |= header_words();
    if (bad) return 1;
    printf("guard_test: ok\n");
    return 0;
}
