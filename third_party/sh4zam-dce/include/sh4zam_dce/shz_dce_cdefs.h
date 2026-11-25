/* SH4ZAM-DCE: shared definitions

   shz_dce_cdefs.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
   NOT affiliated with, endorsed by or maintained by Falco Girgis or the SH4ZAM
   project. Report problems with this library to SH4DER-PVR, not to them.
*/

/** \file    shz_dce_cdefs.h
    \brief   What every SH4ZAM-DCE header shares: the SH4ZAM include, the version, the
             alignment checks.

    SH4ZAM-DCE is a sister include of SH4ZAM: batch routines and reworked single routines for the
    Dreamcast's SH-4, header only, built on SH4ZAM's types and on its two back ends. Compiled for
    the SH-4 in single-precision mode (SH4ZAM's SHZ_BACKEND is SHZ_SH4) a routine uses FTRV, FIPR
    and FSRRA; anywhere else it is plain C on SH4ZAM's software back end, with the same results
    within the bound its comment states. Nothing here copies a SH4ZAM routine: link SH4ZAM as
    its own library and keep its headers on the include path.

    Every routine's comment says four things:
    - Inputs, and what it leaves behind (XMTRX, for the routines that load it).
    - Alignment. The SH-4 stops on a 16-bit load from an odd address, a 32-bit load off a multiple
      of 4, a 64-bit load off a multiple of 8 (an address error; an emulator carries on). Every
      pointer here states the multiple it needs, and a debug build asserts it.
    - Error bound, for a routine that is approximate (FTRV, FIPR and FSRRA are: see below).
    - Bit-identical: whether the result has the same bits as the scalar expression it replaces,
      on the same machine. Code that must reproduce a result bit for bit takes only the routines
      that say yes.

    FTRV and FIPR are approximate on the SH-4. The manual's bound for one inner product of four
    terms is: maximum error = (largest of the four products) x 2^-23 + (the result) x 2^-23, for
    normalised operands (SH-4 manual, geometric operation instructions). They are also not the
    sum a compiler makes of four multiplies and three adds, so their last bits differ from the
    scalar form, and the manual does not promise the same bits on another SH core. FSRRA is an
    approximation too. The software back end is plain float arithmetic, inside the same bound.

    A cycle count in a comment is an ESTIMATE (E) from a model of the SH7750 pipeline run on the
    compiled code (tests/sh4run), every memory access a cache hit: for comparing two routines,
    not a measurement on a console.

    Version: v0.10
*/

#ifndef SHZ_DCE_CDEFS_H
#define SHZ_DCE_CDEFS_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <sh4zam/shz_sh4zam.h>

#define SHZ_DCE_VERSION_MAJOR 0    /**< \brief SH4ZAM-DCE major version. */
#define SHZ_DCE_VERSION_MINOR 10   /**< \brief SH4ZAM-DCE minor version. */
#define SHZ_DCE_VERSION_PATCH 0    /**< \brief SH4ZAM-DCE patch version. */

#if SHZ_VERSION < SHZ_VERSION_INIT(0, 10, 0)
#error "SH4ZAM-DCE needs SH4ZAM 0.10.0 or later"
#endif

/* SH4ZAM's master after the 0.10.0 tag still calls itself 0.10.0, so the version cannot tell the two apart. SHZ_ATAN2F_TERMS
   arrived with the arcsine, arccosine and arctangent of commit cff97e8 (#92, 2026-11-18); shz_opaquef() arrived with 3cdff11
   (2026-11-20). This layer wraps both and is tested against master ff39547 (2026-11-22). */
#ifndef SHZ_ATAN2F_TERMS
#error "SH4ZAM-DCE needs SH4ZAM master at 3cdff11 (2026-11-20) or later, not the 0.10.0 tag: git -C <sh4zam> pull"
#endif

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "SH4ZAM-DCE reads packed data as little-endian words (the Dreamcast's order)"
#endif

