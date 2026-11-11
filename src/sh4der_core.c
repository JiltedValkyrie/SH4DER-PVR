/* SH4DER-PVR

   sh4der_core.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE.

   The shared non-inline parts: the breath tracker, the glint and the colour ramp.
*/

#include "sh4der_core.h"

/* Signed distance of the chest from origin along forward: the dot product of (chest - origin) and forward. Maths only: no PVR calls. */
float sh4der_breath_raw(const float chest[3], const float origin[3], const float forward[3]) {
    return (chest[0] - origin[0]) * forward[0] + (chest[1] - origin[1]) * forward[1] + (chest[2] - origin[2]) * forward[2];
}

/* Empties the breath window. The next push starts a new window. */
void sh4der_breath_reset(sh4der_breath_t *b) {
    b->head = b->count = 0;
}

/* Adds one raw sample to the breath window, a ring of SH4DER_BREATH_WINDOW samples. Returns where the sample sits in the window's
   low..high range: 0 at the low end, 1 at the high end, clamped to 0..1. A non-finite sample returns 0.5 and is not stored. The span
   is never below SH4DER_BREATH_SPAN, so a still chest stays at 0.5. Maths only. */
float sh4der_breath_push(sh4der_breath_t *b, float raw) {
    float lo, hi, span;
    unsigned i;

    if (!sh4der_finite_(raw))
        return 0.5f;
    if (b->head >= SH4DER_BREATH_WINDOW || b->count > SH4DER_BREATH_WINDOW)
        b->head = b->count = 0;   /* head or count out of range: start the window again */
    b->ring[b->head] = raw;
    b->head = (uint16_t)(b->head + 1 == SH4DER_BREATH_WINDOW ? 0 : b->head + 1);
    if (b->count < SH4DER_BREATH_WINDOW)
        b->count++;
    lo = hi = raw;
    for (i = 0; i < b->count; i++) {
        float v = b->ring[i];

        if (v < lo)
            lo = v;
        if (v > hi)
            hi = v;
    }
    span = hi - lo;
    if (span < SH4DER_BREATH_SPAN)
        span = SH4DER_BREATH_SPAN;
    return sh4der_sat_(sh4der_div_(raw - 0.5f * (lo + hi), span) + 0.5f);
}

/* Glint brightness for a light at distance depth, scaled by fade (both clamped to 0..1 where used). It ramps linearly from 0 at
   SH4DER_GLINT_FAR to 1 at SH4DER_GLINT_NEAR, times SH4DER_GLINT_BRIGHTNESS. Returns 0 for depth <= 0 or non-finite input. Maths only. */
float sh4der_glint(float depth, float fade) {
    if (!sh4der_finite_(depth) || !sh4der_finite_(fade) || depth <= 0.0f)
        return 0.0f;
    return SH4DER_GLINT_BRIGHTNESS * sh4der_sat_((depth - SH4DER_GLINT_FAR) * (1.0f / (SH4DER_GLINT_NEAR - SH4DER_GLINT_FAR))) * sh4der_sat_(fade);
}

/* Hue kept, value 1: in HSV a channel is V (1 - S g) with g fixed by the hue, so scaling S by f is c' = 1 - f (1 - c / V), f capped at 1 / S. */
/* Maths only. Reads rgb and v (the channel maximum), writes the result to out. */
static void saturate_by(const float rgb[3], float v, float s, float f, float out[3]) {
    int i;

    if (s * f > 1.0f)
        f = sh4der_recip_pos_(s);
    for (i = 0; i < 3; i++)
        out[i] = sh4der_sat_(1.0f - f * (1.0f - sh4der_div_(rgb[i], v)));
}

/* Edge and core colours for a glowing ramp from the average colour avg (0..1 per channel; a non-finite channel reads as 0). edge and
   core use SH4DER_RAMP_EDGE_SAT and SH4DER_RAMP_CORE_SAT. white (0..1) lifts both toward white. Maths only: the caller draws with them. */
void sh4der_ramp_colours(const float avg[3], float white, float edge[3], float core[3]) {
    float rgb[3], hi, lo, s;
    int i;

    for (i = 0; i < 3; i++)
        rgb[i] = sh4der_finite_(avg[i]) ? sh4der_sat_(avg[i]) : 0.0f;
    hi = rgb[0] > rgb[1] ? (rgb[0] > rgb[2] ? rgb[0] : rgb[2]) : (rgb[1] > rgb[2] ? rgb[1] : rgb[2]);
    lo = rgb[0] < rgb[1] ? (rgb[0] < rgb[2] ? rgb[0] : rgb[2]) : (rgb[1] < rgb[2] ? rgb[1] : rgb[2]);
    if (hi <= 0.0f) {   /* black input: use cyan (0, 1, 1) */
        rgb[0] = 0.0f;
        rgb[1] = rgb[2] = hi = 1.0f;
        lo = 0.0f;
    }
    s = (hi - lo) * sh4der_recip_pos_(hi);
    saturate_by(rgb, hi, s, SH4DER_RAMP_EDGE_SAT, edge);
    saturate_by(rgb, hi, s, SH4DER_RAMP_CORE_SAT, core);
    white = sh4der_finite_(white) ? sh4der_sat_(white) : 0.0f;
    for (i = 0; i < 3; i++) {
        edge[i] = sh4der_lerp_(edge[i], 1.0f, white);
        core[i] = sh4der_lerp_(core[i], 1.0f, white);
    }
}
