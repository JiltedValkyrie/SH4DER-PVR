/* SH4DER-PVR: real hardware guards

   sh4der_guard.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_guard.h
    \brief   Two checks for what an emulator forgives and a real Dreamcast does not:
             a triangle with no screen area, and a polygon header that was never compiled.

    Both come from effects drawn on a real console that looked right in an emulator.

    - Zero-area triangles (sh4der_tri_area_ok). The PVR draws a triangle with no
      area (three points on a line, or two the same) as a line of pixels along
      it; an emulator draws nothing. A disc or ring built from a fan or a strip
      has them where the camera looks along a spoke, and a mesh with a centre
      band at radius 0 has them always. Test the projected triangle before it is
      sent and drop the ones with no area. The area is that of the triangle in
      (x / z, y / z), the screen up to a scale: twice the area is the cross
      product of two edges. A triangle with a point at or behind the near plane
      is not judged here (its projection is meaningless): it returns "ok" and
      the caller's clipper cuts it first.
    - An uncompiled polygon header (sh4der_poly_hdr_ok). A header that was never
      filled in is all zeros, and the first word of a header is the command: 0
      is "end of list" to the tile accelerator, in the middle of the list. An
      emulator carries on; a real console hangs. Compile a header (in KallistiOS
      pvr_poly_compile) before the first quad that uses it, a full-screen filter
      quad as much as a model; this test names the header that was missed.

    No allocation, no globals. The triangle test reads three float[3] arrays the
    caller owns (a float array is 4-byte aligned); the header test reads 32-bit
    words through a uint32_t pointer, so the header must be a 4-byte aligned
    array of words (KallistiOS pvr_poly_hdr_t is). The SH-4 does an unaligned
    load as an address error and stops, so no call here casts a byte pointer to a
    wider one. The reciprocals are SH4ZAM (shz_invf); define
    SH4DER_CORE_NO_SH4ZAM for the plain C path.

    Version: v0.10
    The effect: nothing on screen. It keeps stray lines and a stopped console
      out of a client's own triangles and quads.
    Developed for: Dreamcast clients that submit their own triangles and quads.
    Use it for: any fan, ring, strip or full-screen filter you build yourself.

    \author Jilted Valkyrie
*/

#ifndef SH4DER_GUARD_H
#define SH4DER_GUARD_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_GUARD_MIN_AREA2   1.0e-7f   /**< \brief Twice the (x / z, y / z) area below which a triangle is dropped: about 0.01 square pixel at 640 x 480 with a 90 degree field: only a triangle with no area at all. */
/** @} */

/** \brief 1: send this triangle; 0: it has no screen area (or a non-finite point), drop it.
    a, b, c are the three points in view space: x right, y up or down, z the depth along the view,
    positive in front. near_z is the near plane's depth (> 0). A point with z below near_z, or a
    near_z that is not positive, returns 1: it is not judged here, clip it first. min_area2 is
    twice the area in (x / z, y / z) below which the triangle is dropped (SH4DER_GUARD_MIN_AREA2). */
static inline int sh4der_tri_area_ok(const float a[3], const float b[3], const float c[3], float near_z, float min_area2) {
    int k;

    for (k = 0; k < 3; k++)
        if (!sh4der_finite_(a[k]) || !sh4der_finite_(b[k]) || !sh4der_finite_(c[k]))
            return 0;
    if (!(near_z > 0.0f) || a[2] < near_z || b[2] < near_z || c[2] < near_z)
        return 1;
    {
#ifndef SH4DER_CORE_NO_SH4ZAM
        const float ia = shz_invf(a[2]), ib = shz_invf(b[2]), ic = shz_invf(c[2]);
#else
        const float ia = 1.0f / a[2], ib = 1.0f / b[2], ic = 1.0f / c[2];
#endif
        const float ex = b[0] * ib - a[0] * ia, ey = b[1] * ib - a[1] * ia;
        const float fx = c[0] * ic - a[0] * ia, fy = c[1] * ic - a[1] * ia;
#ifndef SH4DER_CORE_NO_SH4ZAM
        return shz_fabsf(ex * fy - ey * fx) >= min_area2;
#else
        return fabsf(ex * fy - ey * fx) >= min_area2;
#endif
    }
}

/** \brief 1: this polygon header was compiled; 0: it is all zeros or its command word is not a
    polygon or sprite header, and sending it would put "end of list" (or junk) in the tile
    accelerator's list. hdr is the header's eight 32-bit words (KallistiOS pvr_poly_hdr_t,
    4-byte aligned). The command word is the first: its top three bits are the parameter type,
    4 polygon / modifier volume header, 5 sprite header. A debug assert at the point of use. */
static inline int sh4der_poly_hdr_ok(const uint32_t hdr[8]) {
    const uint32_t type = hdr[0] >> 29;

    return type == 4u || type == 5u;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_GUARD_H */
