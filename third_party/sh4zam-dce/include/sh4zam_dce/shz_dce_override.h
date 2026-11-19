/* SH4ZAM-DCE: SH4ZAM's names on the reworked routines

   shz_dce_override.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_override.h
    \brief   One include that makes existing calls of SH4ZAM routines compile to their reworks,
             one compile-time switch a group of routines.

    Every rework in SH4ZAM-DCE keeps the signature of the SH4ZAM routine it replaces. This header
    defines the SH4ZAM name as a macro for the rework, so a source file that includes it needs no
    other change:

        #define SHZ_DCE_USE_INV_SQRT 1                (a switch, before the include)
        #include <sh4zam_dce/shz_dce_override.h>      (it includes SH4ZAM itself first)
        ...
        s = shz_inv_sqrtf(x);                         (compiles to shz_dce_inv_sqrtf(x))

    The switches, each 1 or 0, set before the include (on a compiler's command line, say):

        SHZ_DCE_USE_ASIN_ACOS   default 1   shz_asinf, shz_acosf (shz_dce_trig.h). Results DIFFER from
                                            SH4ZAM's: within 8e-6 rad of the true angle instead of
                                            4.95e-3, and +-pi/2, 0 or pi past +-1 instead of NaN.
        SHZ_DCE_USE_ATAN2       default 1   shz_atan2f (shz_dce_atan2.h). Results DIFFER from
                                            SH4ZAM's: within 1.4e-5 rad of the true angle instead
                                            of 1.02e-2, and it COSTS cycles: about 8 a call in the
                                            model (the trade is SHZ_DCE_ATAN2_TERMS, default 5).
        SHZ_DCE_USE_SQRT        default 0   shz_sqrtf, shz_hypotf (shz_dce_sqrt.h; SH4ZAM's bits)
        SHZ_DCE_USE_INV_SQRT    default 0   shz_inv_sqrtf
        SHZ_DCE_USE_MAGNITUDE   default 0   shz_vec2/3/4_magnitude, shz_vec2/3/4_magnitude_inv
        SHZ_DCE_USE_DISTANCE    default 0   shz_vec2/3/4_distance
        SHZ_DCE_USE_NORMALIZE   default 0   shz_vec2/3/4_normalize_safe
        SHZ_DCE_USE_FIPR        default 0   shz_vec4_normalize, and shz_vec3_magnitude becomes the
                                            FIPR length (shz_dce_vector.h). Results differ from
                                            SH4ZAM's: within 3 and 2 ulp.
        SHZ_DCE_OVERRIDE_ALL    default 0   1: every switch above that is not set by name is on.

    Why these defaults: a console (single calls, reported by Falco Girgis; doc/THESIS.md, section
    7). The arcsine and arccosine won there, in accuracy by a factor of 850 and in cycles by
    nothing or a little. The arctangent won in use: with 5 terms an interpolated rotation that
    jittered with SH4ZAM's routine is steady, for "a few extra cycles" (doc/THESIS.md, 2.5); it
    is the one default here that is slower than what it replaces, so a caller that only sorts
    by angle sets SHZ_DCE_USE_ATAN2 0. The reordered roots, lengths and normalisations lost a
    cycle or two on single calls against a pipeline model that had promised a gain; they are
    here to be switched on a group at a time and measured in a real program. A second set of
    switches, SHZ_DCE_OPAQUE_... (shz_dce_cdefs.h), says where those routines put a barrier on
    their zero.

    Before these switches existed this header renamed the root family always and the arcsine and
    arccosine only with SHZ_DCE_OVERRIDE_APPROX 1. That name is still read: 1 turns
    SHZ_DCE_USE_ASIN_ACOS, SHZ_DCE_USE_ATAN2 and SHZ_DCE_USE_FIPR on, 0 turns them off, unless
    they are set by name.

    Scope: the macros act on the code that follows them. SH4ZAM's own inline routines were
    compiled before them and keep calling SH4ZAM's originals; that is why the lengths, distances
    and safe normalisations have reworks and lines of their own here. To take one routine back,
    `#undef` its name after the include. The original stays reachable in any file that does not
    include this header.

    Version: v0.10
*/

#ifndef SHZ_DCE_OVERRIDE_H
#define SHZ_DCE_OVERRIDE_H

#include "shz_dce_sqrt.h"
#include "shz_dce_trig.h"
#include "shz_dce_atan2.h"
#include "shz_dce_vector.h"