/** \brief 1 when the SH-4 paths (FTRV, FIPR, FSRRA) are compiled, 0 on the software back end. */
#if SHZ_BACKEND == SHZ_SH4
#define SHZ_DCE_SH4 1
#else
#define SHZ_DCE_SH4 0
#endif

/** \brief A value barrier: SH4ZAM's shz_opaquef() (Falco Girgis), called underneath. On the SH-4
    the value passes through an asm statement with an empty template (no instruction is emitted),
    on any other target it is the value itself. This layer had the same barrier under this name
    before SH4ZAM had one; the name stays so that code written against it keeps compiling.
    Bit-identical to shz_opaquef: YES (it is that routine; tests/host/wrap_test.c).

    The compiler does not look inside asm, so what comes out is a value it knows nothing about:
    not a constant, not equal to any other variable, not the result of a multiply. Three of its
    passes stop there (GCC 15.2, seen in its dumps; doc/THESIS.md, appendix B):
    - the swap of a constant for an operand that compared equal to it (uncprop, allowed by
      -fno-signed-zeros): `x == 0.0f ? 0.0f : r` returns x on the zero path in a -ffast-math
      build; `x == 0.0f ? shz_dce_opaquef(0.0f) : r` returns a real +0 in every build;
    - the regrouping of a product or sum across it (reassoc, -fassociative-math), and the
      contraction of a product with a following sum;
    - sinking: an asm statement is not moved below a later branch, so whatever feeds the barrier
      is computed BEFORE that branch, also on the side that does not need it.
    Cost: no instruction of its own, but a constant passed through it is really loaded, the path
    that returns it becomes a block of its own, and it can take an instruction out of a delay
    slot. It is not volatile (unused, it is removed with what feeds it), it protects nothing
    inside its parentheses and it does not order instructions. */
SHZ_FORCE_INLINE float shz_dce_opaquef(float value) SHZ_NOEXCEPT {
    return shz_opaquef(value);
}

/** \brief The name shz_dce_opaquef() had before it was public. */
#define shz_dce_opaquef_ shz_dce_opaquef

/* Removed when SH4ZAM master took these jobs over (ff39547, 2026-11-22; doc/THESIS.md, revision 7): the reordered root bodies
   (SHZ_DCE_REWORK_ROOTS), the FIPR length bodies (SHZ_DCE_REWORK_FIPR) and the barrier switches of both and of the arcsine
   (SHZ_DCE_OPAQUE, SHZ_DCE_OPAQUE_SQRT, _INV_SQRT, _MAGNITUDE, _DISTANCE, _NORMALIZE2 / 3 / 4, _ASIN_ACOS). A build that asks for
   a body stops here rather than get SH4ZAM's routine without a word; a barrier switch is ignored (there is nothing for it to
   act on). The last tree with the bodies is 691aa21. */
#if (defined(SHZ_DCE_REWORK_ROOTS) && SHZ_DCE_REWORK_ROOTS) || (defined(SHZ_DCE_REWORK_FIPR) && SHZ_DCE_REWORK_FIPR)
#error "SHZ_DCE_REWORK_ROOTS / SHZ_DCE_REWORK_FIPR: those bodies were removed; SH4ZAM master has the jobs (see shz_dce_cdefs.h)"
#endif

/** \brief True when a pointer is on a multiple of n bytes (n a power of two). */
#define SHZ_DCE_IS_ALIGNED(ptr, n) ((((uintptr_t)(ptr)) & ((uintptr_t)(n) - 1u)) == 0u)

/** \brief A debug build stops here when a pointer is not on the multiple a routine needs: on the
    host, in a test, before a console stops on it without a word. */
#define SHZ_DCE_ASSERT_ALIGNED(ptr, n) assert(SHZ_DCE_IS_ALIGNED((ptr), (n)))

#endif /* SHZ_DCE_CDEFS_H */