#ifndef SHZ_DCE_OVERRIDE_ALL
#define SHZ_DCE_OVERRIDE_ALL 0
#endif

#ifndef SHZ_DCE_USE_ASIN_ACOS
#ifdef SHZ_DCE_OVERRIDE_APPROX
#define SHZ_DCE_USE_ASIN_ACOS ((SHZ_DCE_OVERRIDE_APPROX) || (SHZ_DCE_OVERRIDE_ALL))
#else
#define SHZ_DCE_USE_ASIN_ACOS 1
#endif
#endif
#ifndef SHZ_DCE_USE_ATAN2
#ifdef SHZ_DCE_OVERRIDE_APPROX
#define SHZ_DCE_USE_ATAN2 ((SHZ_DCE_OVERRIDE_APPROX) || (SHZ_DCE_OVERRIDE_ALL))
#else
#define SHZ_DCE_USE_ATAN2 1
#endif
#endif
#ifndef SHZ_DCE_USE_SQRT
#define SHZ_DCE_USE_SQRT (SHZ_DCE_OVERRIDE_ALL)
#endif
#ifndef SHZ_DCE_USE_INV_SQRT
#define SHZ_DCE_USE_INV_SQRT (SHZ_DCE_OVERRIDE_ALL)
#endif
#ifndef SHZ_DCE_USE_MAGNITUDE
#define SHZ_DCE_USE_MAGNITUDE (SHZ_DCE_OVERRIDE_ALL)
#endif
#ifndef SHZ_DCE_USE_DISTANCE
#define SHZ_DCE_USE_DISTANCE (SHZ_DCE_OVERRIDE_ALL)
#endif
#ifndef SHZ_DCE_USE_NORMALIZE
#define SHZ_DCE_USE_NORMALIZE (SHZ_DCE_OVERRIDE_ALL)
#endif
#ifndef SHZ_DCE_USE_FIPR
#ifdef SHZ_DCE_OVERRIDE_APPROX
#define SHZ_DCE_USE_FIPR ((SHZ_DCE_OVERRIDE_APPROX) || (SHZ_DCE_OVERRIDE_ALL))
#else
#define SHZ_DCE_USE_FIPR (SHZ_DCE_OVERRIDE_ALL)
#endif
#endif

#if SHZ_DCE_USE_ASIN_ACOS
#define shz_asinf                shz_dce_asinf
#define shz_acosf                shz_dce_acosf
#endif

#if SHZ_DCE_USE_ATAN2
#define shz_atan2f               shz_dce_atan2f
#endif

#if SHZ_DCE_USE_SQRT
#define shz_sqrtf                shz_dce_sqrtf
#define shz_hypotf               shz_dce_hypotf
#endif

#if SHZ_DCE_USE_INV_SQRT
#define shz_inv_sqrtf            shz_dce_inv_sqrtf
#endif

#if SHZ_DCE_USE_MAGNITUDE
#define shz_vec2_magnitude       shz_dce_vec2_magnitude
#define shz_vec3_magnitude       shz_dce_vec3_magnitude
#define shz_vec4_magnitude       shz_dce_vec4_magnitude
#define shz_vec2_magnitude_inv   shz_dce_vec2_magnitude_inv
#define shz_vec3_magnitude_inv   shz_dce_vec3_magnitude_inv
#define shz_vec4_magnitude_inv   shz_dce_vec4_magnitude_inv
#endif

#if SHZ_DCE_USE_DISTANCE
#define shz_vec2_distance        shz_dce_vec2_distance
#define shz_vec3_distance        shz_dce_vec3_distance
#define shz_vec4_distance        shz_dce_vec4_distance
#endif

#if SHZ_DCE_USE_NORMALIZE
#define shz_vec2_normalize_safe  shz_dce_vec2_normalize_safe
#define shz_vec3_normalize_safe  shz_dce_vec3_normalize_safe
#define shz_vec4_normalize_safe  shz_dce_vec4_normalize_safe
#endif

/* the FIPR forms: their results differ from SH4ZAM's in the last bits */
#if SHZ_DCE_USE_FIPR
#define shz_vec4_normalize       shz_dce_vec4_normalize
#undef shz_vec3_magnitude
#define shz_vec3_magnitude       shz_dce_vec3_magnitude_fipr
#endif

#endif /* SHZ_DCE_OVERRIDE_H */
